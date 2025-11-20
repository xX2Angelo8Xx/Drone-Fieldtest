# 🚀 Release v1.5.2 - Production Ready Stable

**Release Date:** November 18, 2025  
**Status:** ✅ STABLE - Ready for Production Field Testing  
**Platform:** Jetson Orin Nano + ZED 2i Stereo Camera

---

## 📦 Release Summary

Version 1.5.2 represents the **production-ready stable release** of the drone web controller system. This release includes critical bug fixes discovered during extensive field testing, automatic resource management, and UI consistency improvements. All known bugs have been resolved, making this the recommended version for deployment.

**Key Highlights:**
- ✅ Automatic depth mode management (saves Jetson resources)
- ✅ Unified stop recording routine (robust cleanup for all stop methods)
- ✅ Shutter speed display stability (no jumping after slider release)
- ✅ Livestream initialization fixes (predictable startup state)
- ✅ Clean photographer-friendly shutter speeds at all FPS levels
- ✅ Dark image handling (CORRUPTED_FRAME tolerance)
- ✅ Graceful shutdown (no GUI flicker)
- ✅ WiFi TX power optimized (20 dBm, ~50-60m range)

---

## 🎯 What's New in v1.5.2

### 1. Automatic Depth Mode Management (Critical)

**Problem:** User could enable SVO2+Depth recording with depth mode set to NONE, causing SDK errors. Switching between SVO2 only and SVO2+Depth didn't automatically adjust depth computation.

**Solution:**
- **SVO2 only** → Depth automatically set to **NONE** (saves 30-70% CPU)
- **SVO2 + Depth** → Depth automatically set to **NEURAL_PLUS** (best quality)
- Camera reinitializes when transitioning between modes
- No manual intervention needed

**Benefits:**
- Prevents "depth map not computed (MODE_NONE)" errors
- Optimal resource usage (depth disabled when not needed)
- Best quality depth when recording (NEURAL_PLUS default)
- Compute depth later on PC for SVO2 only recordings

**Documentation:** `docs/DEPTH_MODE_AUTO_MANAGEMENT_v1.5.2.md`

---

### 2. Unified Stop Recording Routine (Critical)

**Problem:** Timer-based stop (when recording reaches duration) used different code path than manual stop. Timer path skipped DepthDataWriter cleanup, depth thread termination, and depth computation disable.

**Solution:**
- Flag-based handoff (`timer_expired_`) between threads
- Both manual and timer stop now use same `stopRecording()` function
- Complete cleanup: DepthDataWriter, depth_viz_thread, depth computation
- No deadlock (stop called from main thread, not monitor thread)

**Benefits:**
- Consistent behavior (same cleanup for manual and timer stop)
- No resource leaks (all threads and files properly closed)
- Identical LCD messages and timing
- Clean state between sequential recordings

**Documentation:** `docs/TIMER_STOP_UNIFICATION_v1.5.2.md`

---

### 3. Shutter Speed Display Stability (v1.5.1)

**Problem:** Shutter speed displayed correctly when slider moved, but jumped to calculated value (e.g., 1/180 → 1/182) after slider release due to status update recalculation.

**Solution:**
- Display shutter speed from array (clean photographer value)
- Don't recalculate from E-value in status updates
- Consistent display throughout interaction

**Documentation:** `docs/UI_CONSISTENCY_FIX_v1.5.1.md`

---

### 4. Livestream Initialization Fix (v1.5.1)

**Problem:** Browser HTML5 form persistence restored livestream checkbox to "enabled" with "10 FPS" selected, causing confusion (actual livestream was off at 2 FPS).

**Solution:**
- Explicit JavaScript initialization in DOMContentLoaded
- Force checkbox unchecked and dropdown to 2 FPS
- Override browser form state restoration

**Documentation:** `docs/UI_CONSISTENCY_FIX_v1.5.1.md`

---

### 5. Clean Shutter Speeds (v1.5.0)

**Implementation:** Dynamic shutter speed generation from photographer-friendly values (60, 90, 120, 150, 180, 240, 360, 480, 720, 960, 1200). E-values calculated automatically based on camera FPS.

**Benefits:**
- No unconventional values (e.g., 1/153, 1/182)
- Adapts to all camera FPS modes (15, 30, 60, 100 FPS)
- E-value constraints respected (5-100 range)

**Documentation:** `docs/FIELD_TEST_IMPROVEMENTS_v1.5.0.md`

---

### 6. CORRUPTED_FRAME Tolerance (v1.5.0)

**Problem:** Fast shutter speeds (1/960, 1/1200) or covered lens caused ZED SDK to flag CORRUPTED_FRAME error. Old code rejected these frames → GUI froze, no image displayed.

**Solution:**
- Treat CORRUPTED_FRAME as warning, not fatal error
- Display dark/black image (matches ZED Explorer behavior)
- System continues functioning, smooth transition when conditions improve

**Documentation:** `docs/FIELD_TEST_IMPROVEMENTS_v1.5.0.md`

---

### 7. Graceful Shutdown (v1.5.0)

**Problem:** Pressing Ctrl+C with active livestream caused browser to try displaying error text as image → visible flickering.

**Solution:**
- Return 1x1 transparent GIF (43 bytes) during shutdown
- Browser displays invisible pixel → no flicker
- Clean shutdown UX

**Documentation:** `docs/FIELD_TEST_IMPROVEMENTS_v1.5.0.md`

---

### 8. WiFi TX Power Boost (v1.4.8)

**Implementation:** Increased from ~15 dBm (default) to 20 dBm (100 mW maximum for 2.4 GHz).

**Benefits:**
- Range: ~30m → ~50-60m open field (+60-100%)
- Bandwidth stability at 30m: 400-600 KB/s → 650-700 KB/s
- Power saving disabled for consistent performance

**Documentation:** `docs/WIFI_TX_POWER_BOOST_v1.4.8.md`

---

## 🔧 Technical Details

### System Architecture

**Platform:**
- Jetson Orin Nano Developer Kit (6-core ARM CPU, 8GB RAM)
- Ubuntu 22.04 LTS, JetPack 6.0
- ZED SDK 4.2+
- ZED 2i Stereo Camera

**Network:**
- WiFi AP: 2.4 GHz, SSID "DroneController", password "drone123"
- IP Address: 10.42.0.1 (NetworkManager-managed, auto-cleanup on crash)
- Web UI: http://10.42.0.1:8080
- TX Power: 20 dBm (100 mW), ~50-60m range

**Recording Modes:**
1. **SVO2 only** (Default) - Compressed video, depth computed later on PC
   - Depth Mode: NONE (auto-set, saves 30-70% CPU)
   - File: Single `.svo2` file (~6.6GB for 4 min @ HD720@30fps)
   
2. **SVO2 + Depth Info** - Video + raw 32-bit depth data files
   - Depth Mode: NEURAL_PLUS (auto-set, best quality)
   - Files: `.svo2` + `depth_data.bin`
   
3. **SVO2 + Depth Images** - Video + depth PNG visualizations
   - Depth Mode: NEURAL_PLUS (auto-set)
   - Files: `.svo2` + `depth_images/*.png` (configurable FPS)
   
4. **RAW Frames** - Separate left/right/depth images (highest quality)
   - Files: `left/`, `right/`, `depth/` directories with individual images

**Camera Modes:**
- HD720@60fps (Default) - 1280×720, 1/120 shutter (50% exposure)
- HD720@30fps - 1280×720
- HD1080@30fps - 1920×1080
- HD2K@15fps - 2208×1242
- VGA@100fps - 672×376

**Storage:**
- USB Label: `DRONE_DATA` (exact match required)
- Filesystem: NTFS or exFAT (FAT32 = 4GB limit with auto-stop)
- Mount: `/media/angelo/DRONE_DATA/`
- Structure: `flight_YYYYMMDD_HHMMSS/` with video, depth, logs

---

## 🧪 Testing Checklist

### Pre-Deployment Testing

**System Boot:**
- [ ] System starts automatically (systemd service)
- [ ] WiFi AP active within 10 seconds
- [ ] Web UI accessible at http://10.42.0.1:8080
- [ ] USB storage detected and mounted
- [ ] LCD displays "Web Controller / 10.42.0.1:8080"

**Camera Defaults:**
- [ ] HD720@60fps selected in GUI
- [ ] Exposure: 1/120 (50% E-value)
- [ ] SVO2 only mode selected
- [ ] Depth dropdown hidden
- [ ] Livestream: OFF, 2 FPS dropdown

**Recording - SVO2 Only:**
- [ ] Start 10-second recording
- [ ] Console: "Depth computation DISABLED (SVO2 only)"
- [ ] Manual stop: Complete cleanup
- [ ] Timer stop: Complete cleanup (same as manual)
- [ ] Files: `.svo2`, `sensor_data.csv`, `recording.log`
- [ ] No depth data files

**Recording - SVO2 + Depth Info:**
- [ ] Switch to "SVO2 + Depth Info"
- [ ] Console: "Auto-switching depth mode to NEURAL_PLUS"
- [ ] Depth dropdown visible, NEURAL_PLUS selected
- [ ] Start 10-second recording
- [ ] Console: "Depth computation ENABLED with mode: NEURAL_PLUS"
- [ ] Timer stop: DepthDataWriter stopped, frame count reported
- [ ] Files: `.svo2`, `depth_data.bin` (>0 bytes), logs

**Recording - SVO2 + Depth Images:**
- [ ] Switch to "SVO2 + Depth Images"
- [ ] Set depth FPS = 10
- [ ] Start 15-second recording
- [ ] Timer stop: depth_viz_thread stopped
- [ ] Files: `.svo2`, `depth_images/*.png` (~150 images), logs

**Shutter Speed:**
- [ ] Move slider to 1/180
- [ ] Release slider
- [ ] Wait 2 seconds
- [ ] Display remains "1/180" (doesn't jump to 1/182)

**Livestream:**
- [ ] Refresh browser page
- [ ] Livestream checkbox: unchecked
- [ ] FPS dropdown: 2 FPS
- [ ] No image visible
- [ ] Enable livestream → image appears, actual FPS = 2

**Mode Transitions:**
- [ ] SVO2 only → SVO2+Depth → Depth = NEURAL_PLUS
- [ ] SVO2+Depth → SVO2 only → Depth = NONE
- [ ] No "depth map not computed" errors

**Shutdown:**
- [ ] Start livestream @ 10 FPS
- [ ] Press Ctrl+C
- [ ] No GUI flicker
- [ ] System stops cleanly
- [ ] Ethernet internet still works (WiFi AP auto-cleaned up)

---

## 📊 Performance Metrics

### Recording Performance (Validated)
- **HD720@60fps LOSSLESS:** 25-28 MB/s, 9.9GB per 4 min, 0 frame drops
- **HD720@30fps LOSSLESS:** 13 MB/s, 6.6GB per 4 min, 0 frame drops
- **CPU Usage (Recording):** 45-60% (all cores)
- **CPU Usage (SVO2+Depth):** 65-75% (NEURAL_PLUS depth mode)

### Network Performance
- **WiFi AP Range:** ~50-60m open field (20 dBm TX power)
- **Max Throughput:** ~700 KB/s @ 2.4 GHz AP mode (hardware limit)
- **Optimal Livestream:** 10 FPS @ 700 KB/s (smooth, no stuttering)
- **Livestream CPU Overhead:** +5-10% (JPEG quality 85, ~75 KB/frame)

### Resource Savings
- **SVO2 only (Depth=NONE):** Saves 30-70% CPU vs. depth enabled
- **Power Consumption:** ~18W typical, +0.5W with WiFi TX power boost

---

## 🐛 Known Issues & Limitations

### Hardware Limitations
1. **NVENC Unavailable:** Jetson Orin Nano lacks hardware H.264/H.265 encoder
   - **Workaround:** Use LOSSLESS compression only (tested, 0 frame drops)
   
2. **5 GHz WiFi "no IR":** Cannot create 5 GHz AP (only client mode)
   - **Workaround:** Use 2.4 GHz AP (20 dBm TX power, sufficient for field work)

3. **2.4 GHz Bandwidth:** ~700 KB/s maximum throughput
   - **Workaround:** Livestream @ 10 FPS is smooth, use lower FPS for longer range

### Software Limitations
1. **FAT32 4GB Limit:** Auto-stops recording at 3.75GB on FAT32 filesystems
   - **Solution:** Use NTFS or exFAT (documented in GUI)
   
2. **Browser Form Persistence:** May restore checkbox/dropdown states
   - **Solution:** Explicit JavaScript initialization (implemented v1.5.1)

### Future Enhancements
- Battery monitoring hardware integration (UI placeholder ready)
- Real-time bandwidth display (current estimated bandwidth OK)
- Object detection integration (architecture documented)
- Streaming to RTMP server (architecture complete, deployment pending)

---

## 📁 File Structure

```
Drone-Fieldtest/
├── apps/
│   ├── drone_web_controller/     ⭐ Main application
│   ├── smart_recorder/           CLI recorder with profiles
│   ├── live_streamer/            H.264/H.265 streaming + telemetry
│   ├── performance_test/         Hardware validation
│   └── zed_cli_recorder/         Wrapper for ZED Explorer CLI
├── common/
│   ├── hardware/                 ZEDRecorder, RawFrameRecorder, I2C_LCD
│   ├── storage/                  StorageHandler (USB detection, filesystem validation)
│   ├── networking/               SafeHotspotManager (NetworkManager-based)
│   └── utils/                    Shared utilities
├── docs/
│   ├── CRITICAL_LEARNINGS_v1.3.md              **READ THIS FIRST**
│   ├── DEPTH_MODE_AUTO_MANAGEMENT_v1.5.2.md    Depth mode auto-switching
│   ├── TIMER_STOP_UNIFICATION_v1.5.2.md        Stop routine unification
│   ├── UI_CONSISTENCY_FIX_v1.5.1.md            Shutter speed & livestream
│   ├── FIELD_TEST_IMPROVEMENTS_v1.5.0.md       Clean speeds, CORRUPTED_FRAME
│   ├── SHUTTER_SPEED_FIX_v1.4.9.md             FPS-dependent calculation
│   ├── WIFI_TX_POWER_BOOST_v1.4.8.md           20 dBm optimization
│   └── GUI_CLEANUP_v1.4.7.md                   Initial polish
├── systemd/
│   └── drone-recorder.service    Production systemd unit
├── scripts/
│   └── build.sh                  Build script (C++17, CMake)
└── build/                        Build artifacts
    └── apps/drone_web_controller/drone_web_controller
```

---

## 🚀 Deployment Instructions

### System Requirements
- Jetson Orin Nano Developer Kit
- ZED 2i Stereo Camera (USB connection)
- WiFi adapter (wlP1p1s0)
- USB storage (≥128GB, NTFS/exFAT, labeled "DRONE_DATA")
- Optional: 16x2 I2C LCD (address 0x27 or 0x3F, bus `/dev/i2c-7`)

### Installation Steps

1. **Clone Repository:**
```bash
cd ~/Projects/
git clone <repository-url> Drone-Fieldtest
cd Drone-Fieldtest
git checkout v1.5.2  # Checkout stable release
```

2. **Build Application:**
```bash
./scripts/build.sh
# Expected: [100%] Built target drone_web_controller
```

3. **Configure Sudoers (WiFi AP):**
```bash
sudo visudo -f /etc/sudoers.d/drone-controller
# Add line:
angelo ALL=(ALL) NOPASSWD: /usr/bin/nmcli connection *
```

4. **Install Systemd Service:**
```bash
sudo cp systemd/drone-recorder.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable drone-recorder
sudo systemctl start drone-recorder
```

5. **Verify Installation:**
```bash
sudo systemctl status drone-recorder
# Expected: active (running)

sudo journalctl -u drone-recorder -f
# Expected: WiFi Hotspot Active! / Web Interface: http://10.42.0.1:8080
```

6. **Test Web Interface:**
```bash
# Connect to WiFi:
# SSID: DroneController
# Password: drone123

# Browser: http://10.42.0.1:8080
# Expected: Drone Controller web interface loads
```

---

## 🔄 Upgrade from Previous Version

### From v1.3.x → v1.5.2

**Breaking Changes:** None (fully backward compatible)

**Upgrade Steps:**
```bash
cd ~/Projects/Drone-Fieldtest
git fetch origin
git checkout v1.5.2
./scripts/build.sh
sudo systemctl restart drone-recorder
```

**Verify Upgrade:**
- Browser refresh: http://10.42.0.1:8080
- Check shutter speed slider (should show clean values)
- Check depth dropdown (NEURAL_PLUS first option)
- Test timer-based recording stop

**Config Preservation:**
- Network settings preserved (systemd service unchanged)
- USB storage location unchanged
- Recording defaults improved (depth management added)

---

## 📞 Support & Troubleshooting

### Common Issues

**Issue: "USB storage not detected"**
- Verify label: `lsblk -o NAME,LABEL,FSTYPE,MOUNTPOINT`
- Must be exactly "DRONE_DATA" (case-sensitive)
- Filesystem must be NTFS or exFAT

**Issue: "Camera initialization failed"**
- Check USB connection: `lsusb | grep Stereolabs`
- Verify ZED SDK: `/usr/local/zed/tools/ZED_Explorer`
- Check permissions: User `angelo` must be in `video` group

**Issue: WiFi AP not starting**
- Check NetworkManager: `nmcli connection show`
- Verify interface: `ip link show wlP1p1s0`
- Check logs: `sudo journalctl -u drone-recorder -f`

**Issue: "Depth map not computed (MODE_NONE)"**
- **Fixed in v1.5.2!** Upgrade to this version
- Verify auto-switching: Console should show depth mode changes

**Issue: Shutter speed jumps after slider release**
- **Fixed in v1.5.1!** Included in v1.5.2
- Hard refresh browser: Ctrl+Shift+R

### Logs & Diagnostics

**System Logs:**
```bash
# Service status
sudo systemctl status drone-recorder

# Recent logs
sudo journalctl -u drone-recorder --since "10 minutes ago"

# Follow logs live
sudo journalctl -u drone-recorder -f
```

**Network Diagnostics:**
```bash
# Check WiFi AP
nmcli connection show DroneController

# Verify TX power
iw dev wlP1p1s0 info | grep txpower
# Expected: txpower 20.00 dBm

# Check connected clients
iw dev wlP1p1s0 station dump
```

**USB Storage:**
```bash
# Check mount
mount | grep DRONE_DATA

# Verify filesystem
df -hT /media/angelo/DRONE_DATA/

# Check free space
du -sh /media/angelo/DRONE_DATA/flight_*
```

---

## 📝 Version History

### v1.5.2 (2025-11-18) - Current Release
- ✅ Automatic depth mode management
- ✅ Unified stop recording routine
- ✅ Shutter speed display stability (v1.5.1)
- ✅ Livestream initialization fix (v1.5.1)
- ✅ Clean shutter speeds (v1.5.0)
- ✅ CORRUPTED_FRAME tolerance (v1.5.0)
- ✅ Graceful shutdown (v1.5.0)
- ✅ WiFi TX power boost (v1.4.8)
- ✅ Camera FPS API (v1.4.9)
- ✅ GUI cleanup (v1.4.7)

### v1.3.4 - Thread Interaction Fix
- Thread deadlock prevention (`break` not `continue` in monitor loops)
- LCD thread ownership model

### v1.3.0 - Initial Stable
- Core recording functionality
- Web UI with livestream
- Multiple recording modes
- Safe WiFi AP management

---

## 🎓 Development Documentation

### Essential Reading (Priority Order)
1. **`docs/CRITICAL_LEARNINGS_v1.3.md`** - All development learnings, common mistakes
2. **`NETWORK_SAFETY_POLICY.md`** - WiFi AP safety requirements (MUST READ before network changes)
3. **`docs/DEPTH_MODE_AUTO_MANAGEMENT_v1.5.2.md`** - Depth mode logic
4. **`docs/TIMER_STOP_UNIFICATION_v1.5.2.md`** - Stop routine patterns
5. **`.github/copilot-instructions.md`** - Complete architecture guide

### Key Development Patterns

**Signal Handling:**
```cpp
std::atomic<bool> g_running(true);
void signalHandler(int signum) { g_running = false; }

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    while (g_running) { /* work */ }
    
    // Clean shutdown: recorders, storage, hotspot
    return 0;
}
```

**Thread Monitor Exit:**
```cpp
void recordingMonitorLoop() {
    while (recording_active_ && !shutdown_requested_) {
        if (current_state_ == RecorderState::STOPPING) {
            break;  // Exit immediately, don't continue
        }
        // ... work ...
    }
}
```

**Depth Mode Auto-Management:**
```cpp
void setRecordingMode(RecordingModeType mode) {
    if (mode == RecordingModeType::SVO2) {
        depth_mode_ = DepthMode::NONE;  // Auto-switch
    } else if (mode == depth_recording_mode) {
        if (depth_mode_ == DepthMode::NONE) {
            depth_mode_ = DepthMode::NEURAL_PLUS;  // Auto-switch
        }
    }
    // ... reinitialize camera ...
}
```

---

## 🏆 Credits & Acknowledgments

**Project:** Drone Field Testing System  
**Platform:** Jetson Orin Nano + ZED 2i  
**Primary Developer:** Angelo  
**AI Assistant:** GitHub Copilot (extensive pair programming)

**Special Thanks:**
- Stereolabs for ZED SDK and excellent documentation
- NVIDIA for Jetson platform and JetPack
- NetworkManager team for safe WiFi AP management patterns

---

## 📄 License

[Your License Here]

---

## 🔗 Quick Links

- **Web Interface:** http://10.42.0.1:8080 (after deployment)
- **System Logs:** `sudo journalctl -u drone-recorder -f`
- **Documentation:** `docs/` directory
- **Quick Start:** `.github/copilot-instructions.md`
- **Build Script:** `./scripts/build.sh`

---

**Status:** ✅ PRODUCTION READY  
**Recommended For:** Field deployment, long-term testing, production use  
**Next Release:** TBD (no critical issues known)

---

