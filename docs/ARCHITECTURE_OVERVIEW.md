# Architecture & Workflows (Entry Point)

Last updated: 2025-11-20  
Target platform: Jetson Orin Nano + ZED 2i  
Languages: C++17, Bash  
Build: CMake + JetPack (Ubuntu 22.04)

Purpose
- Single entry point for developers to understand how the system is structured, how components interact at runtime, and where to change or extend behavior.
- Pairs with the “Critical Learnings” document for non‑negotiable design rules.

Read next
- Non‑negotiable rules and rationales: `docs/CRITICAL_LEARNINGS_v1.3.md`
- Developer deep dive: `docs/guides/DEVELOPER_GUIDE.md`
- Hardware details: `docs/guides/hardware/HARDWARE_INTEGRATION_GUIDE.md`
- Testing & performance: `docs/guides/testing/TESTING_AND_PERFORMANCE_GUIDE.md`

---

## 1. High‑Level System Map

```
+-------------------------------------------------------------------------------------+
|                                     apps/                                           |
|                                                                                     |
|  drone_web_controller  <-- production (WiFi AP + HTTP UI + Recording control)       |
|  smart_recorder        <-- CLI recorder (profiles)                                  |
|  performance_test      <-- Hardware validation                                      |
|  zed_cli_recorder      <-- ZED CLI wrapper                                          |
|  live_streamer         <-- H.26x streaming (kept active; experimental)              |
+-------------------------------------------------------------------------------------+

+--------------------------------------------------+   +------------------------------+
|                    common/                        |   |           scripts/           |
|  hardware/zed_camera/  (ZED SVO2, RAW)           |   | build.sh, install_service.sh |
|  hardware/lcd_display/ (I2C 16x2)                 |   | start/stop/wifi helpers      |
|  hardware/battery/     (INA219, filters)          |   +------------------------------+
|  storage/               (USB + fs validation)     |
|  networking/            (NetworkManager AP)       |   +------------------------------+
|  utils/                 (helpers, time, fmt)      |   |            tests/            |
+--------------------------------------------------+   | camera/ hardware/ integration |
                                                        +------------------------------+

+-------------------+  +--------------------+  +-------------------+
|  systemd/         |  |  data/             |  |  tools/           |
|  drone-recorder   |  |  diagnostics, json |  |  extractors, etc. |
+-------------------+  +--------------------+  +-------------------+
```

Key entry files
- apps/drone_web_controller/drone_web_controller.cpp – main orchestration, threads, HTTP, state machine
- common/hardware/zed_camera/zed_recorder.cpp – ZED capture/recording, modes, depth policy
- common/storage/storage.cpp – USB mount and filesystem checks
- common/networking/safe_hotspot_manager.cpp – AP creation/cleanup via NetworkManager
- common/hardware/lcd_display/* – LCD handler and tool
- common/hardware/battery/* – INA219 sampling, calibration, filtering

---

## 2. Runtime Components & Threads (drone_web_controller)

```
+--------------------------- main() ----------------------------------------------+
|  init hardware/context                                                          |
|  spawn web_server_thread  --+  HTTP listener; sets flags; non-blocking          |
|  spawn monitor threads      |                                                   |
|  while (!shutdown_requested) {                                                  |
|     tick system monitor (IDLE owner of LCD)                                     |
|     react to flags (start/stop/system_shutdown)                                 |
|  }                                                                              |
|  if (system_shutdown_requested) -> call `shutdown -h now` after cleanup         |
+---------------------------------------------------------------------------------+

Additional actors
- Recording monitor loop: active during RECORDING; owns LCD; updates every ~3s
- Storage watcher: ensures DRONE_DATA is present; applies FAT32 cap policy
- Battery thread (optional): INA219 sampling/filtering for SOC/voltage
```

Threading rules (non‑negotiable)
- Request handlers set flags only; they never join or block on cleanup
- Never join a thread from within itself; destructor/main joins
- Monitor loops must break on STOPPING; no continue loops
- Use completion flags + bounded timeout; call `sync()` after successful stop

References: `docs/CRITICAL_LEARNINGS_v1.3.md` and `docs/guides/DEVELOPER_GUIDE.md`.

---

## 3. HTTP API Flow

Endpoints (typical, exact strings may vary in source):
- GET /api/status → return JSON with state, fps, exposure, depth mode, file, size
- POST /api/start → set `start_requested_ = true`
- POST /api/stop → set `stop_requested_ = true`
- POST /api/shutdown → set `system_shutdown_requested_ = true`

Flow
```
Client (Web UI)
   │ POST /api/start
   ▼
Web server thread: set start flag; respond 200 immediately (non-blocking)
   │
   ▼
Main loop sees start flag → startRecording()
   │
   ├─ open ZED with configured resolution/fps; compression = LOSSLESS
   ├─ choose depth policy based on mode (NONE vs NEURAL_PLUS)
   ├─ allocate output path under /media/<user>/DRONE_DATA/flight_YYYYMMDD_HHMMSS/
   └─ spawn/activate recording monitor; transition to RECORDING
```

Stop flow (POST /api/stop)
```
Web server sets stop flag → main loop calls stopRecording() →
- close writers and camera resources
- transition state: STOPPING → IDLE (IDLE set before completion flag)
- set completion flag; handleShutdown() (if invoked) waits on completion flag with timeout
- final `sync()` call to flush filesystem buffers
```

---

## 4. State Machine (Recording Lifecycle)

```
          +---------+           startRecording()           +-----------+
   +----> |  IDLE  | ------------------------------------> | RECORDING |
   |      +---------+                                      +-----------+
   |            ^                                                 |
   |            | stopRecording()                                 | monitor loop (owns LCD)
   |            |                                                 |
   |            +------------------------ STOPPING  --------------+
   |                                      (short; break loops)     \
   |                                                               \
   +-------------------------- handleShutdown() waits ---------------> transition to IDLE, set completion
```

Key rules
- STOPPING is a short, explicit phase
- Monitor loops must `break` on STOPPING (never `continue`)
- Set state to IDLE before setting the stop completion flag

---

## 5. Network AP Flow (SafeHotspotManager)

Goal: WiFi AP must be reversible; Ethernet connectivity must remain available even on crash.

```
setup:
- create temporary AP profile via nmcli (not permanent)
- bring profile UP → AP active (SSID: DroneController)

teardown:
- bring profile DOWN → delete profile
- no /etc changes; Ethernet becomes primary again automatically
```

Files
- `common/networking/safe_hotspot_manager.cpp`
- sudoers: `/etc/sudoers.d/drone-controller` (nmcli only)

---

## 6. Storage & Filesystems

Mount requirements
- Label `DRONE_DATA`
- NTFS/exFAT recommended; FAT32 allowed but triggers 3.75GB safety cap

Behavior
- Output path: `/media/<user>/DRONE_DATA/flight_YYYYMMDD_HHMMSS/`
- Files written: `video.svo2`, optional depth data, logs, sensor CSV
- Final `sync()` invoked after stop to flush buffers

Files
- `common/storage/storage.cpp`

---

## 7. ZED Recording Pipeline

Modes
- SVO2 (video only)
- SVO2 + Depth Info (record structured depth data)
- SVO2 + Depth Images (PNG visualization)
- RAW_FRAMES (left/right/depth images)

Depth policy
- SVO2 only → `DEPTH_MODE::NONE`
- Depth requested → `DEPTH_MODE::NEURAL_PLUS` and safe camera reinit

Robustness
- `CORRUPTED_FRAME` is treated as a warning; frame still saved; counters reset
- Compression is always `LOSSLESS` on Orin Nano

Files
- `common/hardware/zed_camera/zed_recorder.cpp`
- `common/hardware/zed_camera/raw_frame_recorder.cpp`

---

## 8. LCD Ownership Model (16x2 I2C)

Owners (exactly one at a time)
- IDLE → system monitor (main loop)
- RECORDING → recording monitor loop (≈3s cadence)
- STOPPING → stopRecording() temporarily
- STOPPED (~3s) → message holds, then hand back to system monitor

Rules
- Messages must fit 16 chars per line
- Avoid flicker and busy writes; 3s cadence in recording is the sweet spot

Files
- `common/hardware/lcd_display/*`
- Integration code in `apps/drone_web_controller/drone_web_controller.cpp`

---

## 9. Battery Monitoring (INA219)

- 3‑point calibration preferred; 2‑segment and hardcoded fallback supported
- Filtering for voltage/current; tolerate transient read errors
- SOC must be stable under load; validate with hardware tests

Files
- `common/hardware/battery/*`
- Tests in `tests/hardware/*`

---

## 10. Error Handling & Logging Philosophy

- Favor warnings and continued operation during field missions
- Completion flags with timeout instead of arbitrary sleeps
- Early detection of filesystem constraints and network failures

---

## 11. Build, Deploy, Operate

Build
```bash
./scripts/build.sh
```
Run (needs sudo for nmcli)
```bash
sudo ./build/apps/drone_web_controller/drone_web_controller
```
Service (production)
- `systemd/drone-recorder.service` (WorkingDirectory = repo root)
- Logs: `sudo journalctl -u drone-recorder -f`

---

## 12. Development Workflows

Add a recording mode
1. Update enum in controller header (RecordingModeType)
2. Implement branch in `startRecording()`
3. Adjust auto depth policy if needed
4. Add UI control and status serialization
5. Validate 50+ start/stop cycles; ensure STOPPING breaks loops

Add a camera setting
1. Prefer runtime change; if reinit needed, capture exposure, close, sleep 3s, reinit, restore exposure
2. Wire into UI + `/api/status`

LCD changes
- Respect ownership model; keep text ≤16 chars; 3s cadence

Testing
- Big file on NTFS (4+ min @ 60fps); zero drops; proper stop and sync
- Hardware acceptance (LCD hello, INA219 stable SOC, I2C scan ok)

---

## 13. Flow Diagrams (Quick References)

HTTP Start → Record
```
POST /api/start
  → controller flag set
  → main: startRecording()
      → zed_recorder.open(LOSSLESS)
      → depth policy (NONE or NEURAL_PLUS)
      → storage.prepare(flight_YYYYMMDD_HHMMSS)
      → state = RECORDING; spawn/activate rec monitor
```

Stop & Shutdown
```
POST /api/stop
  → stopRecording()
      → writers close
      → state = IDLE (before flag)
      → stop_complete_ = true
  → handleShutdown(): wait (bounded), sync(), exit

POST /api/shutdown
  → system_shutdown_requested_ = true
  → main detects → after cleanup: `shutdown -h now`
```

AP Setup (Safe)
```
setup(): nmcli connection add ...; up DroneController
teardown(): nmcli connection down; delete profile
(no permanent /etc edits, Ethernet unaffected)
```

LCD Ownership Handoff
```
IDLE (system monitor) → RECORDING (rec monitor owns) → STOPPING (stopRecording) → STOPPED (~3s) → IDLE
```

---

## 14. Source Map (Where to Look)

- Controller: `apps/drone_web_controller/drone_web_controller.cpp`
- Camera: `common/hardware/zed_camera/zed_recorder.cpp`, `raw_frame_recorder.cpp`
- Storage: `common/storage/storage.cpp`
- Network: `common/networking/safe_hotspot_manager.cpp`
- LCD: `common/hardware/lcd_display/*`
- Battery: `common/hardware/battery/*`
- Service: `systemd/drone-recorder.service`
- Scripts: `scripts/*.sh`
- Tests: `tests/{camera,hardware,integration}`

---

## 15. Invariants & Limits (TL;DR)

- NVENC H264/H265 not available → LOSSLESS only
- NTFS/exFAT required for >4GB; FAT32 triggers 3.75GB cap
- Dual shutdown flags; no thread self‑join; completion flags with timeout; `sync()`
- CORRUPTED_FRAME is warning; continue recording
- Depth policy follows mode; reinit safely when enabling
- LCD has a single owner per state; ~3s cadence

This document is the canonical entry point to the codebase. Pair it with `CRITICAL_LEARNINGS_v1.3.md` for detailed rationales and anti‑patterns.
