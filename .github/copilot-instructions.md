# Copilot Instructions for Drone Field Test System

## Architecture Overview

This is an **embedded drone field testing system** targeting **Jetson Orin Nano** with ZED 2i stereo camera for autonomous drone AI training data collection. The system uses a modular C++17 architecture with WiFi web control interface and shared hardware abstraction layers.

### Core Applications
- **`drone_web_controller`**: **PRIMARY** - Complete WiFi web interface with autostart control (v1.2-stable)
- **`smart_recorder`**: Multi-profile recorder with 12+ AI-optimized modes including "standard" (4min HD720@30fps)
- **`data_collector`**: Legacy autostart recorder (replaced by web controller) 
- **`performance_test`**: Hardware validation and diagnostics  
- **`live_streamer`**: Real-time H.264/H.265 streaming with telemetry overlay (2-12 Mbps bandwidth)
- **`zed_cli_recorder`**: ZED Explorer backend wrapper (alternative for maximum stability)

### Key Components
- **Web Control Interface**: WiFi hotspot + HTML5 web UI for phone-based drone control (192.168.4.1:8080)
- **Desktop Autostart Control**: Visual ~/Desktop/Autostart file for autostart enable/disable
- **Quick Commands**: `drone` and `drone-status` terminal aliases for instant access
- **Passwordless Sudo**: /etc/sudoers.d/drone-controller for WiFi operations
- **SystemD Integration**: Auto-boot service with proper user permissions
- **Network Architecture**: Ethernet for internet, WiFi AP for drone control (dual-interface setup)
- **Storage**: USB auto-detection with `DRONE_DATA` label, **NTFS/exFAT required** for >4GB files
- **ZED Camera**: Production-ready SDK integration with **unlimited file size support**
- **LCD Display**: I2C-based field status display (`/dev/i2c-7`) with 16-char width formatting + backup control
- **SVO Extraction**: Frame extraction tool creates organized JPEG directories (`extracted_images/`)

## Essential Development Patterns

### 4GB File Size Issue - RESOLVED ✅
**ROOT CAUSE**: FAT32 filesystem limitation, not ZED SDK corruption.
**SOLUTION**: Use NTFS or exFAT formatted USB drives - no file size limits.
**PROTECTION**: Auto-stop at 3.75GB to prevent SVO2 format corruption at 4GB boundary.

```bash
# Format USB to NTFS (recommended)
sudo mkfs.ntfs -f -L DRONE_DATA /dev/sda1

# Result: 8GB+ recordings work perfectly
# Example: 5-minute test = 9.9GB successful recording
# Protection: Auto-stops at 3.75GB if approaching 4GB corruption point
```

### Recording Mode System & Profile Architecture
Use enum-based modes with profile system in `smart_recorder`:
```cpp
// Standard modes for different use cases
RecordingMode::HD720_15FPS    // No frame drops, field-proven (14.1 MB/s)
RecordingMode::HD720_30FPS    // Default production mode
RecordingMode::HD1080_30FPS   // AI training quality

// Profile-based execution
./smart_recorder realtime_light    # 30s HD720@15fps field deployment
./smart_recorder training          # 60s HD1080@30fps for AI models
./smart_recorder long_segmented_test # Auto-segments at 3GB boundary
```

### Wireless Control System  
Phone-based web interface with WiFi hotspot for field operation:
```bash
# Start web controller (creates WiFi AP + web server)
sudo ./build/apps/drone_web_controller/drone_web_controller

# Network Configuration:
# WiFi AP: "DroneController" / Password: "drone123" 
# Web UI: http://192.168.4.1:8080
# Internet: Available via Ethernet (maintains connectivity)
```

### Continuous Recording System
Single-file continuous recording supports unlimited file sizes on NTFS/exFAT:
```cpp
// Continuous recording - no segmentation needed with NTFS/exFAT
// Files can exceed 4GB without corruption (tested up to 6.6GB+)
// Example: 4-minute HD720@30fps = ~6.6GB single file
```

### Signal Handling Pattern
All applications use identical global signal handlers for clean shutdown:
```cpp
// Global pointers for signal cleanup (exact pattern across all apps)
ZEDRecorder* g_recorder = nullptr;
StorageHandler* g_storage = nullptr;
bool g_running = true;

void signalHandler(int signal) {
    g_running = false;  // Triggers main loop exit
}
```

### Build & Deploy Workflow
```bash
# Standard build process
./scripts/build.sh                           # Builds all targets
sudo systemctl restart drone-recorder        # Deploy to autostart
sudo journalctl -u drone-recorder -f         # Monitor field deployment
```

### USB Storage Convention
- **Label**: `DRONE_DATA` (hardcoded expectation)
- **Structure**: Auto-creates `flight_YYYYMMDD_HHMMSS/` directories
- **Files**: `video.svo2`, `sensor_data.csv`, `recording.log`
- **Continuous**: Single files can exceed 4GB (tested to 6.6GB+) on NTFS/exFAT

## Hardware-Specific Considerations

### ZED Camera Integration
- **Frame drops**: Use `HD720_15FPS` mode (14.1 MB/s) to eliminate drops completely
- **File format**: Always `.svo2` (ZED's native format) 
- **GPU dependency**: CUDA 12.6, requires `/usr/local/cuda-12.6/include`
- **Compression**: LOSSLESS mode only (Jetson Orin Nano has no NVENC support)

### LCD Display (I2C)
- **Device**: `/dev/i2c-7` (Jetson-specific)
- **Format**: 16-char width limit, use `formatPath()` for truncation
- **Failure handling**: Non-blocking - system continues without LCD

### Systemd Service Pattern
- **User**: `angelo` (not root) with environment variables
- **Startup delay**: 10-second delay for hardware initialization
- **Working directory**: Must be project root for relative paths

## Profile-Based Recording System

The `smart_recorder` uses profile arguments for different AI training scenarios:

```bash
sudo ./smart_recorder realtime_light    # Field deployment (30s, HD720@15fps)
sudo ./smart_recorder training          # AI training (60s, HD1080@30fps) 
sudo ./smart_recorder development       # Dev testing (20s, HD720@60fps)
```

## Critical File Paths
- Build outputs: `build/apps/{app_name}/{app_name}`
- Autostart script: `apps/data_collector/autostart.sh`
- Service file: `systemd/drone-recorder.service`
- ZED tools: `/usr/local/zed/tools/ZED_Explorer` (video viewer)
- SVO extractor: `build/tools/svo_extractor` (frame extraction to JPEG)
- Extracted images: `extracted_images/flight_YYYYMMDD_HHMMSS_video/`

## Advanced Features & Debugging

### Continuous Recording Verification
```bash
# Test large file recording capability
sudo ./build/apps/smart_recorder/smart_recorder standard  # 4min = ~6.6GB
# Results: Single continuous file, no corruption on NTFS/exFAT USB drives
```

### SVO Frame Extraction
```bash
# Extract frames from SVO files
./build/tools/svo_extractor /path/to/video.svo2 10  # Every 10th frame
# Output: extracted_images/flight_20241027_154429_video/frame_*.jpg
```

### Live Streaming
```bash
# Start streaming with telemetry overlay
sudo ./build/apps/live_streamer/live_streamer
# H.264/H.265 streaming at HD720@15fps with bandwidth optimization
```

### Common Debugging Commands
```bash
# Check recording status
ls -lt /media/angelo/DRONE_DATA/flight_* | head -5

# View recordings
/usr/local/zed/tools/ZED_Explorer /path/to/video.svo2

# Service diagnostics
sudo systemctl status drone-recorder
sudo journalctl -u drone-recorder --since "5 minutes ago"

# Extract training images
./build/tools/svo_extractor /media/angelo/DRONE_DATA/flight_*/video.svo2
```

## Key Implementation Patterns

### Continuous Recording (No Size Limits)
```cpp
// Continuous recording with NTFS/exFAT support - no 4GB limitations
// Files grow continuously without segmentation interruptions
// Tested: 4-minute HD720@30fps = 6.6GB single file successfully
```

### Streaming Architecture
- **Bandwidth optimization**: H.264/H.265 compression (1-10 Mbps for stereo)
- **Telemetry overlay**: Real-time battery, altitude, speed display
- **Quality profiles**: High (6-10 Mbps), Medium (2-4 Mbps), Low (1-2 Mbps)

### Error Recovery Patterns
- **Non-blocking LCD**: System continues if I2C display fails
- **USB detection**: Retries every 5 seconds until `DRONE_DATA` found
- **Graceful degradation**: Recording continues even with component failures

When modifying this system, maintain the USB auto-detection reliability, preserve the profile-based recording architecture, and ensure all changes work with the systemd autostart deployment model.