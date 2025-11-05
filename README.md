# Drone Field Testing System

This is an embedded drone field testing system targeting Jetson Orin Nano with ZED 2i stereo camera for autonomous drone AI training data collection.

## Architecture Overview

The system uses a modular C++17 architecture with four main applications and shared hardware abstraction layers:

### Core Applications
- **`drone_web_controller`**: **PRIMARY** - Web-based drone control via WiFi hotspot with phone interface (NEW DEFAULT)
- **`smart_recorder`**: Multi-profile recorder with 12+ AI-optimized modes including "standard" (4min HD720@30fps)
- **`data_collector`**: Legacy autostart recorder (replaced by web controller) 
- **`performance_test`**: Hardware validation and diagnostics  
- **`live_streamer`**: Real-time H.264/H.265 streaming with telemetry overlay (2-12 Mbps bandwidth)
- **`zed_cli_recorder`**: ZED Explorer backend wrapper (alternative for maximum stability)

### Key Features
- **Web Control Interface**: WiFi hotspot + HTML5 web UI for phone-based drone control (192.168.4.1:8080)
- **Network Architecture**: Ethernet for internet, WiFi AP for drone control (dual-interface setup)
- **Storage**: USB auto-detection with `DRONE_DATA` label, **NTFS/exFAT required** for >4GB files
- **ZED Camera**: Production-ready SDK integration with **unlimited file size support**
- **LCD Display**: I2C-based field status display (`/dev/i2c-7`) with 16-char width formatting
- **SVO Extraction**: Frame extraction tool creates organized JPEG directories (`extracted_images/`)

## Quick Start

```bash
# Build all applications
./scripts/build.sh

# Start web controller (creates WiFi AP + web server)
sudo ./build/apps/drone_web_controller/drone_web_controller

# Access web interface:
# WiFi: Connect to "DroneController" (password: drone123)
# Browser: http://192.168.4.1:8080
```

## System Requirements

- **Platform**: Jetson Orin Nano (ARM64)
- **Camera**: ZED 2i stereo camera
- **Storage**: USB drive with `DRONE_DATA` label (NTFS/exFAT format)
- **Network**: WiFi interface (wlP1p1s0) + Ethernet
- **Dependencies**: ZED SDK v4.1+, CUDA 12.6

## Key Technical Achievements

### 4GB File Size Issue - RESOLVED ✅
**ROOT CAUSE**: FAT32 filesystem limitation, not ZED SDK corruption.
**SOLUTION**: Use NTFS or exFAT formatted USB drives - no file size limits.
**RESULT**: 8GB+ recordings work perfectly (tested up to 9.9GB).

### Continuous Recording
- Single-file continuous recording supports unlimited file sizes on NTFS/exFAT
- Files can exceed 4GB without corruption
- Example: 4-minute HD720@30fps = ~6.6GB single file

### Wireless Control System  
- Phone-based web interface with WiFi hotspot for field operation
- Network: WiFi AP "DroneController" / Password: "drone123" 
- Web UI: http://192.168.4.1:8080
- Internet: Available via Ethernet (maintains connectivity)

## Build & Deploy

```bash
# Standard build process
./scripts/build.sh

# Deploy to autostart service
sudo systemctl restart drone-recorder

# Monitor field deployment
sudo journalctl -u drone-recorder -f
```

## File Structure

```
apps/
├── drone_web_controller/    # Primary web-based control application
├── smart_recorder/          # Multi-profile recording system
├── data_collector/          # Legacy autostart recorder
├── performance_test/        # Hardware validation
├── live_streamer/          # Real-time streaming
└── zed_cli_recorder/       # ZED Explorer wrapper

common/
├── hardware/               # Hardware abstraction layer
├── storage/               # USB storage management
├── streaming/             # Video streaming utilities
└── utils/                 # Shared utilities

systemd/
└── drone-recorder.service  # Production autostart configuration
```

## Recording Modes

The `smart_recorder` uses profile arguments for different scenarios:

```bash
sudo ./smart_recorder realtime_light    # Field deployment (30s, HD720@15fps)
sudo ./smart_recorder training          # AI training (60s, HD1080@30fps) 
sudo ./smart_recorder development       # Dev testing (20s, HD720@60fps)
```

## Version History

- **v1.0**: Web controller with WiFi hotspot, unlimited file size recording
- **v0.9**: Auto-segmentation removal, NTFS support
- **v0.8**: Multi-profile recording system
- **v0.7**: ZED SDK v4.1+ integration

## License

This project is designed for embedded drone field testing applications.