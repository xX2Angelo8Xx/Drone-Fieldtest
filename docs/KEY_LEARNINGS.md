# Key Learnings (Concise)

A curated summary of the most important engineering lessons and constraints. Read this after `docs/CRITICAL_LEARNINGS_v1.3.md` for a compact refresher.

## Network Safety (AP)
- Use NetworkManager transient profiles; never modify persistent /etc configs.
- If the process crashes, Ethernet must remain available automatically.
- Provide sudoers entry for nmcli only (principle of least privilege).

## Shutdown & Threading
- Two flags: app stop vs system shutdown; handlers set flags, not cleanup.
- Never join a thread from itself; perform joins in destructor/main only.
- Replace arbitrary sleeps with completion flags + bounded timeout; call sync().

## State & Monitors
- Monitor loops must break on STOPPING; continue loops wedge the UI/socket.
- Set state to IDLE before raising “stop complete” to avoid race on shutdown.

## LCD Ownership
- Exactly one owner at a time (IDLE, RECORDING, STOPPING, STOPPED windows).
- ~3s update cadence during recording to avoid contention and I2C churn.

## Storage
- NTFS/exFAT required for files >4GB; FAT32 permitted with 3.75GB cap safety.
- Validate label `DRONE_DATA`; always `sync()` after cleanup.

## ZED Camera
- LOSSLESS compression only (no NVENC on Orin Nano).
- Depth mode auto‑managed by recording mode; reinit safely when enabling.
- Treat CORRUPTED_FRAME as warning; keep recording and reset fail counters.

## Exposure ↔ Shutter
- Preserve exposure % across FPS changes; compute shutter as 1 / (FPS / (exposure/100)).
- Document that 50% at 60 FPS ≈ 1/120, and so on.

## Robust UI / Web Server
- Request handlers never block; they set flags and return quickly.
- Status endpoint reflects real state transitions; fast (<100ms) updates.

## Testing Discipline
- 50+ rapid start/stop cycles to flush deadlocks.
- Big‑file tests (4+ min, HD720@60) on NTFS; verify zero drops and proper stop.
- Ownership checks: verify LCD shows “Recording Stopped” for ~3s window.

## Source Pointers
- Controller: `apps/drone_web_controller/`
- Camera: `common/hardware/zed_camera/`
- LCD: `common/hardware/lcd_display/`
- Battery (INA219): `common/hardware/battery/`
- Storage: `common/storage/`
- Networking: `common/networking/`

This page is the compact field manual. For detailed rationale and history, see `docs/CRITICAL_LEARNINGS_v1.3.md` and archives under `docs/archive/`.
