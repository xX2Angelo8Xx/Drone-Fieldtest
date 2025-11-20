# Developer Guide

Last updated: 2025-11-20  
Target platform: Jetson Orin Nano + ZED 2i  
Languages: C++17, Bash  
Build: CMake + Jetson toolchain

## 1. System Overview

The Drone Field Test System is a producer‑grade recorder and controller for ZED 2i on Jetson. It exposes a WiFi AP + HTTP UI to control recording modes and monitors hardware (USB, battery, LCD).

Core apps:
- apps/drone_web_controller (production) – WiFi AP, HTTP UI, recording control
- apps/smart_recorder – CLI multi‑profile recorder
- apps/performance_test – hardware validation
- apps/zed_cli_recorder – small wrapper around ZED tools
- apps/live_streamer – H.26x streaming + telemetry (kept as‑is)

Shared libs (common/):
- hardware/zed_camera – ZED recorder and raw frame capture
- hardware/lcd_display – I2C 16x2 LCD (optional)
- hardware/battery – INA219 voltage/current + filtering
- storage – USB mount detection + filesystem validation
- networking – Safe hotspot manager via NetworkManager
- utils – helpers, logging, time, formatting

## 2. Dependencies

Runtime:
- ZED SDK 4.x, CUDA 12.x
- NetworkManager + nmcli (AP control)
- Linux I2C, INA219 sensor
- Filesystems: NTFS/exFAT (FAT32 tolerated with 3.75GB cap)

Build:
- CMake ≥3.16, GCC ≥9, JetPack (Ubuntu 22.04)

## 3. Repository Layout

```
apps/                  # Applications (controllers, tools)
common/                # Libraries (hardware, storage, networking, utils)
docs/                  # Developer docs, guides, releases, archives
scripts/               # Build/deploy/diagnostics
systemd/               # Unit files
tests/                 # camera, hardware, integration
tools/                 # viewers, extractors, utilities
```

## 4. Critical Design Patterns (do not break)

- Network safety (AP) – use NetworkManager transient profiles only. Never persistently modify /etc or disable NetworkManager. Crash must restore Ethernet automatically.
- Dual shutdown flags – separate app stop from system power‑off. Never join threads from inside themselves.
- Monitor loop exit – use break on STOPPING; do not continue.
- Completion flags – wait on explicit flags with timeout; avoid fixed sleeps; call sync() before exit.
- State transition – move to IDLE before setting completion flags.
- LCD ownership – a single owner per state (IDLE/system monitor; RECORDING/recording loop; STOPPING/stopRecording; STOPPED/short grace period).
- ZED compression – LOSSLESS only (no NVENC on Orin Nano); H264/H265 will fail.
- Depth mode – auto‑managed by recording mode (NONE for SVO2, NEURAL_PLUS when depth is requested); reinit camera safely when toggling.
- CORRUPTED_FRAME – treat as warning, not fatal. Save frame and reset failure counters.

See docs/CRITICAL_LEARNINGS_v1.3.md for full rationale and examples.

## 5. Application Architecture (drone_web_controller)

Threads/components:
- Main thread: boot, signal handling, high‑level state
- Web server thread: accepts HTTP requests, sets flags (never blocks)
- Recording monitor thread: periodic updates, owns LCD while recording
- Storage watcher: detects/remounts USB media
- Battery thread (optional): INA219 sampling/filtering

Core state machine:
- IDLE → startRecording() → RECORDING → stopRecording() → STOPPING → IDLE
- STOPPING is short; monitor loops break immediately on STOPPING.

HTTP API (examples):
- GET /api/status – JSON status (state, file, size, fps, exposure, depth)
- POST /api/start – set start flag, main loop executes startRecording()
- POST /api/stop – initiate stopRecording(), wait for completion flag
- POST /api/shutdown – set system_shutdown flag; main exits and halts system

Lifecycle rules:
- Request handlers set flags only. They do not perform blocking cleanup or joins.
- handleShutdown() signals stop, waits on completion flag with a bounded timeout, calls sync().
- Destructor joins threads from main context only.

## 6. Recording Pipeline

- ZED init with configured resolution/fps; compression LOSSLESS
- Depth mode chosen by recording mode; switching depth requires safe reinit
- Storage path chosen under /media/<user>/DRONE_DATA/flight_YYYYMMDD_HHMMSS/
- Writer emits SVO2 + optional depth info/images; RAW_FRAMES mode writes PNG/RAW
- CORRUPTED_FRAME tolerated – saved and logged; counters reset on success/warning

Size planning (baselines):
- HD720@60fps LOSSLESS: 25–28 MB/s (~9.9 GB per 4 min)
- HD720@30fps LOSSLESS: ~13 MB/s (~6.6 GB per 4 min)

## 7. Storage Handling

- Filesystem detection (vfat/msdos ⇒ enable 3.75GB safety cap)
- Label required: DRONE_DATA
- Graceful stop always finishes then calls sync() to flush buffers

## 8. LCD Model

- Update interval ≈3s when recording
- Ownership handoff described above; only one thread writes at a time
- "Recording Stopped" message retained for ~3s after stop

## 9. Error Handling & Logging

- Prefer warnings over aborts in field conditions (e.g., CORRUPTED_FRAME)
- Monitor consecutive failures with bounded retries
- Use completion flags over sleep‑based guesses

## 10. Extending the System

Add a recording mode:
1) Add to RecordingModeType (header)
2) Implement switch branch in startRecording()
3) Adjust depth mode auto‑policy if necessary
4) Add UI control in HTML template + updateStatus() JS
5) Validate 50+ start/stop cycles; watch for deadlocks

Add a camera setting:
1) Prefer dynamic changes; if reinit required: capture exposure, close camera, sleep 3s, reinit, restore
2) Update web UI + backend state serialization

LCD changes:
- Max 16 chars per line; avoid busy loops; respect ownership model

## 11. Build & Run

Build all apps:
```bash
./scripts/build.sh
```

Run main controller (needs sudo for nmcli):
```bash
sudo ./build/apps/drone_web_controller/drone_web_controller
```

Logs:
```bash
sudo journalctl -u drone-recorder -f
```

## 12. Dependencies & System Integration

- Systemd unit: systemd/drone-recorder.service (WorkingDirectory: repo root)
- Sudoers: /etc/sudoers.d/drone-controller (passwordless nmcli for AP)
- NetworkManager profiles created/removed dynamically by SafeHotspotManager

## 13. Testing

- Unit/integration by running tests in tests/* (shell/python utilities)
- Camera validation: tests/camera/*
- Storage & 4GB limit checks: tests/integration/*
- Hardware/I2C/INA219: tests/hardware/*

## 14. Non‑Goals & Known Limitations

- NVENC H.264/H.265 not available on Orin Nano; LOSSLESS only
- LCD hardware may be temporarily unavailable; software path is robust
- WiFi AP is field‑optimized for 2.4GHz; range ~50 m line‑of‑sight

## 15. Release Process

- Update CHANGELOG.md
- Ensure docs/guides/* are current
- Tag: vX.Y.Z and push
- Keep live_streamer active unless explicitly deprecated
