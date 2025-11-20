# Project Cleanup & Release Preparation Plan

**Date:** 2025-11-20  
**Version:** v1.5.4 → v1.6.0 (First Official Release)  
**Status:** 🔄 IN PROGRESS  
**Goal:** Clean, documented, release-ready codebase

---

## 📋 Master Cleanup Checklist

### Phase 1: Application Audit ✅ COMPLETED
- [x] Identify all applications in apps/
- [x] Verify which are actively used
- [x] Check dependencies and systemd integration

### Phase 2: Code Cleanup (15 Tasks)
- [ ] 1. Remove redundant applications
- [ ] 2. Remove redundant tools
- [ ] 3. Clean up test/debug files in root
- [ ] 4. Clean up scripts directory
- [ ] 5. Consolidate documentation
- [ ] 6. Create unified user guide
- [ ] 7. Create unified developer guide
- [ ] 8. Archive bug fix documentation
- [ ] 9. Archive investigation documents
- [ ] 10. Clean up root directory structure
- [ ] 11. Update main README.md
- [ ] 12. Create CHANGELOG.md
- [ ] 13. Verify systemd service files
- [ ] 14. Clean up build artifacts
- [ ] 15. Create release tag and documentation

---

## 🔍 Application Audit Results

### Currently Deployed (Production)
| App | Status | Used By | Keep? |
|-----|--------|---------|-------|
| **drone_web_controller** | ✅ ACTIVE | systemd service | **KEEP** (Main app) |
| smart_recorder | ✅ ACTIVE | Manual testing, CLI | **KEEP** (Useful tool) |

### Legacy/Development (Not in Production)
| App | Status | Last Used | Keep? |
|-----|--------|-----------|-------|
| data_collector | ⚠️ REPLACED | Legacy autostart | **REMOVE** (replaced by drone_web_controller) |
| live_streamer | ⚠️ EXPERIMENTAL | Testing only | **ARCHIVE** (future feature, not v1.x) |
| performance_test | 🔧 ONE-TIME | Hardware validation | **KEEP** (utility tool) |
| zed_cli_recorder | 🔧 WRAPPER | ZED Explorer wrapper | **KEEP** (simple wrapper utility) |

### Decision Matrix

**drone_web_controller:**
- ✅ Main production application
- ✅ Used by systemd service
- ✅ Web GUI + WiFi AP + Recording
- ✅ All v1.5.x fixes applied
- **Action:** KEEP - This IS the release

**smart_recorder:**
- ✅ CLI tool with 12+ profiles
- ✅ Useful for testing, development
- ✅ Documented in copilot-instructions
- ✅ No dependencies on it (standalone)
- **Action:** KEEP - Useful development tool

**data_collector:**
- ❌ Replaced by drone_web_controller
- ❌ Old autostart system (no longer used)
- ❌ systemd service now uses drone_web_controller
- ⚠️ References in: drone_aliases.sh, docs
- **Action:** REMOVE - Update references to drone_web_controller

**live_streamer:**
- ⚠️ Experimental H.264/H.265 streaming
- ⚠️ Not integrated into main app
- ⚠️ Requires additional setup (nginx-rtmp)
- ℹ️ Future feature candidate (v2.x)
- **Action:** MOVE to apps/experimental/ or archive

**performance_test:**
- ✅ Hardware validation tool
- ✅ One-time use for setup/troubleshooting
- ✅ No dependencies
- ℹ️ Documented in copilot-instructions
- **Action:** KEEP - Useful utility

**zed_cli_recorder:**
- ✅ Simple wrapper for ZED Explorer CLI
- ✅ Alternative recording method
- ✅ No dependencies
- ℹ️ 50 lines of code, minimal maintenance
- **Action:** KEEP - Harmless utility

---

## 🔧 Tools Audit

### Current Tools
| Tool | Purpose | Used By | Keep? |
|------|---------|---------|-------|
| calibrate_battery_monitor | Battery calibration | Manual operation | **KEEP** |
| lcd_display_tool | LCD message display | autostart.sh, scripts | **KEEP** |
| depth_viewer | Depth map visualization | One-time debugging | **KEEP** (utility) |
| svo_extractor | Extract frames from SVO2 | Post-processing | **KEEP** (utility) |

**Decision:** KEEP ALL - All are useful utilities with no maintenance burden.

---

## 📄 Documentation Audit

### Root Directory Documentation (30+ files!)

#### Active/Essential (KEEP in root)
- [x] README.md - Main project documentation
- [x] .github/copilot-instructions.md - Development guide
- [ ] CHANGELOG.md - TO CREATE
- [ ] LICENSE - Need to add
- [ ] RELEASE_v1.5.4_FINAL.md - Latest release notes

#### Historical Bug Fixes (MOVE to docs/archive/bugfixes/)
- [ ] BUGFIXES_v1.4.1.md
- [ ] WEB_DISCONNECT_FIX_v1.3.4.md
- [ ] SHUTDOWN_DEADLOCK_FIX_v1.5.4.md
- [ ] STATE_TRANSITION_FIX_v1.5.4.md
- [ ] WEB_SERVER_BLOCKING_FIX_v1.5.4d.md
- [ ] LCD_BOOT_FINAL_FIX_v1.5.4e.md
- [ ] LCD_BOOT_TIMING_FIX_v1.5.4f.md
- [ ] LCD_BOOT_KNOWN_LIMITATION.md
- [ ] FEEDBACK_FIXES_v1.5.4a.md
- [ ] COMPLETE_SESSION_SUMMARY.md
- [ ] FINAL_FIXES_v1.5.4c.md

#### Historical Investigations (MOVE to docs/archive/investigations/)
- [ ] NVENC_INVESTIGATION_RESULTS.md
- [ ] SDK_VS_EXPLORER_ANALYSIS.md
- [ ] LIVE_STREAMING_ANALYSIS.md
- [ ] ZED_EXPLORER_INVESTIGATION.md
- [ ] 4GB_CORRUPTION_SOLUTION.md
- [ ] 4GB_SOLUTION_FINAL_STATUS.md
- [ ] GAP_REDUCTION_ANALYSIS.md
- [ ] SEAMLESS_SEGMENTATION_RESULTS.md
- [ ] DUAL_INSTANCE_FINAL_ASSESSMENT.md

#### Project Status (MOVE to docs/archive/status/)
- [ ] PROJECT_COMPLETION_SUMMARY.md
- [ ] PROJECT_STATUS_FINAL.md
- [ ] PROJECT_OVERVIEW_NOV2024.md
- [ ] PRERELEASE_v1.3_SUMMARY.md
- [ ] AUTOSTART_TEST_REPORT.md
- [ ] ROBUSTNESS_TEST_PLAN.md

#### Quick Reference (CONSOLIDATE → docs/USER_GUIDE.md)
- [ ] MONITORING_QUICKSTART.md
- [ ] QUICK_COMMANDS.txt
- [ ] QUICK_REFERENCE.txt
- [ ] TERMINAL_COMMANDS.txt

#### Guides (CONSOLIDATE or MOVE to docs/guides/)
- [ ] STREAMING_DEPLOYMENT_GUIDE.md → docs/guides/ (experimental)
- [ ] LIVESTREAM_TEST_GUIDE.md → docs/guides/ (experimental)
- [ ] ORGANIZED_EXTRACTION.md → docs/guides/
- [ ] SVO_EXTRACTION_SUCCESS.md → docs/archive/

#### Policy/Architecture (MOVE to docs/)
- [ ] NETWORK_SAFETY_POLICY.md → docs/
- [ ] NETWORK_SAFETY_SOLUTION.md → docs/
- [ ] Project_Architecture → docs/ARCHITECTURE.md
- [ ] EXTERNAL_FILES_DOCUMENTATION.md → docs/

#### Misc/Temporary (MOVE to docs/archive/misc/)
- [ ] GIT_COMMIT_v1.5.2.md
- [ ] WIFI_IWD_FIX.md
- [ ] RECORDING_MODE_REDESIGN_ROADMAP.md
- [ ] OBJECT_DETECTION_ARCHITECTURE.cpp (orphaned code?)

---

## 🧹 Test/Debug Files Audit (Root Directory)

### Test Scripts (MOVE to tests/)
- [ ] test_4gb_file.sh
- [ ] test_camera_only.py
- [ ] test_enhanced_4gb.sh
- [ ] test_gap_free.sh
- [ ] test_livestream.sh
- [ ] test_ntfs_usb.sh
- [ ] test_zed_60fps.py
- [ ] test_zed_cli_4min.sh
- [ ] test_zed_grab_only.py
- [ ] test_zed_ssd.sh
- [ ] analyze_corruption.sh
- [ ] extract_svo_images.sh

### Compiled Test Binaries (DELETE or add to .gitignore)
- [ ] debug_usb (binary)
- [ ] i2c_lcd_tester (binary)
- [ ] i2c_scanner (binary)
- [ ] lcd_backlight_test (binary)
- [ ] simple_lcd_test (binary)
- [ ] test_lcd (binary)

### Test Source Files (MOVE to tests/)
- [ ] debug_usb.cpp
- [ ] i2c_lcd_tester.cpp
- [ ] i2c_scanner.cpp
- [ ] lcd_backlight_test.cpp
- [ ] minimal_lcd_test.cpp
- [ ] simple_lcd_test.cpp
- [ ] test_lcd.cpp
- [ ] svo_image_extractor.cpp

### Temporary/Generated Files (DELETE)
- [ ] ZED_Diagnostic_ResultsOct18.json
- [ ] performance_baseline.txt
- [ ] extracted_images/ (if empty or test data)

---

## 📁 Proposed Directory Structure

```
Drone-Fieldtest/
├── README.md                          # Main documentation
├── CHANGELOG.md                       # Version history (TO CREATE)
├── LICENSE                            # Project license (TO CREATE)
├── CMakeLists.txt
├── .gitignore
├── .github/
│   └── copilot-instructions.md        # Development guide
│
├── apps/                              # Applications
│   ├── drone_web_controller/          # ⭐ Main application
│   ├── smart_recorder/                # CLI multi-profile recorder
│   ├── performance_test/              # Hardware validation tool
│   └── zed_cli_recorder/              # ZED Explorer wrapper
│
├── common/                            # Shared libraries
│   ├── hardware/
│   ├── networking/
│   ├── storage/
│   └── utils/
│
├── tools/                             # Utility tools
│   ├── calibrate_battery_monitor/
│   ├── lcd_display_tool/
│   ├── depth_viewer/
│   └── svo_extractor/
│
├── scripts/                           # Build and utility scripts
│   ├── build.sh
│   ├── start_drone.sh
│   ├── autostart_control.sh
│   └── drone_aliases.sh
│
├── systemd/                           # System service files
│   └── drone-recorder.service
│
├── tests/                             # Test scripts and utilities
│   ├── hardware/                      # LCD, I2C, USB tests
│   ├── camera/                        # ZED camera tests
│   └── integration/                   # End-to-end tests
│
├── docs/                              # Documentation
│   ├── USER_GUIDE.md                  # User manual (TO CREATE)
│   ├── DEVELOPER_GUIDE.md             # Developer guide (TO CREATE)
│   ├── ARCHITECTURE.md                # System architecture
│   ├── NETWORK_SAFETY_POLICY.md
│   ├── guides/                        # Feature guides
│   │   ├── streaming.md
│   │   └── extraction.md
│   ├── releases/                      # Release notes
│   │   ├── RELEASE_v1.5.4_FINAL.md
│   │   ├── RELEASE_v1.5.3_STABLE.md
│   │   └── RELEASE_v1.5.2_STABLE.md
│   └── archive/                       # Historical documentation
│       ├── bugfixes/                  # All *_FIX_*.md files
│       ├── investigations/            # All *_ANALYSIS.md files
│       ├── status/                    # Project status snapshots
│       └── misc/                      # Other historical docs
│
├── build/                             # Build output (gitignored)
├── data/                              # Test data (gitignored)
└── models/                            # ML models (if any)
```

---

## 🎯 Cleanup Actions

### 1. Remove data_collector Application
```bash
# Verify not in use
grep -r "data_collector" systemd/
grep -r "data_collector" apps/drone_web_controller/

# Remove
rm -rf apps/data_collector/
# Update CMakeLists.txt (remove add_subdirectory)
# Update drone_aliases.sh (remove references)
# Update documentation
```

### 2. Archive live_streamer
```bash
# Move to experimental (future feature)
mkdir -p apps/experimental/
mv apps/live_streamer apps/experimental/
# Update CMakeLists.txt (comment out or remove)
# Move STREAMING_DEPLOYMENT_GUIDE.md to docs/archive/
```

### 3. Create Directory Structure
```bash
mkdir -p docs/{releases,archive/{bugfixes,investigations,status,misc},guides}
mkdir -p tests/{hardware,camera,integration}
```

### 4. Move Documentation
```bash
# Release notes
mv RELEASE_v*.md docs/releases/

# Bug fixes
mv *FIX*.md BUGFIXES*.md FEEDBACK_FIXES*.md docs/archive/bugfixes/
mv COMPLETE_SESSION_SUMMARY.md docs/archive/bugfixes/

# Investigations
mv *INVESTIGATION*.md *ANALYSIS*.md docs/archive/investigations/
mv 4GB_*.md SEAMLESS_*.md GAP_*.md docs/archive/investigations/

# Status
mv PROJECT_*.md PRERELEASE*.md AUTOSTART_TEST*.md docs/archive/status/
mv ROBUSTNESS_TEST_PLAN.md docs/archive/status/

# Guides
mv STREAMING_DEPLOYMENT_GUIDE.md LIVESTREAM_TEST_GUIDE.md docs/guides/
mv ORGANIZED_EXTRACTION.md SVO_EXTRACTION_SUCCESS.md docs/guides/

# Architecture/Policy
mv Project_Architecture docs/ARCHITECTURE.md
mv NETWORK_*.md docs/
mv EXTERNAL_FILES_DOCUMENTATION.md docs/

# Misc
mv GIT_COMMIT*.md WIFI_IWD_FIX.md RECORDING_MODE*.md docs/archive/misc/
```

### 5. Move Test Files
```bash
# Test scripts
mv test_*.sh test_*.py analyze_corruption.sh extract_svo_images.sh tests/

# Test source
mv *_test.cpp debug_usb.cpp i2c_*.cpp svo_image_extractor.cpp tests/hardware/

# Remove compiled binaries (add to .gitignore)
rm -f debug_usb i2c_lcd_tester i2c_scanner lcd_backlight_test simple_lcd_test test_lcd
```

### 6. Update .gitignore
```gitignore
# Build artifacts
build/
*.o
*.a

# Test binaries
debug_usb
i2c_lcd_tester
i2c_scanner
lcd_backlight_test
simple_lcd_test
test_lcd

# Temporary files
*.json
performance_baseline.txt
extracted_images/

# Data files
data/
*.svo
*.svo2

# IDE
.vscode/
.idea/
```

---

## 📝 Documentation to Create

### 1. CHANGELOG.md
```markdown
# Changelog

## [1.6.0] - 2025-11-20 (First Official Release)
### Added
- First official release with cleaned codebase
- Comprehensive documentation structure
- User and developer guides

### Changed
- Reorganized project structure
- Consolidated documentation
- Archived historical development docs

### Fixed
- All v1.5.x bugs and issues resolved

## [1.5.4] - 2025-11-19
(Extract from RELEASE_v1.5.4_FINAL.md)
...
```

### 2. USER_GUIDE.md
Consolidate:
- MONITORING_QUICKSTART.md
- QUICK_COMMANDS.txt
- QUICK_REFERENCE.txt
- Basic usage from README.md

### 3. DEVELOPER_GUIDE.md
Consolidate:
- Project_Architecture
- Build instructions
- Code structure
- Testing procedures

### 4. Updated README.md
- Project description
- Features
- Hardware requirements
- Quick start
- Directory structure
- Documentation index
- License

---

## ✅ Next Steps

**Start with:**
1. Create backup branch: `git checkout -b pre-cleanup-backup`
2. Create directory structure
3. Move documentation (safest, reversible)
4. Update .gitignore
5. Test build after each change
6. Create new documentation
7. Remove redundant apps
8. Final testing
9. Create release tag

**Safety:**
- ✅ Commit after each major change
- ✅ Test build after each change
- ✅ Keep backup branch
- ✅ Can revert any step

---

**Ready to start?** Let's begin with creating the directory structure and moving documentation (safest operations first).
