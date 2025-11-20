# User Guide (Preview)

This guide is intentionally minimal for now. The project is currently developer‑focused. For quick operation, see `docs/QUICK_START.md`.

## Getting Started

- Start the controller:
  ```bash
  drone
  # or
  ./drone_start.sh
  ```
- Connect to WiFi `DroneController` (password `drone123`)
- Open Web UI: http://192.168.4.1:8080

## Recording Modes (Overview)

- SVO2 (video only)
- SVO2 + Depth Info
- SVO2 + Depth Images
- RAW_FRAMES (left/right/depth images)

For detailed developer internals (architecture, threading, shutdown rules), see `docs/guides/DEVELOPER_GUIDE.md` and `docs/CRITICAL_LEARNINGS_v1.3.md`.
