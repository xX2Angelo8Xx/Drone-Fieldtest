# Testing and Performance Guide (Consolidated)

Last updated: 2025-11-20

This guide consolidates performance, camera, and integration testing into a single reference. It replaces earlier per-topic guides now archived in `docs/archive/testing/`.

## 1) Performance Baselines

- HD720@60fps LOSSLESS: 25–28 MB/s, ~9.9GB per 4 min, 0 drops
- HD720@30fps LOSSLESS: ~13 MB/s, ~6.6GB per 4 min
- CPU usage during recording: 45–60% (all cores)

Validate with:
- tests/camera/test_zed_60fps.py
- tests/camera/test_zed_cli_4min.sh

## 2) Big-File Recording (Filesystem Validation)

Goal: ensure no 4GB corruption; validate NTFS/exFAT.

Steps:
1. Mount USB labeled `DRONE_DATA` (NTFS/exFAT).
2. Run `tests/integration/test_4gb_file.sh` and `tests/integration/test_enhanced_4gb.sh`.
3. Confirm SVO2 files open in ZED_Explorer.

Expected: single-file recording > 6GB without corruption on NTFS/exFAT. FAT32 triggers 3.75GB safety cap and auto-stop.

## 3) Camera & Mode Testing

Scenarios:
- SVO2 only (DEPTH_MODE::NONE) – minimum CPU
- SVO2 + Depth Info/Images (NEURAL_PLUS) – ensure safe reinit on toggle
- RAW_FRAMES – ensure file throughput and storage path layout

Acceptance:
- 10+ start/stop cycles without deadlocks
- CORRUPTED_FRAME logged as warning; recording continues

## 4) Shutter / Exposure Validation

- Maintain exposure % across FPS changes; do not freeze shutter.
- Verify derived shutter ≈ 1 / (FPS / (exposure/100)).
- Test with `tests/camera/test_zed_grab_only.py` for stability.

## 5) Integration: Gap-Free Recording

- Run `tests/integration/test_gap_free.sh`
- Ensure continuous write without intervals around stop/start boundaries

## 6) Livestream (If Enabled)

- `tests/integration/test_livestream.sh` checks throughput and caching
- Validate that caching does not interfere with recorder

## 7) Hardware Acceptance

- LCD: update cadence ~3s, ownership model respected; hello message renders
- INA219: 3‑point calibration present; SOC stable under load; JSON loaded

## 8) Troubleshooting

- Early stop near 3.7GB ⇒ FAT32 detected; reformat NTFS/exFAT
- Depth error “MODE_NONE” ⇒ ensure NEURAL_PLUS when depth requested
- Dropped LCD updates ⇒ check ownership model; no competing writers
- Spiky SOC ⇒ recalibrate under load; verify JSON present

## 9) Source Map

- Controller: apps/drone_web_controller/
- Camera: common/hardware/zed_camera/
- Storage: common/storage/
- LCD: common/hardware/lcd_display/
- Battery: common/hardware/battery/
- Networking: common/networking/

This guide supersedes: Performance_Test_Mode.md, Recording_Duration_Guide.md, SHUTTER_SPEED_TESTING_GUIDE.md, ZED_Recording_Info.md.
