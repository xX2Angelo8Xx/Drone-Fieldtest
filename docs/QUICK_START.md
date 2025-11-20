# 🚁 Drone Fieldtest - Quick Start Guide

**Current Version:** v1.5.4 (Stable)  
**Last Updated:** November 2025

## 🚀 Quick Commands

### Start the System
```bash
# Easiest way (if drone alias configured):
drone

# Using startup script:
./drone_start.sh

# Manual start:
cd /home/angelo/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller
```

### Build Project
```bash
./scripts/build.sh
```

## 🌐 Connection Info

- **WiFi SSID:** `DroneController`
- **WiFi Password:** `drone123`
- **Web UI:** http://192.168.4.1:8080 or http://10.42.0.1:8080

## 📊 Monitoring & Logs

```bash
# View service logs (live):
sudo journalctl -u drone-recorder -f

# Check service status:
systemctl --user status drone-recorder

# Check USB storage:
ls -lh /media/angelo/DRONE_DATA/
```

## 🎥 Recording Modes

The web UI provides 4 recording modes:
- **SVO2** - Compressed video only (~6.6GB per 4min @ HD720@30fps)
- **SVO2_DEPTH_INFO** - Video + raw depth data files
- **SVO2_DEPTH_IMAGES** - Video + depth PNG visualizations
- **RAW_FRAMES** - Separate left/right/depth images (highest quality)

## 🔧 Camera Settings

- **Resolution/FPS:** HD720@30/60fps (default 60fps), HD1080@30fps, HD2K@15fps, VGA@100fps
- **Exposure:** -1=Auto, 0-100=Manual (50% @ 60fps = 1/120 shutter)
- **Depth Mode:** NEURAL_PLUS, NEURAL, ULTRA, QUALITY, PERFORMANCE

## 💾 USB Storage Requirements

- **Label:** `DRONE_DATA` (exact match required)
- **Filesystem:** NTFS or exFAT (FAT32 has 4GB limit)
- **Recommended Size:** ≥128GB
- **Auto-mount:** `/media/angelo/DRONE_DATA/`

## 🛠️ ZED Camera Tools

```bash
# Video Viewer (view recordings):
/usr/local/zed/tools/ZED_Explorer

# Camera diagnostics:
/usr/local/zed/tools/ZED_Diagnostic

# Live depth view:
/usr/local/zed/tools/ZED_Depth_Viewer

# IMU/Sensor viewer:
/usr/local/zed/tools/ZED_Sensor_Viewer
```

## 📁 Output Structure

Recordings are saved in timestamped folders:
```
/media/angelo/DRONE_DATA/flight_YYYYMMDD_HHMMSS/
├── video.svo2              # Main video file
├── sensor_data.csv         # IMU/temperature/barometer data
├── recording.log           # Recording metadata and stats
└── depth_data/             # (if depth recording enabled)
    ├── depth_XXXXX.bin     # or
    └── depth_XXXXX.png
```

## 🔄 Service Control

```bash
# Start service:
sudo systemctl start drone-recorder

# Stop service:
sudo systemctl stop drone-recorder

# Restart service:
sudo systemctl restart drone-recorder

# Enable autostart on boot:
sudo systemctl enable drone-recorder

# Disable autostart:
sudo systemctl disable drone-recorder
```

## 🐛 Troubleshooting

### No WiFi AP appearing
```bash
# Check NetworkManager connection:
nmcli connection show

# Cleanup and restart:
sudo nmcli connection down DroneController
sudo systemctl restart drone-recorder
```

### USB not detected
```bash
# Check if mounted:
lsblk | grep DRONE_DATA

# Check filesystem:
sudo blkid | grep DRONE_DATA

# If FAT32, reformat to NTFS or exFAT for >4GB files
```

### Camera not working
```bash
# Test camera with ZED Diagnostic:
/usr/local/zed/tools/ZED_Diagnostic

# Check camera connection:
ls /dev/video*

# View system logs:
sudo journalctl -u drone-recorder -n 100
```

## 📚 More Documentation

- **Full User Guide:** `docs/guides/USER_GUIDE.md`
- **Developer Guide:** `docs/guides/DEVELOPER_GUIDE.md`
- **Changelog:** `CHANGELOG.md`
- **Critical Learnings:** `docs/CRITICAL_LEARNINGS_v1.3.md`
- **Release Notes:** `docs/releases/RELEASE_v1.5.4_STABLE.md`

## ⚡ Performance Baselines

- **HD720@60fps LOSSLESS:** 25-28 MB/s, ~9.9GB per 4 min, 0 frame drops
- **HD720@30fps LOSSLESS:** 13 MB/s, ~6.6GB per 4 min
- **CPU Usage:** 45-60% (all cores) during recording
- **WiFi Range:** ~50m open field (2.4GHz AP)

---

**For detailed documentation, see:**
- User guide: `docs/guides/USER_GUIDE.md`
- Developer guide: `docs/guides/DEVELOPER_GUIDE.md`
- Architecture: `.github/copilot-instructions.md`
