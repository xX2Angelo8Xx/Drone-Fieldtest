#!/bin/bash
# Power Management Disable Script für Drone Recording

echo "=== DISABLING POWER MANAGEMENT FOR RECORDING ==="

# 1. Disable screen blanking/screensaver
echo "Disabling screen blanking..."
export DISPLAY=:0
xset s off
xset -dpms
xset s noblank

# 2. Disable USB autosuspend
echo "Disabling USB autosuspend..."
for usb in /sys/bus/usb/devices/*/power/autosuspend_delay_ms; do
    if [ -w "$usb" ]; then
        echo -1 > "$usb" 2>/dev/null || true
    fi
done

for usb in /sys/bus/usb/devices/*/power/control; do
    if [ -w "$usb" ]; then
        echo on > "$usb" 2>/dev/null || true
    fi
done

# 3. Set CPU governor to performance
echo "Setting CPU governor to performance..."
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -w "$cpu" ]; then
        echo performance > "$cpu" 2>/dev/null || true
    fi
done

# 4. Disable system suspend/hibernate
echo "Disabling system suspend..."
systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target 2>/dev/null || true

# 5. Keep system active with caffeine-like behavior
echo "Keeping system active..."
# Simulate user activity every 30 seconds
while true; do
    # Move mouse cursor slightly (invisible)
    export DISPLAY=:0
    xdotool mousemove_relative 1 1 2>/dev/null || true
    sleep 30
    xdotool mousemove_relative -- -1 -1 2>/dev/null || true
    sleep 30
done &

echo "Power management disabled. PID: $!"
echo "To re-enable: sudo systemctl unmask sleep.target suspend.target hibernate.target hybrid-sleep.target"