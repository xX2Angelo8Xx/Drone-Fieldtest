# Copilot Instructions for Drone Field Test System

**Platform:** Jetson Orin Nano + ZED 2i stereo camera | **Primary App:** `drone_web_controller` (WiFi AP + Web UI @ http://192.168.4.1:8080)

## 🎓 MANDATORY READING BEFORE ANY CHANGES

Read **`docs/CRITICAL_LEARNINGS_v1.3.md`** first - it documents:
- FAT32 4GB corruption issue → NTFS/exFAT requirement
- NVENC hardware encoder unavailable → LOSSLESS compression only
- Thread deadlock fix (`break` not `continue` in monitor loops)
- LCD thread ownership conflicts and resolution
- Shutter speed ↔ exposure conversion (FPS-dependent)

Other essential docs:
- `RELEASE_v1.3_STABLE.md` - Complete feature reference
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

### 2. Signal Handling (ALL apps use this)
```cpp
std::atomic<bool> g_running(true);

void signalHandler(int signum) {
    std::cout << "Signal received" << std::endl;
    g_running = false;  // Set flag, DON'T exit()
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    while (g_running) {
        // Work...
    }
    
    // Clean shutdown (including network teardown!)
    if (g_recorder) g_recorder->release();
    if (g_storage) g_storage->cleanup();
    if (g_hotspot) g_hotspot->teardown();  // Restore Ethernet access
    return 0;
}
```

### 3. Thread Monitor Loop Exit (v1.3.4 CRITICAL FIX)

### 2. Thread Monitor Loop Exit (v1.3.4 CRITICAL FIX)
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

### 3. USB Storage Detection & Filesystem Validation
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

### 4. ZED Camera Compression Mode
```cpp
// ✅ ONLY use LOSSLESS (Jetson Orin Nano lacks NVENC)
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::LOSSLESS;

// ❌ NEVER use H264/H265 - will fail with encoding error
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::H264;  // FAILS!
```

### 5. Exposure ↔ Shutter Speed Conversion (FPS-dependent)
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

### 6. LCD Thread Ownership Model
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
| `apps/drone_web_controller/drone_web_controller.cpp` | Main controller logic (1816 lines) |
| `common/hardware/zed_camera/zed_recorder.cpp` | ZED SDK wrapper, SVO2 recording |
| `common/storage/storage.cpp` | USB detection, filesystem validation |
| `systemd/drone-recorder.service` | Production systemd unit |
| `CMakeLists.txt` | Build configuration (C++17, CUDA paths) |

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
- Network setup → `common/networking/safe_hotspot_manager.cpp`em, v1.3-stable)

Purpose: Help AI agents be productive immediately in this Jetson Orin Nano + ZED 2i field-testing repo. Primary app is `apps/drone_web_controller` (WiFi AP + web UI at http://192.168.4.1:8080). Secondary tools: `smart_recorder`, `live_streamer`, `performance_test`, `zed_cli_recorder`.

**🎓 CRITICAL: New to this project? Read `docs/CRITICAL_LEARNINGS_v1.3.md` FIRST!**
- Complete development journey with ALL learnings
- Common mistakes and how to avoid them
- Thread deadlock fixes, filesystem issues, encoder limitations
- Testing methodology and performance baselines
- Essential for understanding WHY things work this way

Core layout and build
- Apps: `apps/` → binaries at `build/apps/<app>/<app>` (C++17, CMake). Build with `./scripts/build.sh`.
- Shared libs: `common/` → `hardware/` (ZED, LCD), `storage/` (USB auto-detect), `streaming/`, `utils/`.
- Deploy: systemd unit `systemd/drone-recorder.service` (runs as user `angelo`, 10s delay, working dir = repo root). Monitor with `sudo journalctl -u drone-recorder -f`.
- Quick start: `drone` alias or `./drone_start.sh`. WiFi AP SSID `DroneController` / password `drone123`.

Critical invariants (don’t break)
- Storage: USB must be labeled `DRONE_DATA`. Paths like `/media/angelo/DRONE_DATA/flight_YYYYMMDD_HHMMSS/` with `video.svo2`, `sensor_data.csv`, `recording.log`.
- Filesystem: NTFS/exFAT required for >4GB. Continuous single-file `.svo2` (tested 6.6–9.9GB). If FAT32 detected, auto-stop at ~3.75GB to avoid 4GB boundary corruption.
- ZED: Use `.svo2`; LOSSLESS only (Jetson Orin Nano lacks NVENC). CUDA 12.6 present at `/usr/local/cuda-12.6/include`.
- Signals: All apps share the same pattern — global `g_running` flag set false in `signalHandler`; main loop exits cleanly; release `g_recorder`/`g_storage` safely.
- Working dir: Many relative paths assume repo root; systemd service enforces this. Keep user=`angelo` and environment expectations.

Web controller and networking
- `drone_web_controller` creates AP + serves HTML5 UI (mobile-optimized). Do not change SSID/password/IP layout without updating scripts and service.
- Dual-network model: Ethernet for internet, WiFi AP for control (hostapd + dnsmasq). Use provided scripts and sudoers rule at `/etc/sudoers.d/drone-controller` (passwordless for WiFi ops).

Recorder patterns
- `smart_recorder`: enum-based `RecordingMode` with profile CLI. Examples: `standard` (4min HD720@30fps), `realtime_light` (30s HD720@15fps), `training` (60s HD1080@30fps). Keep profile architecture and unlimited-file design on NTFS/exFAT.
- Extraction: `build/tools/svo_extractor <svo2> <stride>` → JPEGs under `extracted_images/flight_*_video/`.

LCD and resilience
- LCD: I2C on `/dev/i2c-7`, 16-char formatting helper, non-blocking (system continues if LCD absent). Diagnostic tools: `i2c_lcd_tester`, `simple_lcd_test`, `lcd_backlight_test`.
- Error recovery: USB detection retries every 5s; graceful degradation across components. Preserve these behaviors.

Typical workflows
- Build: `./scripts/build.sh` → run `sudo ./build/apps/drone_web_controller/drone_web_controller` (manual) or manage via `drone-recorder` service.
- Validate big files: `sudo ./build/apps/smart_recorder/smart_recorder standard` (expect ~6.6GB in 4 min on NTFS/exFAT).
- Inspect output: `ls -lt /media/angelo/DRONE_DATA/flight_* | head -5` and open with `/usr/local/zed/tools/ZED_Explorer`.

Where to add code
- New features in `apps/<app>/`; shared logic in `common/*`. Follow signal pattern, storage conventions, and profile-based execution. Keep network/service assumptions and paths stable.

If anything above is unclear or incomplete (e.g., profile list, exact storage guards, or service env), say which section you need expanded and I’ll refine it.