#!/bin/bash

# WiFi Recovery Script for Drone Web Controller
# This script can be called automatically when network issues are detected

echo "🔄 WiFi Recovery Script Starting..."

# Complete WiFi subsystem reset
echo "🔧 Resetting WiFi subsystem..."
sudo pkill hostapd
sudo pkill dnsmasq
sudo rfkill unblock wifi
sleep 2

# Reset interface completely
sudo ip link set dev wlP1p1s0 down
sudo ip addr flush dev wlP1p1s0
sudo iw dev wlP1p1s0 del 2>/dev/null || true
sleep 2

# Restart WiFi modules if needed
echo "🔄 Resetting WiFi hardware..."
sudo modprobe -r iwlwifi 2>/dev/null || true
sleep 1
sudo modprobe iwlwifi 2>/dev/null || true
sleep 3

# Recreate interface
sudo iw phy phy0 interface add wlP1p1s0 type managed 2>/dev/null || true
sleep 1

# Stop NetworkManager 
if systemctl is-active --quiet NetworkManager; then
    echo "🛑 Stopping NetworkManager..."
    sudo systemctl stop NetworkManager
    sleep 3
fi

# Configure interface step by step
echo "⚙️ Configuring interface..."
sudo iw dev wlP1p1s0 set type managed
sudo ip link set dev wlP1p1s0 up
sleep 1
sudo ip link set dev wlP1p1s0 down
sudo iw dev wlP1p1s0 set type __ap
sudo ip addr add 192.168.4.1/24 dev wlP1p1s0
sudo ip link set dev wlP1p1s0 up

# Power management off (ignore errors)
sudo iwconfig wlP1p1s0 power off 2>/dev/null || true
sleep 2

# Start hostapd
echo "📡 Starting hostapd..."
if [ ! -f /tmp/hostapd.conf ]; then
    echo "❌ hostapd.conf not found - cannot continue"
    exit 1
fi

sudo hostapd /tmp/hostapd.conf -B
sleep 5

# Start dnsmasq 
echo "🌐 Starting DHCP server..."
if [ ! -f /tmp/dnsmasq.conf ]; then
    echo "❌ dnsmasq.conf not found - cannot continue"
    exit 1
fi

sudo dnsmasq -C /tmp/dnsmasq.conf

# Verify services are running
echo "🔍 Verifying services..."
sleep 2

if pgrep hostapd > /dev/null; then
    echo "✅ hostapd is running"
else
    echo "❌ hostapd failed to start"
    exit 1
fi

if pgrep dnsmasq > /dev/null; then
    echo "✅ dnsmasq is running"
else
    echo "❌ dnsmasq failed to start"
    exit 1
fi

if ip addr show wlP1p1s0 | grep -q "192.168.4.1"; then
    echo "✅ Interface configured correctly"
else
    echo "❌ Interface configuration failed"
    exit 1
fi

echo "🎉 WiFi AP recovery successful!"
echo "📶 Network: DroneController"
echo "🔐 Password: drone123"
echo "🌐 Web UI: http://192.168.4.1:8080"
exit 0