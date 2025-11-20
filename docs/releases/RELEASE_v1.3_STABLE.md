# Drone Field Test System - v1.3 Stable Release

## Release Date: November 17, 2025

---

## 🎯 Overview

The Drone Field Test System is a complete aerial data collection platform running on NVIDIA Jetson Orin Nano with ZED 2i stereo camera. This stable release includes a fully functional web-based controller with WiFi Access Point, LCD status display, and professional camera controls.

### Key Features
- ✅ **WiFi Access Point Mode** - Direct connection to drone without router
- ✅ **Web-Based Controller** - Mobile-optimized GUI at http://192.168.4.1:8080
- ✅ **LCD Status Display** - Real-time recording information on 16x2 I2C LCD
- ✅ **Professional Shutter Speed Control** - Photography-standard notation (1/60, 1/120, etc.)
- ✅ **Multiple Recording Modes** - SVO2, SVO2+Depth, RAW frames
- ✅ **Automatic Storage Management** - USB detection and monitoring
- ✅ **Robust Error Handling** - Graceful degradation and recovery

---

## 📦 System Requirements

### Hardware
- **Computer:** NVIDIA Jetson Orin Nano (8GB recommended)
- **Camera:** ZED 2i Stereo Camera
- **Storage:** USB drive labeled `DRONE_DATA` (NTFS/exFAT, ≥128GB recommended)
- **Display:** 16x2 I2C LCD (optional, system works without it)
- **Network:** Onboard WiFi for Access Point mode

### Software
- **OS:** Ubuntu 22.04 (JetPack 6.0)
- **ZED SDK:** 4.2+
- **CUDA:** 12.6
- **CMake:** 3.16+
- **Dependencies:** See `EXTERNAL_FILES_DOCUMENTATION.md`

---

## 🚀 Quick Start

### 1. Installation

```bash
# Clone repository
git clone https://github.com/xX2Angelo8Xx/Drone-Fieldtest.git
cd Drone-Fieldtest

# Build project
./scripts/build.sh

# Install systemd service (optional, for auto-start)
sudo ./scripts/install_service.sh
```

### 2. Prepare USB Storage

```bash
# Format USB drive
sudo mkfs.ntfs -Q -L DRONE_DATA /dev/sdX1  # Replace X with your drive

# Or use exFAT
sudo mkfs.exfat -n DRONE_DATA /dev/sdX1
```

### 3. Start System

**Manual Start:**
```bash
sudo ./build/apps/drone_web_controller/drone_web_controller
```

**Service Start:**
```bash
sudo systemctl start drone-recorder
sudo journalctl -u drone-recorder -f  # Monitor logs
```

**Using Alias:**
```bash
drone  # Shortcut for sudo systemctl start drone-recorder
```

### 4. Connect and Control

1. Connect to WiFi: **SSID:** `DroneController` | **Password:** `drone123`
2. Open browser: http://192.168.4.1:8080
3. Start recording, adjust settings, monitor status

---

## 📱 Web Controller Interface

### Main Controls

**Recording Control:**
- **Start Recording** - Begin data capture
- **Stop Recording** - End recording (shows immediate feedback)
- **Status Display** - IDLE / RECORDING / STOPPING

**Recording Modes:**
- **SVO2 Only** - Compressed stereo video (fastest, smallest)
- **SVO2 + Raw Depth** - Video + 32-bit depth data (for post-processing)
- **SVO2 + Depth PNG** - Video + visualization images (human-readable)
- **RAW Frames** - Separate left/right/depth images (largest, most flexible)

**Camera Settings:**
- **Resolution/FPS:**
  - HD720 @ 60 FPS (default, balanced)
  - HD1080 @ 30 FPS (high resolution)
  - VGA @ 100 FPS (high speed)
  - HD2K @ 15 FPS (maximum resolution)
  
- **Shutter Speed:** (12 positions)
  - Auto
  - 1/60, 1/90, 1/120 ⭐ (default)
  - 1/150, 1/180, 1/240
  - 1/360, 1/480, 1/720
  - 1/960, 1/1200 (fastest)

- **Depth Mode:** (for depth recording modes)
  - NEURAL_LITE (fast, good quality)
  - NEURAL (best quality)
  - ULTRA (maximum detail)

### Real-Time Information

**During Recording:**
- Elapsed time / Total duration
- File size (GB)
- Write speed (MB/s)
- Current FPS
- Progress percentage

**Status Messages:**
- Camera initialization status
- USB storage detection
- Recording mode changes
- Error notifications

---

## 🖥️ LCD Display

### Bootup Sequence (2 stages)

```
1. Starting...
   (empty)

2. Ready!
   10.42.0.1:8080
   (stays 2 seconds)
```

### During Recording (2-page cycle, 3-second rotation)

**Page 1 - Progress + Mode:**
```
Rec: 45/240s
SVO2
```

**Page 2 - Progress + Settings:**
```
Rec: 45/240s
720@60 1/120
```

### Stopping Sequence

```
1. Stopping
   Recording...
   (immediate feedback)

2. Recording
   Stopped
   (stays 3 seconds)

3. Web Controller
   10.42.0.1:8080
   (idle state)
```

### Idle State

```
Web Controller
10.42.0.1:8080
```

---

## 📁 File Structure

### Recording Output

Each recording creates a timestamped directory:
```
/media/angelo/DRONE_DATA/flight_YYYYMMDD_HHMMSS/
├── video.svo2              # Compressed stereo video
├── sensor_data.csv         # IMU and camera metadata
├── recording.log           # Recording session log
└── depth_data/             # (Optional) Depth information
    ├── frame_0000.dat      # Raw 32-bit depth (DEPTH_INFO mode)
    └── depth_0000.png      # Depth visualization (DEPTH_IMAGES mode)
```

### Important Locations

```
Drone-Fieldtest/
├── apps/
│   ├── drone_web_controller/     # Main application
│   ├── smart_recorder/           # Profile-based recorder
│   ├── live_streamer/            # Network streaming (WIP)
│   └── performance_test/         # System benchmarking
├── common/
│   ├── hardware/
│   │   ├── zed_camera/          # ZED SDK integration
│   │   └── lcd_display/         # LCD I2C interface
│   ├── storage/                 # USB storage management
│   └── utils/                   # Shared utilities
├── build/                       # Compiled binaries
├── scripts/                     # Helper scripts
├── systemd/                     # Service files
└── docs/                        # Documentation
```

---

## ⚙️ Configuration

### Recording Profiles

**Standard Profile** (default):
- Duration: 240 seconds (4 minutes)
- Resolution: HD720
- Frame Rate: 60 FPS
- Shutter Speed: 1/120 (auto-set)
- Expected File Size: ~6.6 GB

**Custom Settings:**
- Adjust via web interface
- Changes persist during session
- Reset on restart (reverts to defaults)

### Network Configuration

**Access Point:**
- SSID: `DroneController`
- Password: `drone123`
- IP Address: `192.168.4.1`
- DHCP Range: `192.168.4.2-192.168.4.20`

**Dual Network Mode:**
- Ethernet: Internet access (if connected)
- WiFi: Control interface (isolated)

### System Service

**Autostart on Boot:**
```bash
sudo systemctl enable drone-recorder
```

**Manual Control:**
```bash
sudo systemctl start drone-recorder   # Start
sudo systemctl stop drone-recorder    # Stop
sudo systemctl status drone-recorder  # Check status
```

**View Logs:**
```bash
sudo journalctl -u drone-recorder -f       # Follow live
sudo journalctl -u drone-recorder --since "10 minutes ago"
```

---

## 🔧 Technical Details

### Camera Specifications

**ZED 2i Configuration:**
- Baseline: 120mm
- Field of View: 110° (H) × 70° (V)
- Depth Range: 0.2m - 20m
- Supported Resolutions: VGA to 2.2K
- Supported Frame Rates: 15 - 100 FPS

**Recording Format:**
- SVO2: H.264/H.265 compressed
- Lossless mode: No hardware encoder on Orin Nano
- Write Speed: 50-200 MB/s depending on mode

### Shutter Speed / Exposure

**Conversion Formula:**
```
Exposure% = (FPS / shutter_denominator) × 100
```

**Examples at 60 FPS:**
- 1/60 = 100% exposure (maximum at 60 FPS)
- 1/120 = 50% exposure (default, 2× shutter rule)
- 1/240 = 25% exposure (fast action)

**FPS-Aware Behavior:**
- Exposure percentage preserved across FPS changes
- Shutter speed recalculates automatically
- Example: 60 FPS @ 1/120 (50%) → 30 FPS @ 1/60 (50%)

### Storage Requirements

**File Size Estimates (4-minute recording):**
- SVO2 Only: ~6.6 GB (HD720@60fps)
- SVO2 + Depth Info: ~8-10 GB (adds 32-bit depth)
- SVO2 + Depth PNG: ~12-15 GB (adds PNG images)
- RAW Frames: ~20-30 GB (separate images)

**Filesystem Requirements:**
- NTFS or exFAT (supports files >4GB)
- FAT32: Auto-stops at ~3.75GB to prevent corruption
- Minimum USB 3.0 recommended for high FPS

### Performance Benchmarks

**HD720 @ 60 FPS:**
- Write Speed: ~160 MB/s
- CPU Usage: ~30%
- Memory: ~2 GB
- No frame drops

**HD1080 @ 30 FPS:**
- Write Speed: ~120 MB/s
- CPU Usage: ~25%
- Memory: ~2 GB
- No frame drops

**VGA @ 100 FPS:**
- Write Speed: ~200 MB/s
- CPU Usage: ~35%
- Memory: ~2.5 GB
- Occasional drops on USB 2.0

---

## 🛡️ Error Handling & Recovery

### Automatic Recovery

**USB Storage:**
- Auto-detection every 5 seconds
- Retry on mount failure
- Graceful degradation if unavailable

**Camera:**
- Init timeout: 30 seconds
- Automatic retry on failure
- Clear error messages on LCD/GUI

**WiFi Access Point:**
- Health check every 5 seconds
- Auto-restart after 3 consecutive failures
- Fallback to ethernet if available

### Manual Recovery

**Camera Not Detected:**
```bash
# Check ZED connection
lsusb | grep Stereolabs

# Restart ZED daemon
sudo systemctl restart zed-daemon

# Re-run application
sudo systemctl restart drone-recorder
```

**LCD Not Working:**
```bash
# Check I2C
i2cdetect -y 7  # Should show device at 0x27

# Test LCD
./build/tools/lcd_display_tool

# System continues without LCD if not present
```

**Storage Issues:**
```bash
# Check USB mount
lsblk | grep DRONE_DATA

# Remount manually
sudo mount /dev/sdX1 /media/angelo/DRONE_DATA

# Check filesystem
sudo fsck /dev/sdX1
```

---

## 🐛 Known Issues & Limitations

### Hardware Limitations

1. **No NVENC Support**
   - Jetson Orin Nano lacks hardware video encoder
   - Workaround: Use lossless SVO2 mode (software compression)
   - Impact: Slightly higher CPU usage, larger files

2. **USB Throughput**
   - USB 2.0 may drop frames at 100 FPS
   - Recommendation: Use USB 3.0 drive
   - Monitor: Check write speed in GUI

3. **LCD I2C Address**
   - Some LCD modules use 0x3F instead of 0x27
   - Fix: Modify `lcd_i2c.cpp` if needed
   - Check: Run `i2cdetect -y 7`

### Software Considerations

1. **File Size**
   - FAT32: Auto-stops at ~3.75GB (4GB limit protection)
   - Solution: Use NTFS or exFAT
   - Verification: Check filesystem before recording

2. **Recording Duration**
   - Current limit: 9999 seconds (~2.7 hours)
   - Reason: LCD display format constraint
   - Workaround: Increase `recording_duration_seconds_` in code

3. **WiFi Compatibility**
   - Some devices may not support 2.4GHz-only AP
   - Workaround: Use 5GHz-capable network adapter
   - Current: 2.4GHz for maximum compatibility

---

## 📊 Version History

### v1.3 Stable (November 17, 2025)

**Major Features:**
- ✅ Complete web-based controller with mobile UI
- ✅ Professional shutter speed controls (photography notation)
- ✅ WiFi Access Point with automatic management
- ✅ LCD status display with 2-page rotation
- ✅ Multiple recording modes (SVO2, SVO2+Depth, RAW)
- ✅ Robust error handling and recovery
- ✅ FPS-aware exposure management

**Bug Fixes:**
- ✅ Fixed web server disconnect on recording stop (v1.3.4)
- ✅ Removed "Remaining:" text artifacts (v1.3.3)
- ✅ Fixed LCD stopping message visibility (v1.3.5)
- ✅ Fixed thread deadlock in recording monitor
- ✅ Fixed slider initialization position
- ✅ Fixed high shutter speed values (extended range)

**Improvements:**
- ✅ Streamlined LCD bootup (2 stages instead of 4)
- ✅ 3-second LCD refresh (was 1 second)
- ✅ Immediate "Stopping..." feedback
- ✅ 12 shutter speed positions (was 10)
- ✅ Minimum exposure capped at 5% (was 6%)
- ✅ "Recording Stopped" stays visible for 3 seconds

### Previous Versions

- **v1.2:** Gap-free recording, 4GB FAT32 protection
- **v1.1:** Smart recorder with recording profiles
- **v1.0:** Basic SVO2 recording functionality

---

## 📚 Additional Documentation

### 🎓 NEW DEVELOPER ONBOARDING (START HERE!)

**`docs/CRITICAL_LEARNINGS_v1.3.md`** - **REQUIRED READING**
- Complete development journey and lessons learned
- Detailed analysis of all major problems and solutions
- Common mistakes to avoid
- Testing methodology
- Performance baselines
- 5-day onboarding checklist
- **READ THIS FIRST to avoid repeating solved problems!**

### Comprehensive Guides

1. **Project Architecture** (`Project_Architecture`)
   - System design overview
   - Component interactions
   - Data flow diagrams

2. **Recording Modes** (`docs/Recording_Duration_Guide.md`)
   - Mode comparison
   - Performance characteristics
   - Use case recommendations

3. **Shutter Speed System** (`docs/SHUTTER_SPEED_UI_v1.3.md`)
   - Conversion formulas
   - Implementation details
   - Testing procedures

4. **LCD Display** (`docs/LCD_FINAL_v1.3.2.md`)
   - Display formats
   - Update cycles
   - Character count validation

5. **External Dependencies** (`EXTERNAL_FILES_DOCUMENTATION.md`)
   - Systemd units
   - Network configuration
   - sudoers rules

### Problem Analysis & Solutions

6. **4GB Filesystem Issue** (`4GB_SOLUTION_FINAL_STATUS.md`)
   - Root cause: FAT32 limitation, not ZED SDK
   - Solution: NTFS/exFAT validation
   - Testing results (up to 9.9 GB validated)

7. **NVENC Hardware Encoder** (`NVENC_INVESTIGATION_RESULTS.md`)
   - Why H.264/H.265 don't work on Jetson
   - LOSSLESS is the only reliable mode
   - File size implications

8. **Thread Deadlock Fix** (`docs/WEB_DISCONNECT_FIX_v1.3.4.md`)
   - Web GUI disconnect analysis
   - `continue` vs `break` in monitor loops
   - Thread interaction best practices

9. **LCD Thread Conflicts** (`docs/LCD_FINAL_FIX_v1.3.3.md`)
   - Multiple threads updating LCD
   - Thread ownership model
   - State-based LCD control

### Technical References

- **ZED SDK Documentation:** https://www.stereolabs.com/docs/
- **CUDA Toolkit:** https://developer.nvidia.com/cuda-toolkit
- **Jetson Orin Nano:** https://developer.nvidia.com/embedded/jetson-orin-nano-devkit

---

## 🤝 Contributing

### Development Setup

```bash
# Create development branch
git checkout -b feature/your-feature

# Make changes and test
./scripts/build.sh
sudo ./build/apps/drone_web_controller/drone_web_controller

# Commit changes
git add .
git commit -m "Description of changes"
git push origin feature/your-feature
```

### Code Style

- C++17 standard
- Google C++ style guide
- Clear comments for complex logic
- Error handling for all I/O operations

### Testing

- Build without warnings
- Test all recording modes
- Verify LCD display
- Check web interface
- Monitor for memory leaks

---

## 📞 Support & Troubleshooting

### Common Issues

**Issue:** Web GUI not accessible
- Check WiFi connection to `DroneController`
- Verify IP address: `ip addr show wlan0`
- Check service status: `sudo systemctl status drone-recorder`

**Issue:** Recording not starting
- Check USB storage: `lsblk | grep DRONE_DATA`
- Verify ZED camera: `lsusb | grep Stereolabs`
- Check logs: `sudo journalctl -u drone-recorder -f`

**Issue:** Frame drops during recording
- Use USB 3.0 storage device
- Reduce resolution or FPS
- Check write speed in GUI during recording
- Monitor with: `iostat -x 1`

### Debug Mode

```bash
# Run with verbose output
sudo ./build/apps/drone_web_controller/drone_web_controller --verbose

# Monitor system resources
htop  # CPU/Memory
iotop  # Disk I/O
```

### Logs Location

```bash
# Systemd service logs
sudo journalctl -u drone-recorder --since today

# Application logs (if manual start)
Check terminal output

# Recording logs
/media/angelo/DRONE_DATA/flight_*/recording.log
```

---

## 📄 License

This project is developed for field testing and research purposes.

---

## ✨ Acknowledgments

- **Stereolabs** for ZED SDK and camera hardware
- **NVIDIA** for Jetson platform and CUDA support
- **Open Source Community** for libraries and tools

---

## 🎯 Roadmap

### Planned Features (v1.4+)

- [ ] Live streaming over WiFi network
- [ ] Real-time object detection integration
- [ ] GPS/IMU data logging
- [ ] Multi-camera synchronization
- [ ] Cloud upload automation
- [ ] Advanced depth processing modes

### Potential Improvements

- [ ] Web UI themes (dark mode)
- [ ] Custom recording profiles in GUI
- [ ] Automatic video extraction to frames
- [ ] Email/SMS notifications on events
- [ ] Remote monitoring dashboard

---

## 📝 Notes

### Field Testing Tips

1. **Pre-Flight Checklist:**
   - ✅ USB storage inserted and labeled
   - ✅ ZED camera connected
   - ✅ Battery fully charged
   - ✅ Service running and accessible
   - ✅ Test recording for 10 seconds

2. **During Operation:**
   - Monitor LCD for recording status
   - Check web GUI for write speed
   - Keep backup USB drive ready
   - Note any error messages

3. **Post-Flight:**
   - Safely unmount USB: `sudo umount /media/angelo/DRONE_DATA`
   - Verify recording files exist
   - Check file sizes are expected
   - Backup data immediately

### Performance Optimization

1. **For Maximum Speed (100 FPS):**
   - Use VGA resolution
   - SVO2 Only mode (no depth)
   - USB 3.0 storage
   - Disable other services

2. **For Maximum Quality:**
   - Use HD1080 or 2K resolution
   - Lower FPS (15-30)
   - SVO2 + Depth PNG for visualization
   - Fast SD card or SSD

3. **For Balanced Performance (Recommended):**
   - HD720 @ 60 FPS
   - SVO2 Only or SVO2 + Depth Info
   - 1/120 shutter speed
   - USB 3.0 flash drive

---

**System Status:** ✅ STABLE - Ready for field deployment
**Last Updated:** November 17, 2025
**Version:** 1.3 Stable
**Maintainer:** Angelo (xX2Angelo8Xx)

---

*For detailed technical documentation, see individual files in `/docs/` directory.*
