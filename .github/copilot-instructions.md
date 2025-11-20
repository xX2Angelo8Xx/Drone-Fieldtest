# Copilot Instructions for Drone Field Test System

**Platform:** Jetson Orin Nano + ZED 2i stereo camera | **Primary App:** `drone_web_controller` (WiFi AP + Web UI @ http://192.168.4.1:8080)  
**Current Version:** v1.5.3-stable (November 2025)

## 🎓 MANDATORY READING BEFORE ANY CHANGES

Read **`docs/CRITICAL_LEARNINGS_v1.3.md`** first - it documents:
- FAT32 4GB corruption issue → NTFS/exFAT requirement
- NVENC hardware encoder unavailable → LOSSLESS compression only
- Thread deadlock fix (`break` not `continue` in monitor loops)
- LCD thread ownership conflicts and resolution
- Shutter speed ↔ exposure conversion (FPS-dependent)

Other essential docs:
- `RELEASE_v1.5.3_STABLE.md` - Latest stable release notes
- `RELEASE_v1.5.2_STABLE.md` - Automatic resource management features
- `4GB_SOLUTION_FINAL_STATUS.md` - Filesystem validation logic
- `docs/WEB_DISCONNECT_FIX_v1.3.4.md` - Thread interaction patterns
- `docs/SHUTTER_SPEED_UI_v1.3.md` - Camera exposure system

## 🏗️ Architecture Overview

```
apps/
  ├── drone_web_controller/  ⭐ Main: WiFi AP + Web GUI + Recording control
  ├── smart_recorder/         CLI recorder with profiles (standard, training, etc.)
  ├── live_streamer/          H.264/H.265 streaming + telemetry
  ├── performance_test/       Hardware validation
  └── zed_cli_recorder/       Wrapper for ZED Explorer CLI

common/
  ├── hardware/               ZEDRecorder, RawFrameRecorder, I2C_LCD
  ├── storage/                StorageHandler (USB detection, filesystem validation)
  ├── networking/             SafeHotspotManager (NetworkManager-based)
  └── utils/                  Shared utilities

Build: ./scripts/build.sh → build/apps/<app>/<app>
Deploy: systemd/drone-recorder.service (user=angelo, WorkingDirectory=repo root)
```

## ⚠️ CRITICAL PATTERNS (DO NOT BREAK)

### 1. Network Safety (ABSOLUTE PRIORITY - DO NOT VIOLATE)

**CRITICAL:** The WiFi AP setup MUST be reversible. If the program crashes, the Jetson MUST retain internet connectivity via Ethernet. Never modify system-wide network configs that survive process termination.

```cpp
// ✅ CORRECT - SafeHotspotManager pattern (common/networking/)
// Uses NetworkManager profiles that auto-cleanup on connection down
bool setupWiFiHotspot() {
    // 1. Create temporary AP connection via nmcli
    system("nmcli connection add type wifi ifname wlP1p1s0 con-name DroneController "
           "autoconnect yes ssid DroneController");
    system("nmcli connection modify DroneController 802-11-wireless.mode ap "
           "802-11-wireless.band bg ipv4.method shared");
    
    // 2. Activate (not permanent system change!)
    system("nmcli connection up DroneController");
    // If process crashes → connection goes down → internet restored
}

bool teardownWiFiHotspot() {
    // 3. Clean shutdown
    system("nmcli connection down DroneController");
    system("nmcli connection delete DroneController");
    // Ethernet connection automatically becomes primary again
}

// ❌ WRONG - DO NOT modify permanent configs
// systemctl disable NetworkManager
// echo "managed=false" >> /etc/NetworkManager/NetworkManager.conf
// iptables rules without cleanup
// hostapd/dnsmasq as system services (survives crash)
```

**Why This Works:**
- NetworkManager connection profiles are process-scoped
- Ethernet (enpXXX) remains active for internet
- AP crash → connection auto-deactivates → Ethernet resumes
- No manual `/etc/` file editing required for recovery
- Tested across 50+ start/stop cycles without issues

**Recovery Test:**
```bash
# If system is broken (shouldn't happen with current code):
sudo nmcli connection down DroneController 2>/dev/null
sudo nmcli connection delete DroneController 2>/dev/null
# Internet should be restored immediately via Ethernet
```

**Sudoers Rule (Required):**
```bash
# /etc/sudoers.d/drone-controller
angelo ALL=(ALL) NOPASSWD: /usr/bin/nmcli connection *
# Allows passwordless AP control without full root access
```

### 2. Signal Handling & Shutdown (CRITICAL - v1.5.4 DUAL-FLAG FIX)

**NEVER call handleShutdown() or join threads from within those threads!**

**Dual Shutdown Flag System:**
- `shutdown_requested_` = Stop application only (Ctrl+C, systemctl stop)
- `system_shutdown_requested_` = Power off Jetson (GUI shutdown button)

```cpp
// ✅ CORRECT - Shutdown signaling pattern (drone_web_controller)
// API handler for GUI shutdown button (runs in web server thread)
if (request.find("POST /api/shutdown") != std::string::npos) {
    response = generateAPIResponse("Shutdown initiated");
    send(client_socket, response.c_str(), response.length(), 0);
    system_shutdown_requested_ = true;  // Set SYSTEM shutdown flag!
    return;
}

// Signal handler (Ctrl+C - runs in signal handler context)
void signalHandler(int signal) {
    if (instance_) {
        instance_->shutdown_requested_ = true;  // Just set APP STOP flag!
    }
}

// Main loop (runs in main thread)
while (!controller.isShutdownRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

// Main function checks which flag was set
if (controller.isSystemShutdownRequested()) {
    system("sudo shutdown -h now");  // Only power off if GUI button pressed
}
// Ctrl+C exits cleanly without shutting down system
while (!controller.isShutdownRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
// Destructor called here → handleShutdown() → thread cleanup

// handleShutdown() - DON'T join web server thread (might be called from it)
void handleShutdown() {
    shutdown_requested_ = true;
    web_server_running_ = false;
    close(server_fd_);  // Close socket → thread exits naturally
    // NO thread join here!
}

// Destructor - Safe to join threads (main thread context)
~DroneWebController() {
    handleShutdown();
    if (web_server_thread_->joinable()) {
        web_server_thread_->join();  // Safe - we're NOT in web thread
    }
}

// ❌ WRONG - Causes "Resource deadlock avoided" error
if (request.find("POST /api/shutdown") != std::string::npos) {
    shutdownSystem();  // Calls handleShutdown() from web thread!
}
void handleShutdown() {
    web_server_thread_->join();  // DEADLOCK: Can't join self!
}
```

### 3. Thread Monitor Loop Exit (v1.3.4 CRITICAL FIX)
```cpp
// ✅ CORRECT - Use break to exit immediately
void recordingMonitorLoop() {
    while (recording_active_ && !shutdown_requested_) {
        if (current_state_ == RecorderState::STOPPING) {
            break;  // Exit loop NOW - prevents deadlock
        }
        // ... do work ...
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

// ❌ WRONG - DO NOT USE continue (causes web disconnect)
if (current_state_ == RecorderState::STOPPING) {
    continue;  // Stays in loop forever!
}
```

### 4. State Transition & Completion Flags (v1.5.4 CRITICAL TIMING)
```cpp
// ✅ CORRECT - Transition state BEFORE setting completion flag
bool stopRecording() {
    // ... perform all cleanup ...
    
    // CRITICAL: Transition state BEFORE setting completion flag
    // This ensures shutdown waits for COMPLETE state transition, not partial cleanup
    current_state_ = RecorderState::IDLE;
    std::cout << "State transitioned to IDLE" << std::endl;
    
    // Set completion flag AFTER state transition is complete
    recording_stop_complete_ = true;
    
    return true;
}

// handleShutdown waits for complete state transition
bool handleShutdown() {
    if (recording_active_) {
        stopRecording();  // Transitions state synchronously
        
        int wait_count = 0;
        while (!recording_stop_complete_ && wait_count < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        // When flag is true, state is ALREADY IDLE (not just STOPPING)
    }
}

// ❌ WRONG - Async state transition in separate thread
bool stopRecording() {
    // ... cleanup ...
    recording_stop_complete_ = true;  // Flag set but state still STOPPING!
    // State transition happens later in systemMonitorLoop (up to 5s delay)
}
```

### 5. USB Storage Detection & Filesystem Validation
```cpp
// common/storage/storage.cpp pattern
std::string filesystem = getFilesystemType(usb_path);
if (filesystem == "vfat" || filesystem == "msdos") {
    // FAT32 has 4GB limit - enable protection
    enable_size_limit_ = true;
    max_file_size_ = 3750000000;  // 3.75GB safety margin
    std::cout << "⚠️ FAT32 detected - 4GB limit active" << std::endl;
}
// Only NTFS/exFAT support >4GB continuous recording
```

### 6. ZED Camera Compression Mode
```cpp
// ✅ ONLY use LOSSLESS (Jetson Orin Nano lacks NVENC)
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::LOSSLESS;

// ❌ NEVER use H264/H265 - will fail with encoding error
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::H264;  // FAILS!
```

### 7. Exposure ↔ Shutter Speed Conversion (FPS-dependent)
```cpp
// Formula: shutter = FPS / (exposure / 100)
// Example: 50% exposure at 60 FPS = 1/120 shutter
std::string exposureToShutterSpeed(int exposure, int fps) {
    if (exposure <= 0) return "Auto";
    int shutter = static_cast<int>(std::round((fps * 100.0) / exposure));
    return "1/" + std::to_string(shutter);
}

// CRITICAL: Preserve exposure % across FPS changes, not shutter speed
// When changing from 60→30 FPS, 50% exposure changes from 1/120→1/60
```

### 8. LCD Thread Ownership Model
```cpp
// State-based ownership - ONE thread updates LCD per state:
// IDLE → systemMonitorLoop
// RECORDING → recordingMonitorLoop (3-second interval)
// STOPPING → stopRecording() function
// STOPPED (0-3s) → stopRecording() ("Recording Stopped" visible)
// STOPPED (>3s) → systemMonitorLoop (back to IDLE display)

// Check before updating:
if (recording_active_) {
    return;  // recordingMonitorLoop owns LCD
}
auto time_since_stop = now - recording_stopped_time_;
if (time_since_stop < 3s) {
    return;  // stopRecording owns LCD for 3 seconds
}
```

### 9. CORRUPTED_FRAME Tolerance (v1.5.3 FIELD ROBUSTNESS)
```cpp
// ✅ CORRECT - Treat as warning, continue recording (v1.5.3+)
sl::ERROR_CODE grab_result = active_camera.grab();
bool frame_corrupted = (grab_result == sl::ERROR_CODE::CORRUPTED_FRAME);

if (grab_result == sl::ERROR_CODE::SUCCESS || frame_corrupted) {
    consecutive_failures = 0;  // Reset for both SUCCESS and CORRUPTED_FRAME
    
    if (frame_corrupted) {
        std::cout << "[ZED] Warning: Frame may be corrupted (dark/covered lens), continuing..." << std::endl;
    }
    // Frame is saved (dark/black but no data loss) - recording continues
}

// ❌ WRONG - Treating CORRUPTED_FRAME as fatal error (v1.5.2 and earlier)
if (grab_result == sl::ERROR_CODE::SUCCESS) {
    consecutive_failures = 0;
} else {
    consecutive_failures++;  // CORRUPTED_FRAME increments counter!
    if (consecutive_failures >= max_consecutive_failures) {
        recording_ = false;  // ABORTS RECORDING - mission failure
        break;
    }
}

// Why this matters: Grass/leaves covering lens, very fast shutter (1/960+),
// or dark scenes cause CORRUPTED_FRAME. Recording should continue with
// dark frames rather than abort the mission.
```

### 10. Automatic Depth Mode Management (v1.5.2)
```cpp
// System auto-switches depth computation based on recording mode:

// SVO2 only → depth_mode = NONE (saves 30-70% CPU, compute depth later on PC)
if (recording_mode_ == RecordingModeType::SVO2) {
    depth_mode_ = sl::DEPTH_MODE::NONE;
}

// SVO2 + Depth recording → depth_mode = NEURAL_PLUS (best quality, required for depth data)
if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
    recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
    
    if (depth_mode_ == sl::DEPTH_MODE::NONE) {
        depth_mode_ = sl::DEPTH_MODE::NEURAL_PLUS;  // Auto-enable depth
        // Camera reinit required - save exposure, close, wait 3s, reinit, restore exposure
    }
}

// Prevents "depth map not computed (MODE_NONE)" SDK errors
// User never needs to manually adjust depth mode for recording modes
```

### 11. Completion Flags vs Timing Delays (v1.5.4 BEST PRACTICE)

**NEVER use arbitrary timing delays - use completion flags with timeout**

```cpp
// ✅ CORRECT - Wait for completion flag
bool DroneWebController::handleShutdown() {
    if (recording_active_) {
        stopRecording();  // Initiates cleanup
        
        // Wait for completion signal, not arbitrary delay
        int wait_count = 0;
        const int max_wait = 100;  // 10s timeout
        while (!recording_stop_complete_ && wait_count < max_wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        if (recording_stop_complete_) {
            std::cout << "✓ Completed in " << (wait_count * 100) << "ms" << std::endl;
        } else {
            std::cout << "⚠ Timeout after 10s" << std::endl;
        }
        
        sync();  // Final filesystem flush
    }
}

bool DroneWebController::stopRecording() {
    // ... all cleanup steps ...
    
    // CRITICAL: Set flag AFTER all cleanup is complete
    recording_stop_complete_ = true;
    return true;
}

// ❌ WRONG - Arbitrary timing guess
bool DroneWebController::handleShutdown() {
    if (recording_active_) {
        stopRecording();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Bad! 500ms might be too short or too long
        sync();
    }
}
```

**Why this matters:**
- Timing varies by recording mode, file size, USB speed, system load
- 500ms might cause data loss (too short) or waste time (too long)
- Completion flags adapt automatically to actual operation time
- See `docs/BEST_PRACTICE_COMPLETION_FLAGS.md` for full explanation

## 🛠️ Development Workflows

### Building & Testing
```bash
# Build all apps
./scripts/build.sh

# Run main controller manually (with sudo for WiFi)
sudo ./build/apps/drone_web_controller/drone_web_controller

# Test big file recording (4 min, HD720@30fps, ~6.6GB on NTFS)
sudo ./build/apps/smart_recorder/smart_recorder standard

# Monitor service logs
sudo journalctl -u drone-recorder -f

# Quick commands (if drone alias configured)
drone          # Start system
drone-status   # Check status
```

### Connect to System
1. WiFi SSID: `DroneController` / Password: `drone123`
2. Web UI: http://192.168.4.1:8080 or http://10.42.0.1:8080
3. LCD (optional): 16x2 I2C @ `/dev/i2c-7` (0x27 or 0x3F)

### USB Storage Requirements
- **Label:** `DRONE_DATA` (exact match)
- **Filesystem:** NTFS or exFAT (FAT32 = 4GB limit with auto-stop)
- **Size:** ≥128GB recommended (HD720@60fps = ~9.9GB per 4 min)
- **Mount:** Auto-detected at `/media/angelo/DRONE_DATA/`
- **Structure:** `flight_YYYYMMDD_HHMMSS/` → `video.svo2`, `sensor_data.csv`, `recording.log`

### Recording Modes (drone_web_controller)
- `SVO2` - Compressed video only (LOSSLESS, ~6.6GB/4min @ HD720@30fps)
- `SVO2_DEPTH_INFO` - Video + raw 32-bit depth data files
- `SVO2_DEPTH_IMAGES` - Video + depth PNG visualizations
- `RAW_FRAMES` - Separate left/right/depth images (highest quality)

### Camera Settings
- **Resolution/FPS:** HD720@30/60fps, HD1080@30fps, HD2K@15fps, VGA@100fps
- **Exposure:** -1=Auto, 0-100=Manual (50% @ 60fps = 1/120 shutter)
- **Depth Modes:** NEURAL_PLUS, NEURAL, ULTRA, QUALITY, PERFORMANCE
- **Default:** HD720@60fps, 50% exposure (1/120 shutter)

## 📝 Common Tasks

### Adding New Recording Mode
1. Add enum to `RecordingModeType` in `drone_web_controller.h`
2. Update `startRecording()` switch statement
3. Add UI controls in HTML template
4. Test thread interactions (start/stop cycles)

### Modifying LCD Display
- Max 16 characters per line
- Update interval: 3 seconds (balance visibility/CPU)
- Check thread ownership before updating
- Test "Recording Stopped" visibility (3-second hold)

### Adding Camera Setting
1. Validate setting doesn't require camera reinit (if possible)
2. If reinit needed: Save current exposure, close camera, wait 3s, reinit, reapply exposure
3. Update web UI slider/control
4. Add JavaScript sync in `updateStatus()`

### Debugging Thread Issues
- Check monitor loops use `break` not `continue` for exit conditions
- Verify only one `in-progress` state at a time
- Test rapid start/stop cycles (50+ iterations)
- Monitor for deadlocks: `sudo journalctl -u drone-recorder -f`

## 🚫 Common Mistakes to Avoid

1. **DON'T** use FAT32 for >4GB files (will corrupt)
2. **DON'T** use H.264/H.265 compression (NVENC unavailable)
3. **DON'T** use `continue` in state-check loops (causes deadlocks)
4. **DON'T** let multiple threads update LCD simultaneously
5. **DON'T** hard-code exposure values without documenting FPS relationship
6. **DON'T** exit without cleanup (leaves ZED camera open, USB mounted, **AP active**)
7. **DON'T** trust duration for file size estimates (validate actual bytes)
8. **DON'T** modify permanent network configs (use nmcli connection profiles)
9. **DON'T** add iptables rules without cleanup handlers
10. **DON'T** modify `WorkingDirectory` in systemd service (breaks relative paths)
11. **DON'T** disable NetworkManager or modify `/etc/NetworkManager/` configs
12. **DON'T** assume hardware works as documented (test thoroughly)
13. **DON'T** treat CORRUPTED_FRAME as fatal error (field robustness - it's a warning)
14. **DON'T** manually manage depth mode for recording types (auto-managed in v1.5.2+)
15. **DON'T** use different stop paths for timer vs manual (unified in v1.5.2+)
16. **DON'T** join threads from within themselves (shutdown deadlock - v1.5.4 fix)
17. **DON'T** call blocking cleanup from web server request handlers (use flags instead)
18. **DON'T** use arbitrary timing delays - use completion flags with timeout (v1.5.4 best practice)
19. **DON'T** set completion flags before state transitions finish (wait for IDLE, not STOPPING - v1.5.4 fix)

## 📍 Where to Add Code

- **New app:** `apps/<new_app>/` + `CMakeLists.txt` + update `copilot-instructions.md`
- **Shared camera logic:** `common/hardware/zed_camera/`
- **Storage handling:** `common/storage/`
- **Web API endpoint:** `drone_web_controller.cpp` `handleClientRequest()`
- **LCD message:** `lcd_handler.cpp` or `drone_web_controller.cpp` `updateLCD()`
- **Recording profile:** `smart_recorder/main.cpp` mode switch

## 🔍 Key Files Reference

| File | Purpose |
|------|---------|
| `apps/drone_web_controller/drone_web_controller.cpp` | Main controller logic (~2280 lines) |
| `common/hardware/zed_camera/zed_recorder.cpp` | ZED SDK wrapper, SVO2 recording |
| `common/hardware/zed_camera/raw_frame_recorder.cpp` | RAW frame capture with depth |
| `common/storage/storage.cpp` | USB detection, filesystem validation |
| `systemd/drone-recorder.service` | Production systemd unit |
| `CMakeLists.txt` | Build configuration (C++17, CUDA paths) |
| `docs/CRITICAL_LEARNINGS_v1.3.md` | Complete development journey (MUST READ) |
| `docs/SHUTDOWN_DEADLOCK_FIX_v1.5.4.md` | Shutdown thread deadlock fix |
| `docs/STATE_TRANSITION_FIX_v1.5.4.md` | State transition timing fix |
| `RELEASE_v1.5.3_STABLE.md` | Latest features and fixes |
| `RELEASE_v1.5.2_STABLE.md` | Auto depth management, unified stop |

## 📚 Performance Baselines (Validated)

- **HD720@60fps LOSSLESS:** 25-28 MB/s, 9.9GB per 4 min, 0 frame drops
- **HD720@30fps LOSSLESS:** 13 MB/s, 6.6GB per 4 min
- **CPU Usage:** 45-60% (all cores during recording)
- **LCD Update:** ~80ms per full write, 3-second safe interval
- **WiFi Range:** ~50m open field (2.4GHz AP)

---

**Questions?** Check section you need expanded:
- Profile list details → `smart_recorder` CLI args
- Storage guard specifics → `common/storage/storage.cpp` validation
- Service environment → `systemd/drone-recorder.service` full config
- Network setup → `common/networking/safe_hotspot_manager.cpp`
