# Autostart Quick Reference

## 🎯 Current Status (After Nov 18 Cleanup)

**Service:** `drone-recorder.service` is **ENABLED** ✅  
**Desktop File:** `~/Desktop/Autostart` **EXISTS** ✅  
**Result:** System **WILL AUTO-START** on next boot 🚀

## 🔧 Two-Level Control System

### Level 1: Systemd Service (Master Switch)
- **What:** Controls whether the autostart script runs on boot
- **Check:** `systemctl is-enabled drone-recorder.service`

```bash
# Enable (service runs on boot)
sudo systemctl enable drone-recorder.service

# Disable (service won't run on boot)
sudo systemctl disable drone-recorder.service
```

### Level 2: Desktop File (Script Check)
- **What:** The autostart script checks for this file before proceeding
- **Check:** `ls ~/Desktop/Autostart`

```bash
# Enable (script proceeds with startup)
touch ~/Desktop/Autostart

# Disable (script exits without starting drone)
rm ~/Desktop/Autostart
# Or: mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED
```

## 📊 Control Matrix

| Systemd | Desktop File | Behavior |
|---------|-------------|----------|
| ✅ enabled | ✅ exists | **Auto-starts on boot** |
| ✅ enabled | ❌ missing | Service runs, script exits early, LCD shows "Disabled" |
| ❌ disabled | ✅ exists | Nothing happens (service doesn't run) |
| ❌ disabled | ❌ missing | Nothing happens (service doesn't run) |

## 🎛️ Common Scenarios

### I want auto-start on boot (BOTH required):
```bash
sudo systemctl enable drone-recorder.service
touch ~/Desktop/Autostart
sudo reboot
```

### I want to DISABLE auto-start (Option A - recommended):
```bash
sudo systemctl disable drone-recorder.service
# Desktop file doesn't matter now
sudo reboot
```

### I want to DISABLE auto-start (Option B - soft disable):
```bash
# Keep service enabled, but remove Desktop file
mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED
sudo reboot
# Service runs but exits early
```

### I want to temporarily disable without changing service:
```bash
# Just rename the Desktop file
mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED
# To re-enable: mv ~/Desktop/Autostart_DISABLED ~/Desktop/Autostart
```

## ⚠️ Why Two Levels?

1. **Systemd Service** = System-level control (requires sudo)
   - More permanent
   - Service won't run at all if disabled
   - Cleaner shutdown (no wasted CPU)

2. **Desktop File** = User-level control (no sudo)
   - Quick toggle
   - Service still runs but exits gracefully
   - Shows "Autostart Disabled" on LCD

## 🚨 Common Mistake (What Happened After Cleanup)

**Problem:** Renamed Desktop file but nothing started on boot

**Why:** Service was **disabled** (Level 1 failed)
- Desktop file = ✅ exists (Level 2 ready)
- Systemd service = ❌ disabled (Level 1 blocked)
- **Result:** Service never ran, so Desktop file was never checked

**Fix:** Enable the service:
```bash
sudo systemctl enable drone-recorder.service
```

## 📱 Quick Check Command

```bash
echo "=== AUTOSTART STATUS ==="
echo "Service: $(systemctl is-enabled drone-recorder.service 2>&1)"
echo "Desktop: $(ls ~/Desktop/Autostart 2>&1 | grep -q Autostart && echo 'EXISTS' || echo 'MISSING')"
echo ""
if systemctl is-enabled drone-recorder.service 2>&1 | grep -q enabled && ls ~/Desktop/Autostart 2>/dev/null; then
    echo "✅ Auto-start is ENABLED"
else
    echo "❌ Auto-start is DISABLED"
fi
```

## 🔍 Troubleshooting

### After reboot, nothing starts:
```bash
# Check service status
systemctl status drone-recorder.service

# Check logs
sudo journalctl -u drone-recorder.service -b

# Verify both levels
systemctl is-enabled drone-recorder.service  # Should be 'enabled'
ls ~/Desktop/Autostart  # Should exist
```

### Service runs but drone doesn't start:
```bash
# Check if Desktop file is missing
ls ~/Desktop/Autostart
# If missing, create it:
touch ~/Desktop/Autostart
```

### Want to test without rebooting:
```bash
# Manually run the service
sudo systemctl start drone-recorder.service

# Check status
systemctl status drone-recorder.service

# View logs
sudo journalctl -u drone-recorder.service -f
```

---

**Updated:** November 18, 2025  
**Status:** Service enabled, Desktop file exists - auto-start active ✅
