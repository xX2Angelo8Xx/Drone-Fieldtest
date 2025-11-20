# Graceful Recording Shutdown - Implementation Summary

**Date:** November 18, 2025  
**Version:** v1.5.4 (continued)  
**Feature:** Graceful recording save during shutdown

## What Was Changed

### Problem
The system's `handleShutdown()` function was performing **incomplete cleanup** when stopping active recordings during shutdown:

**Missing cleanup steps:**
- ❌ DepthDataWriter not stopped (SVO2_DEPTH_INFO mode)
- ❌ Depth visualization thread not stopped (SVO2_DEPTH_IMAGES mode)
- ❌ Recording monitor thread not joined
- ❌ Depth computation not disabled properly
- ❌ No filesystem sync to ensure USB write completion
- ❌ Missing "Recording Stopped" LCD message

**Why this matters:**
1. User forgets to stop recording before shutdown → corrupted file
2. Battery reaches critical level → recording lost
3. Ctrl+C during recording → incomplete data
4. Thread cleanup issues → potential deadlock

### Solution

**Call the complete `stopRecording()` method** instead of duplicating partial cleanup logic:

```cpp
// OLD CODE (apps/drone_web_controller/drone_web_controller.cpp:2168-2184)
if (recording_active_) {
    std::cout << "[WEB_CONTROLLER] Stopping active recording..." << std::endl;
    recording_active_ = false;
    current_state_ = RecorderState::STOPPING;
    
    // Stop appropriate recorder
    if ((recording_mode_ == RecordingModeType::SVO2 || 
         recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) && svo_recorder_) {
        svo_recorder_->stopRecording();
    } else if (recording_mode_ == RecordingModeType::RAW_FRAMES && raw_recorder_) {
        raw_recorder_->stopRecording();
    }
    
    current_state_ = RecorderState::IDLE;
    std::cout << "[WEB_CONTROLLER] ✓ Recording stopped" << std::endl;
}
```

```cpp
// NEW CODE (apps/drone_web_controller/drone_web_controller.cpp:2168-2193)
if (recording_active_) {
    std::cout << "[WEB_CONTROLLER] Active recording detected - performing graceful stop..." << std::endl;
    updateLCD("Saving", "Recording...");
    
    // Call the FULL stopRecording() method to ensure complete cleanup:
    // - Stops DepthDataWriter (SVO2_DEPTH_INFO mode)
    // - Stops depth visualization thread (SVO2_DEPTH_IMAGES mode)  
    // - Waits for recording monitor thread to finish
    // - Disables depth computation properly
    // - Shows "Recording Stopped" message
    stopRecording();
    
    // CRITICAL: Brief wait to ensure all file buffers are flushed to USB
    // Especially important for large SVO2 files (can be several GB)
    std::cout << "[WEB_CONTROLLER] Waiting for filesystem sync..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    sync();  // Force kernel to flush all filesystem buffers
    
    std::cout << "[WEB_CONTROLLER] ✓ Recording saved and stopped gracefully" << std::endl;
} else {
    std::cout << "[WEB_CONTROLLER] No active recording to stop" << std::endl;
}
```

## Benefits

### 1. User Safety
- User can click shutdown without manually stopping recording first
- System automatically saves recording before powering off
- No risk of lost footage due to forgotten stop button

### 2. Battery Monitoring Ready
When we add INA219 battery monitoring (next step):
```cpp
// Future battery monitoring code
if (battery_voltage < 10.5f) {  // Critical level
    controller->shutdown_requested_ = true;
    // Automatic graceful shutdown with recording save!
}
```

### 3. Data Integrity
- All recording modes properly finalized:
  - **SVO2:** Video headers written, file closed
  - **SVO2_DEPTH_INFO:** Depth data files completed
  - **SVO2_DEPTH_IMAGES:** All PNG images saved
  - **RAW_FRAMES:** All frame pairs written
- Filesystem sync ensures USB write completion
- No corrupted or incomplete recordings

### 4. Thread Safety
- Recording monitor thread properly joined
- Depth visualization thread stopped cleanly
- No resource leaks or dangling threads

## Shutdown Sequence

```
1. Shutdown triggered (GUI button / Ctrl+C / Battery low)
   ↓
2. shutdown_requested_ = true
   ↓
3. Main loop exits
   ↓
4. Destructor calls handleShutdown()
   ↓
5. handleShutdown() detects recording_active_ == true
   ↓
6. LCD: "Saving / Recording..."
   ↓
7. Call stopRecording() - COMPLETE cleanup:
   - Stop DepthDataWriter
   - Stop depth viz thread
   - Stop recorder (SVO2/RAW)
   - Join recording monitor thread
   - Update state to IDLE
   - LCD: "Recording / Stopped"
   ↓
8. Wait 500ms + sync() - Flush filesystem buffers
   ↓
9. Close ZED cameras
   ↓
10. Tear down WiFi AP
   ↓
11. Stop system monitor thread
   ↓
12. LCD: "Shutdown / Complete"
   ↓
13. system("sudo shutdown -h now")
```

## Testing

### Test 1: Shutdown During Recording
```bash
# 1. Start system
sudo /home/angelo/Projects/Drone-Fieldtest/scripts/start_drone.sh

# 2. Connect to GUI: http://192.168.4.1:8080

# 3. Start recording (any mode - SVO2, SVO2_DEPTH_INFO, etc.)

# 4. Let it record for ~30 seconds

# 5. Click "System Herunterfahren" button WITHOUT stopping recording

# Expected behavior:
# ✓ LCD shows "Saving / Recording..."
# ✓ Recording stops automatically
# ✓ LCD shows "Recording / Stopped" (3 seconds)
# ✓ Wait 500ms for filesystem sync
# ✓ LCD shows "Shutdown / Complete"
# ✓ System powers off
# ✓ Recording file is complete and playable
```

### Test 2: Verify Recording Saved
```bash
# After shutdown, power on and check USB
cd /media/angelo/DRONE_DATA/

# Find most recent flight
ls -lrt

# Check recording files
cd flight_YYYYMMDD_HHMMSS/
ls -lh

# Expected files:
# video.svo2 (or frame directories)
# sensor_data.csv
# recording.log

# Verify SVO2 is playable
/usr/local/zed/tools/ZED_Explorer video.svo2
# Should play without errors - no corruption
```

### Test 3: Different Recording Modes
Test shutdown with each recording mode to ensure all cleanup paths work:

1. **SVO2 only** - Basic video recording
2. **SVO2_DEPTH_INFO** - With DepthDataWriter
3. **SVO2_DEPTH_IMAGES** - With depth visualization thread
4. **RAW_FRAMES** - With separate frame saving

All should save gracefully without corruption.

## Log Messages to Watch For

```bash
# Monitor shutdown in real-time
tail -f /tmp/drone_controller.log

# Successful graceful shutdown looks like:
[WEB_CONTROLLER] System shutdown requested
[WEB_CONTROLLER] Initiating shutdown sequence...
[WEB_CONTROLLER] Active recording detected - performing graceful stop...
[LCD] Update: Saving / Recording...
[WEB_CONTROLLER] Stopping recording...
[WEB_CONTROLLER] Stopping DepthDataWriter...  # (if SVO2_DEPTH_INFO mode)
[DEPTH_DATA] Stopped. Total frames: 123
[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: 123
[ZED] Recording stopped
[WEB_CONTROLLER] Waiting for filesystem sync...
[WEB_CONTROLLER] ✓ Recording saved and stopped gracefully
[ZED] Closing camera explicitly...
[ZED] ✓ SVO recorder closed
[WEB_CONTROLLER] Tearing down WiFi hotspot...
[WEB_CONTROLLER] ✓ Hotspot teardown complete
[WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓
[MAIN] Executing system shutdown...
```

## Integration with Battery Monitoring (INA219)

This change sets the foundation for automatic battery-low shutdown:

```cpp
// Pseudocode for future INA219 integration
class BatteryMonitor {
    void monitoringLoop() {
        while (monitoring_active_) {
            float voltage = readBatteryVoltage();
            
            // Voltage thresholds for 3S LiPo (11.1V nominal)
            if (voltage < 10.5f) {  // Critical: 3.5V per cell
                std::cout << "[BATTERY] CRITICAL: " << voltage << "V" << std::endl;
                
                // Trigger graceful shutdown
                controller_->shutdown_requested_ = true;
                
                // The system will:
                // 1. Save active recording (if any)
                // 2. Close camera
                // 3. Tear down WiFi
                // 4. Shut down Jetson
                // All data preserved!
                
                monitoring_active_ = false;
            }
            else if (voltage < 11.0f) {  // Warning: 3.66V per cell
                std::cout << "[BATTERY] LOW: " << voltage << "V" << std::endl;
                // Show warning on LCD, don't shut down yet
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};
```

## Files Modified

1. **apps/drone_web_controller/drone_web_controller.cpp**
   - Line 2168-2193: Updated `handleShutdown()` to call `stopRecording()` + sync

## New Documentation

1. **docs/GRACEFUL_RECORDING_SHUTDOWN.md**
   - Complete technical explanation
   - Testing procedures
   - Battery monitoring integration guide
   - Filesystem sync rationale

2. **docs/GRACEFUL_SHUTDOWN_SUMMARY.md** (this file)
   - Quick implementation summary
   - Testing checklist
   - Log message examples

## Known Limitations

### What We Handle
- ✅ User forgets to stop recording before shutdown
- ✅ Battery monitoring emergency shutdown (future)
- ✅ Ctrl+C / SIGTERM during recording
- ✅ Web GUI shutdown during recording
- ✅ All recording modes (SVO2, DEPTH_INFO, DEPTH_IMAGES, RAW_FRAMES)

### What We Cannot Handle
- ❌ Immediate power loss (no software can prevent this)
- ❌ USB drive physically removed during write
- ❌ Kernel panic / system crash
- ❌ Out of memory (OOM) killer

**For power loss protection:**
- Use journaling filesystem (NTFS, exFAT, ext4)
- Consider battery with undervoltage protection circuit
- Future: Implement periodic checkpoint saves

## Next Steps

1. ✅ **Graceful recording shutdown** - COMPLETED
2. 🔄 **INA219 battery monitoring** - NEXT
   - Add I2C communication for INA219
   - Monitor voltage in background thread
   - Trigger shutdown at critical voltage
   - Show battery level on LCD
3. 🔄 **Battery warning system**
   - Display warning at 11.0V
   - Flash LED at critical level
   - Audio beep (if speaker connected)

## Version History

- **v1.5.4** - Shutdown deadlock fix + graceful recording save
- **v1.5.3** - CORRUPTED_FRAME tolerance
- **v1.5.2** - Automatic depth mode management
- **v1.3.4** - Web disconnect fix

---

**Status:** ✅ IMPLEMENTED AND TESTED  
**Ready for:** INA219 battery monitoring integration
