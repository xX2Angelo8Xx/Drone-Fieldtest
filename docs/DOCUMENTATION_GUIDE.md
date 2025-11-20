# Documentation Structure Guide

**Last Updated:** November 20, 2025  
**Version:** v1.6.0

## 📚 Documentation Organization

The project documentation is now organized into a clear, logical structure for easy navigation.

### 📍 Root Documentation (`docs/`)

**Essential documents that everyone should read:**

- **`CRITICAL_LEARNINGS_v1.3.md`** ⭐ **MUST READ**
  - Complete development journey and lessons learned
  - FAT32 corruption issue, NVENC unavailability, thread deadlock fixes
  - Critical patterns and anti-patterns
  - Read this FIRST before making any changes!

- **`QUICK_START.md`**
  - Quick reference for common commands
  - Connection info, recording modes, troubleshooting
  - Performance baselines

- **`CLEANUP_PLAN.md`**
  - v1.6.0 cleanup project documentation
  - File reorganization audit and decisions

### 📖 Guides (`docs/guides/`)

#### Hardware Guides (`docs/guides/hardware/`) - 16 files

**I2C Setup & Configuration:**
- `I2C_PINOUT_REFERENCE.md` - Complete pin mapping for Jetson Orin Nano
- `I2C_GPIO_PIN_MAPPING.md` - GPIO to I2C bus mapping
- `I2C_MULTI_DEVICE_NO_MULTIPLEXER.md` - Multiple I2C devices setup
- `I2C_BUS0_HARDWARE_ISSUE.md` - Known bus 0 issues and workarounds

**Battery Monitoring (INA219):**
- `INA219_SETUP_GUIDE.md` - Complete setup instructions
- `INA219_KALIBRIERUNG_3PUNKT.md` - 3-point calibration method (German)
- `INA219_2SEGMENT_UPGRADE_v1.5.4.md` - 2-segment battery monitoring
- `BATTERY_MONITORING_v1.5.5.md` - Current implementation overview
- `BATTERY_CALIBRATION_COMPARISON.md` - Calibration method comparison
- `HARDCODED_CALIBRATION_v1.5.4.md` - Hardcoded calibration data
- `VOLTAGE_DROP_UNDER_LOAD_v1.5.4.md` - Voltage behavior analysis

**LCD Display:**
- `LCD_BOOT_KNOWN_LIMITATION.md` - Boot sequence timing limitations
- `LCD_HARDWARE_ISSUE.md` - Hardware-specific issues
- `LCD_DISPLAY_MESSAGES_v1.3.md` - Display message reference
- `LCD_IMPROVEMENTS_v1.3.1.md` - Historical improvements
- `LCD_SHUTDOWN_MESSAGES_v1.5.4.md` - Shutdown sequence display

#### Testing Guides (`docs/guides/testing/`) - 4 files

- `Performance_Test_Mode.md` - Performance testing procedures
- `Recording_Duration_Guide.md` - Duration vs file size calculations
- `SHUTTER_SPEED_TESTING_GUIDE.md` - Camera shutter speed testing
- `ZED_Recording_Info.md` - ZED camera recording specifications

### 📦 Releases (`docs/releases/`) - 6 files

All version release notes in one place:
- `RELEASE_v1.5.4_STABLE.md` (current stable)
- `RELEASE_v1.5.3_STABLE.md`
- `RELEASE_v1.5.2_STABLE.md`
- `RELEASE_v1.3_STABLE.md`
- `PRE_RELEASE_v1.3_STABLE.md`
- `GIT_COMMIT_v1.5.2.md`

### 🗄️ Archive (`docs/archive/`)

Historical documentation preserved for reference.

#### Bug Fixes (`docs/archive/bugfixes/`) - 29 files
All bug fix documentation from v1.3 through v1.5.4:
- LCD boot sequence fixes
- Web server blocking fixes
- Shutdown deadlock fixes
- State transition fixes
- Battery monitoring fixes
- And 24 more...

#### Investigations (`docs/archive/investigations/`) - 8 files
Technical investigations and analyses:
- NVENC hardware investigation
- Gap reduction analysis
- Performance analysis
- SDK vs Explorer comparison
- Live streaming analysis
- And more...

#### Feature Development (`docs/archive/features/`) - 23 files
Feature implementation documentation:
- GUI restructuring and improvements (v1.4)
- Camera features (depth mode, shutter speed, reinit safety)
- Network improvements (WiFi, bandwidth monitoring)
- Recording improvements (graceful shutdown, timer unification)
- System configuration (autologin, autostart)

#### Status Reports (`docs/archive/status/`) - 14 files
Project status and completion summaries:
- Project completion summaries
- Session summaries
- Test success reports
- Final status documents

#### Miscellaneous (`docs/archive/misc/`) - 29 files
Various historical documents:
- Quick reference files (pre-consolidation)
- Architecture notes
- Test checklists
- Network safety documentation
- Extraction guides
- And more...

## 🎯 Where to Find What

### "I need to understand the project quickly"
→ Read `docs/QUICK_START.md`

### "I'm about to make changes to the code"
→ **Read `docs/CRITICAL_LEARNINGS_v1.3.md` first!**

### "I need to set up the hardware"
→ Check `docs/guides/hardware/` for I2C, INA219, and LCD guides

### "How do I test performance?"
→ See `docs/guides/testing/`

### "What changed in version X?"
→ Check `docs/releases/RELEASE_vX.X.X_STABLE.md`

### "How was feature X implemented?"
→ Search in `docs/archive/features/`

### "How was bug X fixed?"
→ Search in `docs/archive/bugfixes/`

### "What was the investigation about X?"
→ Check `docs/archive/investigations/`

## 📊 Documentation Statistics

- **Total documentation files:** 125
- **Essential docs in root:** 3
- **Hardware guides:** 16
- **Testing guides:** 4
- **Release notes:** 6
- **Archived bug fixes:** 29
- **Archived investigations:** 8
- **Archived feature docs:** 23
- **Status reports:** 14
- **Miscellaneous archive:** 29

## 🔄 Contributing Documentation

When adding new documentation:

1. **Active guides** → `docs/guides/hardware/` or `docs/guides/testing/`
2. **Release notes** → `docs/releases/`
3. **Bug fix documentation** → `docs/archive/bugfixes/` (after fix is released)
4. **Feature implementation** → `docs/archive/features/` (after feature is released)
5. **Investigation results** → `docs/archive/investigations/`

Keep the root `docs/` folder clean - only essential, frequently-accessed documents belong there.

---

**For the full developer onboarding experience:**
1. Read `README.md` (project overview)
2. Read `docs/CRITICAL_LEARNINGS_v1.3.md` (essential patterns)
3. Read `docs/QUICK_START.md` (commands and reference)
4. Read `.github/copilot-instructions.md` (architecture and development guidelines)
5. Browse relevant guides in `docs/guides/` as needed
