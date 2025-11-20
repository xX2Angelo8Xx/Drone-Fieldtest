# Changelog

All notable changes to this project will be documented in this file. This project adheres to Semantic Versioning.

## [v1.6.0] - Unreleased

Developer-focused release: repository structure, documentation, and maintainability.

- Project structure cleanup (no functional changes)
  - Moved 70+ docs into a clear hierarchy: docs/guides, docs/releases, docs/archive/{bugfixes,features,investigations,status,misc}
  - Moved 25+ test binaries/scripts into tests/{hardware,camera,integration}
  - Added .gitignore rules for compiled test binaries
  - Archived historical I2C/diagnostic scripts to scripts/archive/i2c_debugging
  - Created docs/QUICK_START.md and docs/DOCUMENTATION_GUIDE.md
- Applications
  - Removed apps/data_collector (superseded by drone_web_controller)
  - Kept apps/live_streamer active (not archived) per current requirements
- Build
  - Updated top-level CMakeLists to drop data_collector
  - Full build verified on Jetson Orin Nano + ZED SDK 4.x

## [v1.5.4] - 2025-11-19

Field robustness and shutdown/threading correctness. Major production stability improvement.

- Shutdown and threading
  - Dual-flag shutdown model: app-stop vs system-shutdown
  - Web server no longer performs blocking cleanup in request threads
  - Destructor-driven thread joins from main thread only (prevents deadlocks)
  - Monitor loops use break (not continue) on STOPPING to avoid wedging
  - Completion flags with timeout replace arbitrary delays; final sync() on stop
  - State transition set to IDLE before completion flag is raised
- Recording pipeline
  - Unified stop path for manual/timer; consistent cleanup semantics
  - LCD ownership model per state (IDLE, RECORDING, STOPPING, STOPPED)
- Battery and sensors
  - INA219 calibration flow (3‑point; 2‑segment UI), sampling, smoothing
  - Graceful handling of transient sensor read errors
- Web controller
  - Non‑blocking HTTP request handling; status refresh improvements
  - Boot‑time LCD sequence hardened

## [v1.5.3] - 2025-11-18

- ZED camera robustness
  - Treat CORRUPTED_FRAME as warning (not fatal) to avoid aborting missions
  - Dark/covered frames are recorded and counters reset on success or warning
- Performance baselines documented (HD720@60fps LOSSLESS ≈ 25–28 MB/s)

## [v1.5.2] - 2025-11-17

- Automatic depth mode management
  - SVO2 only ⇒ DEPTH_MODE::NONE (save CPU)
  - SVO2+Depth ⇒ auto‑enable NEURAL_PLUS and reinit safely
- Unified stop routine; fewer code paths and safer state transitions

## [v1.5.0] - 2025-11-15

- UI and camera UX
  - Clean shutter speeds (1/60, 1/90, 1/120, …)
  - Exposure ↔ shutter conversion respecting FPS

## [v1.3.0] - 2025-11-10

- First stable WiFi web controller
  - AP setup via NetworkManager (reversible; Ethernet preserved)
  - Desktop autostart control; passwordless sudo for nmcli
  - Continuous LOSSLESS SVO2 recording on NTFS/exFAT (no 4GB corruption)

[v1.6.0]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/compare/v1.5.4...HEAD
[v1.5.4]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases/tag/v1.5.4
[v1.5.3]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases/tag/v1.5.3
[v1.5.2]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases/tag/v1.5.2
[v1.5.0]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases
[v1.3.0]: https://github.com/xX2Angelo8Xx/Drone-Fieldtest/releases
