# Drone Controller - Shutdown & Control Reference

**Last Updated:** November 18, 2025 (v1.5.4)

## Stop/Start Commands

### Stop Service (Cleanup, Jetson Stays Running)
```bash
# Recommended: Use systemctl
sudo systemctl stop drone-recorder

# Alternative: Use stop script directly
sudo /home/angelo/Projects/Drone-Fieldtest/scripts/stop_drone.sh

# Result:
# ✓ Web server stopped
# ✓ WiFi AP torn down (Ethernet restored)
# ✓ ZED camera closed
# ✓ USB unmounted
# ✗ Jetson still running (no system shutdown)
```

### Start Service
```bash
sudo systemctl start drone-recorder

# Wait 10 seconds for initialization, then check:
systemctl status drone-recorder
```

### Restart Service (After Code Updates)
```bash
# Rebuild code
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh

# Restart service
sudo systemctl restart drone-recorder
```

## Autostart Control

### Disable Autostart (Two-Level System)
```bash
# Level 1: Disable systemd service (prevents boot startup)
sudo systemctl disable drone-recorder

# Level 2: Remove Desktop file (prevents script execution)
mv ~/Desktop/Autostart ~/Desktop/Autostart_DISABLED

# Both must be disabled to prevent autostart
```

### Enable Autostart
```bash
# Level 1: Enable systemd service
sudo systemctl enable drone-recorder

# Level 2: Restore Desktop file
mv ~/Desktop/Autostart_DISABLED ~/Desktop/Autostart

# Both must be enabled for autostart on boot
```

## System Shutdown Options

### Option 1: GUI Shutdown Button (Recommended for Field Use)
1. Connect to WiFi: `DroneController` (password: `drone123`)
2. Open: http://192.168.4.1:8080
3. Click: **"System Herunterfahren"** button
4. Result: Complete cleanup + Jetson shuts down

### Option 2: Terminal Shutdown
```bash
# Graceful: Cleanup then shutdown (10 second delay)
sudo shutdown -h now

# Immediate: No delay
sudo poweroff

# Reboot instead
sudo reboot
```

### Option 3: Keyboard (If Connected)
```bash
# Press Ctrl+C in terminal where drone controller is running
# Result: Signal handler triggers cleanup, then process exits
# Note: Does NOT shut down Jetson automatically
```

## What Each Method Does

| Method | Stop Service | Cleanup | Shutdown Jetson |
|--------|-------------|---------|----------------|
| **GUI Shutdown Button** | ✅ | ✅ | ✅ |
| **systemctl stop** | ✅ | ✅ | ❌ |
| **stop_drone.sh** | ✅ | ✅ | ❌ |
| **Ctrl+C** | ✅ | ✅ | ❌ |
| **shutdown -h now** | ✅ (via systemd) | ✅ | ✅ |

## Status Checks

```bash
# Check if service is running
systemctl status drone-recorder

# Check if process is running
ps aux | grep drone_web_controller | grep -v grep

# Check WiFi AP status
nmcli connection show --active | grep DroneController

# Check last 20 log lines
sudo journalctl -u drone-recorder -n 20

# Follow logs in real-time
sudo journalctl -u drone-recorder -f

# Check application log
tail -50 /tmp/drone_controller.log
```

## Common Scenarios

### Scenario 1: Update Code and Restart
```bash
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh
sudo systemctl restart drone-recorder
# Wait 10 seconds
systemctl status drone-recorder
```

### Scenario 2: Debug Why Service Won't Start
```bash
sudo journalctl -u drone-recorder -n 50 --no-pager
tail -50 /tmp/drone_controller.log
```

### Scenario 3: Clean Stop Before USB Removal
```bash
# IMPORTANT: Always stop service before removing USB
sudo systemctl stop drone-recorder
# Wait for "Shutdown complete" message
# Now safe to remove USB drive
```

### Scenario 4: Stuck Process (Emergency)
```bash
# Find PID
ps aux | grep drone_web_controller | grep -v grep

# Kill gracefully (SIGTERM)
sudo kill <PID>

# If still running after 5 seconds, force kill
sudo kill -9 <PID>

# Clean up stuck WiFi AP
sudo nmcli connection down DroneController
sudo nmcli connection delete DroneController
```

### Scenario 5: Field Shutdown (No GUI Access)
```bash
# SSH into Jetson
ssh angelo@192.168.4.1  # or 10.42.0.1

# Graceful shutdown
sudo shutdown -h now
```

## Aliases (If Configured in ~/.bashrc)

```bash
# Quick commands (requires drone_aliases.sh sourced)
drone           # Start system
drone-stop      # Stop system
drone-status    # Check status
drone-logs      # View logs
drone-restart   # Restart service
```

## Service File Locations

- **Active Service:** `/etc/systemd/system/drone-recorder.service`
- **Repo Copy:** `/home/angelo/Projects/Drone-Fieldtest/systemd/drone-recorder.service`
- **Autostart Script:** `/home/angelo/Projects/Drone-Fieldtest/apps/drone_web_controller/autostart.sh`
- **Start Script:** `/home/angelo/Projects/Drone-Fieldtest/scripts/start_drone.sh`
- **Stop Script:** `/home/angelo/Projects/Drone-Fieldtest/scripts/stop_drone.sh`

## Important Notes

1. **Always use `sudo systemctl stop` before removing USB drive** - Prevents data corruption
2. **GUI shutdown is the only method that powers off Jetson automatically**
3. **`systemctl stop` is for development** - Stops service but keeps Jetson running
4. **Desktop file controls script execution** - Service is master switch, Desktop file is secondary check
5. **WiFi AP auto-tears down on process exit** - No manual cleanup needed (NetworkManager handles it)

## Troubleshooting

### Service Shows "Failed" Status
```bash
sudo systemctl reset-failed drone-recorder
sudo systemctl restart drone-recorder
```

### WiFi AP Stuck Active After Stop
```bash
sudo nmcli connection down DroneController
sudo nmcli connection delete DroneController
```

### Process Won't Stop Gracefully
```bash
# Find PID
PID=$(pgrep -f drone_web_controller)

# Send SIGTERM
sudo kill $PID

# Wait 10 seconds, then check
ps -p $PID

# If still running, force kill
sudo kill -9 $PID
```

### Autostart Not Working
```bash
# Check both levels
systemctl is-enabled drone-recorder  # Should show "enabled"
ls -la ~/Desktop/Autostart           # File must exist (not Autostart_DISABLED)

# Check logs
sudo journalctl -u drone-recorder -b  # -b = since last boot
```

---

**Quick Emergency Stop:** `sudo systemctl stop drone-recorder && sudo nmcli connection down DroneController`

**Quick Full Shutdown:** Click GUI shutdown button OR `sudo shutdown -h now`
