# Autostart Service Cleanup - November 18, 2025

## 🐛 Problem Discovered

User renamed `~/Desktop/Autostart` to `~/Desktop/Autostart_DISABLED` but the system still auto-started the drone controller on boot.

## 🔍 Root Cause

**Two overlapping systemd services were installed:**

1. **`drone-recorder.service`** (Nov 5 - CORRECT)
   - Status: Disabled
   - Calls: `apps/drone_web_controller/autostart.sh`
   - ✅ **Checks Desktop file before starting**
   - Respects user's autostart preference

2. **`drone-web-controller.service`** (Nov 17 - PROBLEMATIC)
   - Status: **Enabled** (caused the issue!)
   - Calls: `scripts/autostart_service_wrapper.sh`
   - ❌ **No Desktop file check - always runs**
   - Bypassed user control

The second service was likely created to add features (early LCD feedback, resource limits, cleanup) but **forgot the critical Desktop file check**, making it ignore user preferences.

## 🧹 Cleanup Actions Performed

### Files Deleted:
1. ❌ `/etc/systemd/system/drone-web-controller.service` - Rogue service file
2. ❌ `scripts/autostart_service_wrapper.sh` - Wrapper used only by deleted service
3. ❌ `scripts/early_lcd_feedback.sh` - Early LCD script (redundant with autostart.sh)
4. ❌ `systemd/drone-web-controller.service` - Repo copy of rogue service

### Files Kept:
1. ✅ `/etc/systemd/system/drone-recorder.service` - Correct service (has Desktop file check)
2. ✅ `apps/drone_web_controller/autostart.sh` - Main autostart script with Desktop file logic
3. ✅ `scripts/stop_drone.sh` - Useful for `drone-stop` alias
4. ✅ `scripts/autostart_control.sh` - Status checker
5. ✅ `systemd/drone-recorder.service` - Repo template

## ✅ Current State (Post-Cleanup)

### Only One Service Remains:
```bash
$ systemctl list-unit-files | grep drone
drone-recorder.service    enabled    enabled
```

### Two-Level Autostart Control (BOTH Required):

**Level 1: Systemd Service** (Controls if script runs on boot)
```bash
sudo systemctl enable drone-recorder.service   # Service runs on boot
sudo systemctl disable drone-recorder.service  # Service won't run on boot
```

**Level 2: Desktop File** (Script checks this before starting drone)
```bash
touch ~/Desktop/Autostart              # Script proceeds with startup
rm ~/Desktop/Autostart                 # Script exits early (no startup)
mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED  # Same as rm
```

### Complete Control Matrix:

| Systemd Service | Desktop File | Result |
|----------------|--------------|--------|
| enabled | exists | ✅ Auto-starts on boot |
| enabled | missing | ⏸️ Service runs but exits early (no drone startup) |
| disabled | exists | ❌ Nothing happens (service doesn't run) |
| disabled | missing | ❌ Nothing happens (service doesn't run) |

**Current Status:** Both enabled - system WILL auto-start drone on next boot

### Manual Control:
```bash
# Start manually
drone

# Stop
drone-stop

# Check status
drone-status
```

## 📝 Lessons Learned

1. **Never create duplicate services without documenting why**
2. **If adding features to a service, update the existing one - don't create a new one**
3. **Critical checks (like Desktop file) must be preserved when refactoring**
4. **Document service purpose and dependencies clearly**

## 🔧 Recommended Practice

**Single Service Architecture:**
- One service: `drone-recorder.service`
- One autostart script: `autostart.sh` (with Desktop file check)
- Clear enable/disable mechanism via Desktop file
- No hidden duplicate services

## 📋 Verification Steps

To ensure autostart is properly configured:

### Check Current Status:
```bash
# 1. Check service is enabled
systemctl is-enabled drone-recorder.service
# Should show: enabled

# 2. Check Desktop file exists
ls ~/Desktop/Autostart
# Should exist (not Autostart_DISABLED)
```

### To ENABLE Autostart (both required):
```bash
# 1. Enable systemd service
sudo systemctl enable drone-recorder.service

# 2. Ensure Desktop file exists
touch ~/Desktop/Autostart
# Or: mv ~/Desktop/Autostart_DISABLED ~/Desktop/Autostart

# 3. Reboot to test
sudo reboot
# After boot: Hotspot created, web UI available at http://192.168.4.1:8080
```

### To DISABLE Autostart (either one works):

**Option A: Disable service (recommended - completely prevents boot startup)**
```bash
sudo systemctl disable drone-recorder.service
# Desktop file doesn't matter now - service won't run
```

**Option B: Remove/rename Desktop file (soft disable - service runs but exits)**
```bash
rm ~/Desktop/Autostart
# Or: mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED
# Service runs on boot but exits early without starting drone
```

## 🎯 Result

✅ Autostart control is now **reliable and user-controlled**  
✅ No hidden services bypass user preferences  
✅ System behavior is predictable and documented
