# Hardware Integration Guide (Consolidated)

Last updated: 2025-11-20  
Target: Jetson Orin Nano + ZED 2i + INA219 + 16x2 I2C LCD

This guide consolidates all hardware-facing knowledge (I2C, INA219, LCD) into a single, canonical reference for development and maintenance. It supersedes individual hardware notes now archived in `docs/archive/hardware/`.

## 1) Platform & Dependencies

- Jetson Orin Nano (Ubuntu 22.04 / JetPack)
- ZED SDK 4.x, CUDA 12.x
- NetworkManager (nmcli)
- Linux I2C stack + INA219 sensor
- Filesystems: NTFS/exFAT (FAT32 tolerated with 3.75GB cap)

Code touchpoints:
- `common/hardware/zed_camera/` – camera pipeline and recording
- `common/hardware/battery/` – INA219 sampling, filtering, calibration
- `common/hardware/lcd_display/` – I2C LCD driver and utilities
- `apps/drone_web_controller/` – ownership model, state-driven updates

## 2) I2C Architecture (Jetson)

- Typical LCD bus: `/dev/i2c-7` (addresses 0x27 or 0x3F)
- INA219 address depends on wiring (e.g., 0x40 / 0x41 family)
- Bus 0 has known quirks – prefer stable bus for LCD/INA219 (see archived notes). Avoid reusing camera USB power rails for sensor power.

Validation checklist:
- Detect devices: `i2cdetect -y 7`
- Read sensor: minimal `tests/hardware/test_ina219.py`
- LCD smoke test: `tests/hardware/simple_lcd_test`

## 3) Battery Monitoring (INA219)

Design goals:
- Stable SOC (%) under load; avoid oscillations
- Accurate consumption tracking; resilient to spikes

Implementation highlights:
- 3‑point calibration is the canonical method (see `tests/hardware/calibrate_ina219_3point.py`)
- 2‑segment model supported (legacy); hardcoded calibration allowed for field reliability
- Filtering and smoothing applied to voltage/current; transient read errors tolerated

Recommended workflow:
1. Wire INA219 with proper shunt resistor and stable ground reference.
2. Run 3‑point calibration (empty/mid/full) and save coefficients.
3. Place `ina219_calibration.json` in `tests/hardware/` or baked into production defaults.
4. Validate with `tests/hardware/test_battery_monitor.py` while recording.

Known pitfalls:
- Voltage sag under load is normal – see “Voltage drop under load” analysis.
- Calibrate at realistic loads; idle-only calibration leads to optimistic SOC.

## 4) LCD Display (16x2 I2C)

Goals:
- Minimal CPU use; no contention; readable field telemetry

Rules of engagement:
- Ownership model: exactly one owner at a time
  - IDLE → system monitor thread
  - RECORDING → recording monitor loop (≈3s interval)
  - STOPPING → stopRecording() holds for the short window
  - STOPPED → short 3s grace message, then back to system monitor
- Update interval: ~3s in recording to limit I2C traffic
- Keep messages ≤16 chars per line; avoid rapid flicker

Bring‑up procedure:
1. Verify bus and address: 0x27 or 0x3F on `/dev/i2c-7`.
2. Run `tests/hardware/i2c_scanner` and `simple_lcd_test`.
3. Enable LCD in controller config; confirm ownership model during state transitions.

Known issues and mitigations:
- Boot sequencing: allow the LCD to settle; initial write after camera bring‑up is safer.
- Hardware variance: Some backpacks expose different addresses; scan before hardcoding.

## 5) ZED Integration (Field Constraints)

- Compression: use `LOSSLESS` only (no NVENC H.264/265 on Orin Nano)
- Depth policy is mode‑driven:
  - SVO2 only → `DEPTH_MODE::NONE`
  - Depth requested (info/images) → `DEPTH_MODE::NEURAL_PLUS` with safe reinit
- CORRUPTED_FRAME is a warning – continue recording and reset failure counters

## 6) Storage & Filesystems

- Require `DRONE_DATA` label; mount under `/media/<user>/DRONE_DATA/`
- NTFS/exFAT recommended; FAT32 triggers 3.75GB safety cap and auto‑stop
- Always `sync()` at the end of shutdown/stop

## 7) Acceptance Tests (Hardware)

Run these before flight or after hardware changes:
- I2C bus scan passes; LCD “hello” renders; INA219 reads plausible values
- 10+ start/stop cycles without deadlocks; LCD ownership respected
- 4‑minute HD720@60 LOSSLESS recording without dropped frames or stop failures
- USB validation: free space and filesystem type detected correctly

## 8) Troubleshooting Map

Symptom → Likely causes → Where to look:
- LCD blank → bus/address mismatch, contention; test via `simple_lcd_test`; check ownership
- Wild SOC swings → missing calibration, load not considered; rerun 3‑point; review filtering
- Early stop at ~3.7GB → FAT32 detected; reformat NTFS/exFAT; label `DRONE_DATA`
- Depth SDK error → mode NONE while depth requested; ensure NEURAL_PLUS with reinit

## 9) Source Map (Quick Pointers)

- LCD: `common/hardware/lcd_display/` (handler, tool); ownership logic in controller
- Battery: `common/hardware/battery/` (INA219 driver, calibration, filters)
- Camera: `common/hardware/zed_camera/` (recorder, raw frames);
- Storage: `common/storage/` (filesystem checks, mount paths)
- Networking: `common/networking/` (SafeHotspotManager/NM)

---

This single guide replaces the previous hardware documents now stored under `docs/archive/hardware/`. Use those for historical detail; use this page for current, canonical practice.
