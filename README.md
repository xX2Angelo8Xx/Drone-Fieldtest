# 🚁 Drone Field Test System

**Embedded drone field testing system for Jetson Orin Nano with ZED 2i stereo camera and wireless web control interface**

[![Version](https://img.shields.io/badge/version-v1.2--stable-brightgreen)](https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases)
[![Platform](https://img.shields.io/badge/platform-Jetson%20Orin%20Nano-orange)](https://developer.nvidia.com/embedded/jetson-orin-nano-developer-kit)
[![Camera](https://img.shields.io/badge/camera-ZED%202i-blue)](https://www.stereolabs.com/zed-2i/)

## 🎯 Project Overview

This system provides **wireless drone control** via smartphone with **real-time recording capabilities**. Perfect for AI training data collection, field testing, and autonomous drone operations.

### ✅ Core Features

- **📱 WiFi Web Control**: Phone-based interface with real-time status
- **📹 HD Video Recording**: Up to 9.95GB continuous recording (NTFS/exFAT)
- **🔄 Progress Monitoring**: Live file size, speed, elapsed time display
- **🚀 One-Click Autostart**: Desktop file-based autostart control
- **⚡ Quick Commands**: `drone` command starts system instantly
- **🔧 Field-Ready**: Robust WiFi reconnection and error recovery

## 🚀 Quick Start

### Terminal Commands
```bash
# Start drone system (simplest way)
drone

# Or use script
./drone_start.sh

# Check autostart status
drone-status
```

### WiFi Connection
1. **Network**: `DroneController`
2. **Password**: `drone123`  
3. **Web Interface**: `http://192.168.4.1:8080`

### Autostart Control
- **Enable**: Keep file `~/Desktop/Autostart`
- **Disable**: Rename to `~/Desktop/Autostart_DISABLED`

## 📋 System Requirements

### Hardware
- **Platform**: NVIDIA Jetson Orin Nano Developer Kit
- **Camera**: ZED 2i stereo camera  
- **Storage**: USB drive with `DRONE_DATA` label (**NTFS/exFAT required**)
- **Display**: I2C LCD (optional, `/dev/i2c-7`)
- **Network**: WiFi capable interface

### Software
- **OS**: Ubuntu 22.04 LTS (JetPack)
- **ZED SDK**: v4.1+ 
- **CUDA**: 12.6
- **CMake**: 3.16+
- **Dependencies**: hostapd, dnsmasq, NetworkManager

## 🏗️ Architecture

### Applications
- **`drone_web_controller`** ⭐ - Primary WiFi web interface
- **`smart_recorder`** - Multi-profile recorder (12+ modes)
- **`live_streamer`** - H.264/H.265 streaming with telemetry
- **`performance_test`** - Hardware validation
- **`zed_cli_recorder`** - CLI-based recording alternative

### Key Components
```
├── apps/                   # Main applications
├── common/                 # Shared libraries
│   ├── hardware/          # ZED camera, LCD display
│   ├── storage/           # USB auto-detection
│   └── streaming/         # H.264/265 streaming
├── scripts/               # Build and utility scripts
├── systemd/               # Service configuration
└── tools/                 # SVO extraction utilities
```

## 📱 Web Interface Features

### Modern UI
- **Responsive Design**: Mobile-optimized interface
- **Real-Time Progress**: Live recording statistics
- **Visual Feedback**: Progress bars, file size, transfer speed
- **Error Handling**: Connection error detection with red indicators

### Recording Controls
- **Start/Stop Recording**: One-click operation
- **Auto-Stop**: 4-minute default duration with countdown
- **Manual Override**: Stop recording at any time
- **System Shutdown**: Safe shutdown with cleanup

### Status Information
- **Recording Time**: Elapsed and remaining time
- **File Size**: Real-time size in GB
- **Transfer Speed**: MB/s recording rate
- **File Name**: Current recording filename
- **Progress**: Visual progress bar (0-100%)

## 🔧 Installation & Setup

### 1. Clone Repository
```bash
git clone https://github.com/xX2Angelo8Xx/Drone-Fieldtest.git
cd Drone-Fieldtest
```

### 2. Build System
```bash
./scripts/build.sh
```

### 3. Install Autostart Service
```bash
./scripts/install_service.sh
```

### 4. Setup Passwordless Sudo (for WiFi control)
```bash
# Creates sudoers file for drone operations
sudo visudo -f /etc/sudoers.d/drone-controller
```

### 5. Enable Autostart
```bash
# Autostart control file (must exist for autostart)
touch ~/Desktop/Autostart
```

## 📊 Recording Capabilities

### File Formats
- **Video**: `.svo2` (ZED native format)
- **Sensors**: `.csv` (IMU, temperature, timestamps)
- **Logs**: `.log` (system events)

### Recording Modes
- **HD720@30fps**: Default production mode (4min ≈ 6.6GB)
- **HD720@15fps**: Frame-drop free mode (14.1 MB/s)
- **HD1080@30fps**: High quality for AI training

### Storage Requirements
- **Filesystem**: NTFS or exFAT (**required** for >4GB files)
- **Label**: USB drive must be labeled `DRONE_DATA`
- **Capacity**: 32GB+ recommended for multiple flights

## 🌐 Network Architecture

### WiFi Access Point Mode
- **SSID**: `DroneController`
- **Security**: WPA2-PSK 
- **Channel**: 6 (2.4GHz)
- **DHCP Range**: 192.168.4.2-20
- **Gateway**: 192.168.4.1

### Dual Network Setup
- **Ethernet**: Internet connectivity maintained
- **WiFi AP**: Isolated drone control network
- **Automatic**: Home WiFi disconnection during operation

## 🔄 Autostart System

### Desktop File Control
The system uses a visual Desktop file to control autostart:

**File Present** → Autostart **ENABLED** ✅
```bash
~/Desktop/Autostart  # System starts automatically on boot
```

**File Missing** → Autostart **DISABLED** ❌
```bash
~/Desktop/Autostart_DISABLED  # Normal boot, no autostart
```

### Service Management
```bash
# Check status
sudo systemctl status drone-recorder

# Manual start/stop
sudo systemctl start drone-recorder
sudo systemctl stop drone-recorder

# View logs
sudo journalctl -u drone-recorder -f
```

## 🐛 Troubleshooting

### Common Issues

**WiFi Network Not Visible**
- Check if home WiFi is disconnected
- Restart with: `sudo systemctl restart drone-recorder`

**Recording Fails**
- Verify USB drive labeled `DRONE_DATA`
- Ensure NTFS/exFAT filesystem (not FAT32)
- Check available storage space

**Web Interface Unresponsive**
- Connect to `DroneController` WiFi first
- Navigate to `http://192.168.4.1:8080`
- Clear browser cache if needed

**Camera Not Detected**
- Check ZED camera USB connection
- Kill existing processes: `sudo pkill -f drone_web_controller`
- Restart system if needed

### Debug Commands
```bash
# Check autostart status
./scripts/autostart_control.sh

# Manual WiFi setup
./scripts/wifi_disconnect.sh

# Build system
./scripts/build.sh

# Check storage
ls -lh /media/angelo/DRONE_DATA/
```

## 📁 Project Files Outside Repository

### Critical System Files
These files are created outside the project directory and are essential for operation:

#### `/home/angelo/Desktop/Autostart`
**Purpose**: Visual autostart control  
**Content**: Instructions and status information  
**Critical**: System checks this file to determine autostart behavior

#### `/etc/sudoers.d/drone-controller`
**Purpose**: Passwordless sudo for WiFi operations  
**Content**: sudo permissions for hostapd, dnsmasq, ip commands  
**Critical**: Required for WiFi hotspot functionality without password prompts

#### `/home/angelo/.bashrc` (appended)
**Purpose**: Quick command aliases  
**Content**: `drone` and `drone-status` aliases  
**Enhancement**: Convenience commands for terminal operation

#### `/etc/systemd/system/drone-recorder.service`
**Purpose**: Autostart service configuration  
**Content**: systemd service definition  
**Critical**: Enables automatic startup on boot

### File Relationships
```
Boot Process:
├── systemd checks: /etc/systemd/system/drone-recorder.service
├── Service runs: ~/Projects/Drone-Fieldtest/apps/drone_web_controller/autostart.sh
├── Script checks: ~/Desktop/Autostart (file existence)
├── WiFi operations use: /etc/sudoers.d/drone-controller (passwordless sudo)
└── Terminal aliases from: ~/.bashrc (drone commands)
```

## 🏷️ Version History

### v1.2-stable (Current)
- ✅ Complete WiFi web controller functionality
- ✅ Desktop autostart control system
- ✅ Passwordless sudo configuration
- ✅ 9.95GB continuous recording capability
- ✅ Modern web UI with progress monitoring
- ✅ Quick terminal commands (`drone`, `drone-status`)

### v1.1-stable  
- ✅ Fixed WiFi stability and shutdown deadlocks
- ✅ Improved UI without emoji display issues
- ✅ Thread safety improvements

### v1.0-baseline
- ✅ Basic WiFi hotspot functionality
- ✅ Core recording capabilities
- ✅ Initial web interface

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