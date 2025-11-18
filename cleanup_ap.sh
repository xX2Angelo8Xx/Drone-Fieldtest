#!/bin/bash
# WiFi AP Cleanup Script
# Use this if drone_web_controller crashed without cleaning up the hotspot
# Author: Angelo (Drone-Fieldtest Project)
# Date: 2025-11-18

echo "🔧 WiFi AP Cleanup Script"
echo "=========================="
echo ""
echo "This script will:"
echo "  1. Stop any active DroneController WiFi hotspot"
echo "  2. Delete the DroneController connection profile"
echo "  3. Restore normal Ethernet internet access"
echo ""

# Check if DroneController connection exists
if nmcli connection show | grep -q "DroneController"; then
    echo "📡 Found existing DroneController connection"
    
    # Try to stop it (might fail if already down, that's OK)
    echo "⏹️  Stopping DroneController hotspot..."
    sudo nmcli connection down DroneController 2>/dev/null
    
    if [ $? -eq 0 ]; then
        echo "✅ Hotspot stopped successfully"
    else
        echo "⚠️  Hotspot was already stopped (not running)"
    fi
    
    # Delete the connection profile
    echo "🗑️  Deleting DroneController connection profile..."
    sudo nmcli connection delete DroneController
    
    if [ $? -eq 0 ]; then
        echo "✅ Connection profile deleted"
    else
        echo "❌ Failed to delete connection profile"
        exit 1
    fi
else
    echo "✅ No DroneController connection found (already clean)"
fi

echo ""
echo "🔍 Checking WiFi interface state..."
ip link show wlP1p1s0 | grep -E "state|mtu"

echo ""
echo "🌐 Checking internet connectivity..."
if ping -c 1 -W 2 8.8.8.8 &> /dev/null; then
    echo "✅ Internet access working (via Ethernet)"
else
    echo "⚠️  No internet access detected"
    echo "    Check Ethernet cable connection"
fi

echo ""
echo "📊 Active network connections:"
nmcli connection show --active | head -n 5

echo ""
echo "✅ Cleanup complete!"
echo ""
echo "You can now start the controller again:"
echo "  sudo ./build/apps/drone_web_controller/drone_web_controller"
