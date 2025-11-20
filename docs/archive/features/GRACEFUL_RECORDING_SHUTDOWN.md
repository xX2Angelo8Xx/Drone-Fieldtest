# Graceful Recording Shutdown Pattern (v1.5.4)

**Date:** November 18, 2025  
**Purpose:** Ensure recordings are always saved properly, even during unexpected shutdown  
**Critical For:** Battery monitoring, user errors, emergency shutdowns

## Problem Statement

When a recording is active and the system shuts down, we must ensure:
1. ✅ All recorded frames are written to disk
2. ✅ File headers/metadata are finalized
3. ✅ Depth data (if enabled) is completely saved
4. ✅ Filesystem buffers are flushed to USB
5. ✅ No corrupted or incomplete recordings

**Use Cases:**
- User clicks shutdown button while recording (forgets to stop first)
- Battery reaches critical level → automatic shutdown
- Ctrl+C during active recording
- System crash/power loss (hardware-level, can't prevent)

## Implementation

### Shutdown Sequence (handleShutdown)

```cpp
// STEP 3: Stop any active recording with FULL cleanup
if (recording_active_) {
    std::cout << "[WEB_CONTROLLER] Active recording detected - performing graceful stop..." << std::endl;
    updateLCD("Saving", "Recording...");
    
    // Call the FULL stopRecording() method to ensure complete cleanup:
    // - Stops DepthDataWriter (SVO2_DEPTH_INFO mode)
    // - Stops depth visualization thread (SVO2_DEPTH_IMAGES mode)  
    // - Waits for recording monitor thread to finish
    // - Disables depth computation properly
    stopRecording();
    
    // CRITICAL: Brief wait to ensure all file buffers are flushed
    std::cout << "[WEB_CONTROLLER] Waiting for filesystem sync..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sync();  // Force kernel to flush all filesystem buffers
    
    std::cout << "[WEB_CONTROLLER] ✓ Recording saved and stopped gracefully" << std::endl;
} else {
    std::cout << "[WEB_CONTROLLER] No active recording to stop" << std::endl;
}
```

### Why Call stopRecording() Instead of Direct Cleanup?

**OLD CODE (WRONG):**
```cpp
// ❌ Incomplete cleanup in handleShutdown()
if (recording_active_) {
    recording_active_ = false;
    current_state_ = RecorderState::STOPPING;
    
    if (svo_recorder_) {
        svo_recorder_->stopRecording();  // Missing depth data cleanup!
    }
    
    current_state_ = RecorderState::IDLE;
}
// Missing: DepthDataWriter stop, depth viz thread, monitor thread join
```

**NEW CODE (CORRECT):**
```cpp
// ✅ Complete cleanup using stopRecording()
if (recording_active_) {
    stopRecording();  // Handles ALL cleanup paths
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sync();  // Filesystem flush
}
```

### What stopRecording() Does

```cpp
bool DroneWebController::stopRecording() {
    // 1. Update LCD
    updateLCD("Stopping", "Recording...");
    current_state_ = RecorderState::STOPPING;
    
    // 2. Stop DepthDataWriter if running (SVO2_DEPTH_INFO mode)
    if (depth_data_writer_) {
        depth_data_writer_->stop();
        int frame_count = depth_data_writer_->getFrameCount();
        std::cout << "DepthDataWriter stopped. Total frames: " << frame_count << std::endl;
        depth_data_writer_.reset();
    }
    
    // 3. Stop depth visualization thread (SVO2_DEPTH_IMAGES mode)
    if (depth_viz_running_) {
        depth_viz_running_ = false;
        if (depth_viz_thread_ && depth_viz_thread_->joinable()) {
            depth_viz_thread_->join();
        }
    }
    
    // 4. Stop recorder (SVO2 or RAW)
    if (svo_recorder_) {
        svo_recorder_->stopRecording();
        svo_recorder_->enableDepthComputation(false);  // Reset for next session
    } else if (raw_recorder_) {
        raw_recorder_->stopRecording();
    }
    
    // 5. Signal recording monitor thread to stop
    recording_active_ = false;
    
    // 6. Wait for recording monitor thread to finish
    if (recording_monitor_thread_ && recording_monitor_thread_->joinable()) {
        recording_monitor_thread_->join();
        recording_monitor_thread_.reset();
    }
    
    // 7. Update state and LCD
    current_state_ = RecorderState::IDLE;
    updateLCD("Recording", "Stopped");
    recording_stopped_time_ = std::chrono::steady_clock::now();
    
    return true;
}
```

## Filesystem Sync Explanation

### Why `sync()` + 500ms delay?

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(500));
sync();  // Force kernel to flush all filesystem buffers
```

**What sync() does:**
- Forces Linux kernel to write all dirty filesystem buffers to disk
- Returns immediately (doesn't wait for completion)
- Critical for USB drives (may have write caching)

**Why 500ms delay:**
- ZED SDK's `stopRecording()` may still be flushing internal buffers
- USB controllers need time to complete DMA transfers
- Large SVO2 files (several GB) need time to finalize headers
- 500ms is empirically safe without noticeable delay to user

**Alternative (synchronous):**
```cpp
// Could use syncfs() if we had file descriptor
// syncfs(fd);  // Blocks until specific filesystem is synced

// Or fsync() for specific file
// fsync(file_fd);  // Blocks until file is written
```

We use `sync()` because:
1. ZED SDK handles file I/O internally (we don't have file descriptor)
2. We want to flush ALL filesystems (USB, temp files, logs)
3. Non-blocking is fine with 500ms buffer time

## Integration with Battery Monitoring (INA219)

### Future Battery Monitor Pattern

```cpp
// In battery_monitor.cpp (future implementation)
void BatteryMonitor::monitoringLoop() {
    while (monitoring_active_) {
        float voltage = ina219_->getBusVoltage();
        float current = ina219_->getCurrent();
        
        // Critical battery level: 10.5V (3S LiPo minimum safe voltage)
        if (voltage < 10.5f) {
            std::cout << "[BATTERY] CRITICAL: " << voltage << "V - Initiating emergency shutdown" << std::endl;
            
            // Trigger graceful shutdown
            if (controller_) {
                controller_->shutdown_requested_ = true;
                // OR: controller_->shutdownSystem();
            }
            
            // The shutdown sequence will:
            // 1. Stop active recording (if any) - saves all data
            // 2. Close cameras
            // 3. Tear down WiFi
            // 4. Execute system shutdown
            // All recordings are saved safely!
            
            break;  // Exit monitoring loop
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
```

### Why This Works

1. **Battery monitor detects critical voltage**
2. **Sets `shutdown_requested_ = true`**
3. **Main loop exits**
4. **Destructor calls `handleShutdown()`**
5. **handleShutdown() calls `stopRecording()`** ← Saves recording!
6. **Filesystem sync ensures USB write completion**
7. **System shuts down cleanly**

No data loss, even with sudden battery depletion!

## Testing Scenarios

### Test 1: Shutdown During Active Recording
```bash
# Start system
sudo systemctl start drone-recorder

# Connect to GUI: http://192.168.4.1:8080
# Start recording (any mode)
# Wait 30 seconds (recording active)
# Click "System Herunterfahren" button

# Expected:
# ✓ LCD shows "Saving Recording..."
# ✓ Recording stops gracefully
# ✓ File saved completely to USB
# ✓ System shuts down after sync
# ✓ No corrupted recordings
```

### Test 2: Ctrl+C During Recording
```bash
# Start manually
sudo ./build/apps/drone_web_controller/drone_web_controller

# Start recording via GUI
# Press Ctrl+C in terminal

# Expected:
# ✓ Signal handler sets shutdown_requested_ = true
# ✓ Main loop exits
# ✓ Destructor calls handleShutdown()
# ✓ Recording saved via stopRecording()
# ✓ Clean exit
```

### Test 3: Systemctl Stop During Recording
```bash
# Start via systemd
sudo systemctl start drone-recorder

# Start recording via GUI
# Stop service
sudo systemctl stop drone-recorder

# Expected:
# ✓ SIGTERM signal received
# ✓ Same graceful shutdown as Ctrl+C
# ✓ Recording saved
```

### Test 4: Verify Saved Recording
```bash
# After any shutdown test above:
cd /media/angelo/DRONE_DATA/

# Find most recent flight directory
ls -lrt | tail -1

# Check files exist and are non-zero size
cd flight_YYYYMMDD_HHMMSS/
ls -lh
# Should see: video.svo2 (or frames), sensor_data.csv, recording.log

# Verify SVO2 file is playable
/usr/local/zed/tools/ZED_Explorer video.svo2
# Should play without errors
```

## Monitoring Shutdown in Logs

```bash
# Watch shutdown sequence in real-time
sudo journalctl -u drone-recorder -f

# Look for these messages:
# [WEB_CONTROLLER] Active recording detected - performing graceful stop...
# [LCD] Update: Saving / Recording...
# [WEB_CONTROLLER] Stopping recording...
# [WEB_CONTROLLER] DepthDataWriter stopped. Total frames: XXX
# [ZED] Recording stopped
# [WEB_CONTROLLER] Waiting for filesystem sync...
# [WEB_CONTROLLER] ✓ Recording saved and stopped gracefully
# [ZED] Closing camera explicitly...
# [WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓
```

## Key Principles

1. **Always use stopRecording()** - Never duplicate cleanup logic
2. **Filesystem sync is mandatory** - USB drives need explicit flush
3. **Wait for threads** - Recording monitor must finish before camera close
4. **Update LCD** - User feedback during shutdown
5. **Log everything** - Helps debug shutdown issues

## Known Limitations

### What We CAN Handle
- ✅ User forgets to stop recording before shutdown
- ✅ Battery monitoring triggers emergency shutdown
- ✅ Signal-based shutdown (Ctrl+C, systemctl stop)
- ✅ Web GUI shutdown button during recording

### What We CANNOT Handle
- ❌ Immediate power loss (plug pulled, battery disconnected)
- ❌ Kernel panic / system crash
- ❌ USB drive physically removed during recording
- ❌ Out of memory (OOM) killer

**Mitigation for power loss:**
- Use journaling filesystem (ext4, NTFS, exFAT) - NOT FAT32
- Consider battery with undervoltage protection
- Implement periodic checkpoint saves (future feature)

## Related Documentation

- `docs/SHUTDOWN_DEADLOCK_FIX_v1.5.4.md` - Shutdown thread safety
- `RELEASE_v1.5.2_STABLE.md` - Unified stop recording routine
- `.github/copilot-instructions.md` - Critical patterns (Section 2)

## Future Enhancements

1. **Periodic checkpoint saves** - Write SVO2 index every 30 seconds
2. **Recording resume on restart** - Continue after power loss
3. **Battery low warning** - Alert at 11.5V before emergency shutdown
4. **Graceful shutdown timeout** - Force kill after 10 seconds if hung

---

**Status:** ✅ IMPLEMENTED - Recording shutdown is now graceful and safe  
**Next Step:** Integrate INA219 battery monitoring with this shutdown pattern
