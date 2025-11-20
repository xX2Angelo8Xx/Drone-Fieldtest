#!/bin/bash
# Quick Test Commands für v1.5.4 Testing

echo "═══════════════════════════════════════════════════════════════════"
echo "  QUICK TEST COMMANDS - v1.5.4"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

echo "🔧 SYSTEM CONTROL"
echo "─────────────────────────────────────────────────────────────────"
echo "sudo systemctl restart drone-recorder    # Service neu starten"
echo "sudo systemctl stop drone-recorder       # Service stoppen"
echo "sudo systemctl status drone-recorder     # Status prüfen"
echo ""

echo "📊 LOGS & MONITORING"
echo "─────────────────────────────────────────────────────────────────"
echo "sudo journalctl -u drone-recorder -f           # Live Logs"
echo "sudo journalctl -u drone-recorder -n 50        # Letzte 50 Zeilen"
echo "sudo journalctl -u drone-recorder | grep segment    # Kalibrierung prüfen"
echo ""

echo "🔍 VOLTAGE VERIFICATION"
echo "─────────────────────────────────────────────────────────────────"
echo "sudo ./test_2segment_calibration           # Direkter Voltage-Test"
echo "cat /home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json | head -10"
echo ""

echo "📹 RECORDING VERIFICATION"
echo "─────────────────────────────────────────────────────────────────"
echo "ls -lh /media/angelo/DRONE_DATA/flight_*/          # Recordings auflisten"
echo "cat /media/angelo/DRONE_DATA/flight_*/recording.log | tail -20"
echo "/usr/local/zed/tools/ZED_SVO_Editor -info /media/angelo/DRONE_DATA/flight_*/video.svo2"
echo ""

echo "🌐 NETWORK STATUS"
echo "─────────────────────────────────────────────────────────────────"
echo "nmcli connection show                    # Alle Connections"
echo "nmcli connection show --active           # Aktive Connections"
echo "ip addr show wlP1p1s0                    # WiFi Interface Status"
echo ""

echo "🔥 MANUAL START (für Ctrl+C Test)"
echo "─────────────────────────────────────────────────────────────────"
echo "sudo systemctl stop drone-recorder"
echo "cd /home/angelo/Projects/Drone-Fieldtest"
echo "sudo ./build/apps/drone_web_controller/drone_web_controller"
echo ""

echo "📱 WEB UI"
echo "─────────────────────────────────────────────────────────────────"
echo "URL: http://192.168.4.1:8080"
echo "WiFi: SSID=DroneController, Password=drone123"
echo ""

