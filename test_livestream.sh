#!/bin/bash
# Livestream Performance Test Script
# Monitors network and CPU during livestream testing

echo "🚁 DRONE LIVESTREAM TEST SETUP"
echo "======================================"
echo ""

# Check if monitoring tools are installed
if ! command -v iftop &> /dev/null; then
    echo "❌ iftop not found. Installing..."
    sudo apt-get install -y iftop
fi

if ! command -v htop &> /dev/null; then
    echo "❌ htop not found. Installing..."
    sudo apt-get install -y htop
fi

echo "✅ Monitoring tools ready"
echo ""
echo "📋 SETUP INSTRUCTIONS:"
echo "======================================"
echo ""
echo "You need 3 terminals for this test:"
echo ""
echo "Terminal 1 (Network Monitor):"
echo "  sudo iftop -i wlP1p1s0"
echo ""
echo "Terminal 2 (CPU Monitor):"
echo "  htop"
echo ""
echo "Terminal 3 (Controller - THIS TERMINAL):"
echo "  Will start automatically in 5 seconds..."
echo ""
echo "📱 After controller starts:"
echo "  1. Connect phone/laptop to WiFi: DroneController"
echo "  2. Open browser: http://10.42.0.1:8080"
echo "  3. Go to Livestream tab"
echo "  4. Enable livestream checkbox"
echo "  5. Try different FPS settings (2, 5, 10, 15)"
echo "  6. Watch iftop for network usage!"
echo ""
echo "⚠️  IMPORTANT:"
echo "  - iftop will show 0 until a client connects"
echo "  - htop will show ~5-10% until recording starts"
echo "  - Recording alone = 65-70% CPU"
echo "  - Recording + Livestream @ 10 FPS = ~70-75% CPU"
echo ""

read -p "Press Enter to start drone_web_controller (or Ctrl+C to abort)..."

echo ""
echo "🚀 Starting Drone Web Controller..."
echo ""

cd ~/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller
