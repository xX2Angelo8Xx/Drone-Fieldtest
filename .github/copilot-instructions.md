# Copilot Instructions (Drone Field Test Where to add code
- New features in `apps/<app>/`; shared logic in `common/*`. Follow signal pattern, storage conventions, and profile-based execution. Keep network/service assumptions and paths stable.

Documentation references
- `RELEASE_v1.3_STABLE.md` - Complete v1.3 features, usage, and configuration
- `docs/CRITICAL_LEARNINGS_v1.3.md` - **ESSENTIAL** development journey and learnings
- `4GB_SOLUTION_FINAL_STATUS.md` - Filesystem corruption fix (FAT32 vs NTFS)
- `NVENC_INVESTIGATION_RESULTS.md` - Why LOSSLESS is the only mode
- `docs/WEB_DISCONNECT_FIX_v1.3.4.md` - Thread deadlock analysis
- `docs/SHUTTER_SPEED_UI_v1.3.md` - Exposure/FPS conversion system
- `EXTERNAL_FILES_DOCUMENTATION.md` - System dependencies

If anything above is unclear or incomplete (e.g., profile list, exact storage guards, or service env), say which section you need expanded and I'll refine it.em, v1.3-stable)

Purpose: Help AI agents be productive immediately in this Jetson Orin Nano + ZED 2i field-testing repo. Primary app is `apps/drone_web_controller` (WiFi AP + web UI at http://192.168.4.1:8080). Secondary tools: `smart_recorder`, `live_streamer`, `performance_test`, `zed_cli_recorder`.

**🎓 CRITICAL: New to this project? Read `docs/CRITICAL_LEARNINGS_v1.3.md` FIRST!**
- Complete development journey with ALL learnings
- Common mistakes and how to avoid them
- Thread deadlock fixes, filesystem issues, encoder limitations
- Testing methodology and performance baselines
- Essential for understanding WHY things work this way

Core layout and build
- Apps: `apps/` → binaries at `build/apps/<app>/<app>` (C++17, CMake). Build with `./scripts/build.sh`.
- Shared libs: `common/` → `hardware/` (ZED, LCD), `storage/` (USB auto-detect), `streaming/`, `utils/`.
- Deploy: systemd unit `systemd/drone-recorder.service` (runs as user `angelo`, 10s delay, working dir = repo root). Monitor with `sudo journalctl -u drone-recorder -f`.
- Quick start: `drone` alias or `./drone_start.sh`. WiFi AP SSID `DroneController` / password `drone123`.

Critical invariants (don’t break)
- Storage: USB must be labeled `DRONE_DATA`. Paths like `/media/angelo/DRONE_DATA/flight_YYYYMMDD_HHMMSS/` with `video.svo2`, `sensor_data.csv`, `recording.log`.
- Filesystem: NTFS/exFAT required for >4GB. Continuous single-file `.svo2` (tested 6.6–9.9GB). If FAT32 detected, auto-stop at ~3.75GB to avoid 4GB boundary corruption.
- ZED: Use `.svo2`; LOSSLESS only (Jetson Orin Nano lacks NVENC). CUDA 12.6 present at `/usr/local/cuda-12.6/include`.
- Signals: All apps share the same pattern — global `g_running` flag set false in `signalHandler`; main loop exits cleanly; release `g_recorder`/`g_storage` safely.
- Working dir: Many relative paths assume repo root; systemd service enforces this. Keep user=`angelo` and environment expectations.

Web controller and networking
- `drone_web_controller` creates AP + serves HTML5 UI (mobile-optimized). Do not change SSID/password/IP layout without updating scripts and service.
- Dual-network model: Ethernet for internet, WiFi AP for control (hostapd + dnsmasq). Use provided scripts and sudoers rule at `/etc/sudoers.d/drone-controller` (passwordless for WiFi ops).

Recorder patterns
- `smart_recorder`: enum-based `RecordingMode` with profile CLI. Examples: `standard` (4min HD720@30fps), `realtime_light` (30s HD720@15fps), `training` (60s HD1080@30fps). Keep profile architecture and unlimited-file design on NTFS/exFAT.
- Extraction: `build/tools/svo_extractor <svo2> <stride>` → JPEGs under `extracted_images/flight_*_video/`.

LCD and resilience
- LCD: I2C on `/dev/i2c-7`, 16-char formatting helper, non-blocking (system continues if LCD absent). Diagnostic tools: `i2c_lcd_tester`, `simple_lcd_test`, `lcd_backlight_test`.
- Error recovery: USB detection retries every 5s; graceful degradation across components. Preserve these behaviors.

Typical workflows
- Build: `./scripts/build.sh` → run `sudo ./build/apps/drone_web_controller/drone_web_controller` (manual) or manage via `drone-recorder` service.
- Validate big files: `sudo ./build/apps/smart_recorder/smart_recorder standard` (expect ~6.6GB in 4 min on NTFS/exFAT).
- Inspect output: `ls -lt /media/angelo/DRONE_DATA/flight_* | head -5` and open with `/usr/local/zed/tools/ZED_Explorer`.

Where to add code
- New features in `apps/<app>/`; shared logic in `common/*`. Follow signal pattern, storage conventions, and profile-based execution. Keep network/service assumptions and paths stable.

If anything above is unclear or incomplete (e.g., profile list, exact storage guards, or service env), say which section you need expanded and I’ll refine it.