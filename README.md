# Drone Field Test System (Developer Overview)

Target: Jetson Orin Nano + ZED 2i stereo camera

This repository contains a production‑ready field recording stack with a WiFi‑controlled web front‑end and a robust ZED camera pipeline. This README is developer‑oriented. For quick usage, see `docs/QUICK_START.md`.

## Architecture

- apps/
  - drone_web_controller – main production app (WiFi AP + HTTP UI + recording control)
  - smart_recorder – CLI multi‑profile recorder
  - performance_test – hardware validation
  - zed_cli_recorder – ZED tools wrapper
  - live_streamer – H.26x streaming (kept active; experimental)
- common/
  - hardware/{zed_camera,lcd_display,battery} – ZED, I2C LCD, INA219
  - storage – USB detection + filesystem validation
  - networking – Safe hotspot via NetworkManager
  - utils – helpers
- scripts/ – build/deploy/diagnostics
- tests/ – camera, hardware, integration
- tools/ – extractors, viewers

## Dependencies

- ZED SDK 4.x, CUDA 12.x
- NetworkManager (nmcli)
- Linux I2C + INA219 sensor
- C++17, CMake ≥ 3.16
- Filesystems: NTFS/exFAT recommended; FAT32 tolerated with 3.75GB cap

## Build & Run

```bash
./scripts/build.sh
sudo ./build/apps/drone_web_controller/drone_web_controller
```

Logs:
```bash
sudo journalctl -u drone-recorder -f
```

## Critical Design Rules (do not break)

- Network safety: use transient NetworkManager profiles; never persist system config; crash must restore Ethernet.
- Dual shutdown flags: app stop vs system shutdown; do not join threads from within those threads.
- Monitor loops: break on STOPPING; no continue‑loops that wedge the UI.
- Completion flags: explicit flag + timeout; no fixed sleeps; call sync() at the end of stop.
- State transition: set IDLE before raising completion.
- LCD ownership: one owner per state to avoid contention.
- ZED: LOSSLESS compression only (no NVENC on Orin Nano).
- Depth policy: auto‑managed by recording mode; reinit safely when toggling.
- CORRUPTED_FRAME: warning, not fatal; continue recording.

Full rationale and code patterns: `docs/CRITICAL_LEARNINGS_v1.3.md`.

## Documentation

- Quick Start: `docs/QUICK_START.md`
- Developer Guide: `docs/guides/DEVELOPER_GUIDE.md`
- Documentation Guide: `docs/DOCUMENTATION_GUIDE.md`
- Releases/Changelog: `docs/releases/`, `CHANGELOG.md`
- Historical material: `docs/archive/`

## Versioning

See `CHANGELOG.md` and `docs/releases/`.

## License

Pending project choice (MIT or GPLv3). Please confirm your preference.