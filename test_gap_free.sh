#!/bin/bash

echo "🎬 GAP-FREE RECORDING TEST"
echo "========================="
echo "Testing optimized recording with minimal sync operations"
echo "Focus: Eliminate 1-minute gaps in middle of recording"
echo

# Build first
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh

echo
echo "🚀 Starting gap-free test recording (training profile)..."
echo "Watch for:"
echo "  ❌ Frame gap warnings (should be minimal)"
echo "  ✅ Consistent file size growth"
echo "  ✅ Reduced sync frequency"
echo

cd build/apps/smart_recorder
sudo ./smart_recorder training

echo
echo "📊 POST-TEST ANALYSIS:"
LATEST_DIR=$(ls -1t /media/angelo/DRONE_DATA/flight_* | head -1)
if [ -d "$LATEST_DIR" ]; then
    echo "Latest recording: $LATEST_DIR"
    echo "File size: $(ls -lh "$LATEST_DIR"/video.svo* | awk '{print $5}')"
    echo
    echo "🎯 TEST THE VIDEO:"
    echo "1. Open in ZED Explorer"
    echo "2. Check for 1-minute gaps in middle"
    echo "3. Verify smooth continuous footage"
    echo
    echo "Opening ZED Explorer..."
    /usr/local/zed/tools/ZED_Explorer "$LATEST_DIR"/video.svo2 &
else
    echo "❌ No recording found"
fi