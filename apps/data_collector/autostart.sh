#!/bin/bash
set -e

# Drone Field Recording - Autologin Autostart Script
echo "=== DRONE FIELD RECORDER AUTOSTART ==="
echo "Timestamp: $(date)"
echo "User: $(whoami)"
echo "Working Directory: $(pwd)"

# Disable power management for stable recording
echo "Disabling power management..."
export DISPLAY=:0
xset s off 2>/dev/null || true
xset -dpms 2>/dev/null || true
xset s noblank 2>/dev/null || true

# Disable USB autosuspend
echo "Disabling USB autosuspend..."
for usb in /sys/bus/usb/devices/*/power/control; do
    if [ -w "$usb" ]; then
        echo on > "$usb" 2>/dev/null || true
    fi
done

# Set CPU to performance mode
echo "Setting CPU to performance mode..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true

# Pfad zum Smart Recorder (optimiert für AI-Training)
EXECUTABLE_PATH="/home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder/smart_recorder"

# Prüfen ob die ausführbare Datei existiert
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "Error: Smart Recorder not found at $EXECUTABLE_PATH"
    exit 1
fi

# Warte auf vollständiges System-Boot und USB-Geräte
echo "Waiting for system boot completion and USB devices..."
sleep 15

# Warte auf Netzwerk und weitere Services
echo "Waiting for all services to be ready..."
sleep 5

# Starte das Programm mit realtime_30fps Profil (optimiert für Feldaufnahmen)
echo "Starting Smart Recorder with realtime_30fps profile..."
echo "Mode: HD720@30FPS with Frame Drop Prevention"
"$EXECUTABLE_PATH" long_mission

echo "=== AUTOSTART COMPLETED ==="

# Falls das Programm beendet wird
echo "Data collector exited with code $?"

