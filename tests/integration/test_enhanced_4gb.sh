#!/bin/bash

echo "🧪 TESTING ENHANCED >4GB CORRUPTION FIXES"
echo "========================================"
echo

# Check if we have the enhanced build
if [ ! -f "./smart_recorder" ]; then
    echo "❌ smart_recorder not found in current directory"
    exit 1
fi

echo "📊 Previous 4GB test results:"
echo "Previous file (before fixes): Had corruption, lost 30s"
echo "This test: Enhanced shutdown sequence with multiple sync points"
echo

echo "🚀 Starting 3-minute HD1080@30fps recording..."
echo "Expected file size: ~7.2GB (definitely >4GB)"
echo "Enhanced features active:"
echo "  ✅ Pre-shutdown sync"
echo "  ✅ Extended buffer flush time (3s for >2GB files)"
echo "  ✅ More frequent sync intervals for large files"
echo "  ✅ Recording loop cleanup"
echo "  ✅ Final filesystem sync"
echo

# Run the test
sudo ./smart_recorder test_4gb_plus

echo
echo "🔍 POST-TEST ANALYSIS:"
LATEST_DIR=$(ls -1t /media/angelo/DRONE_DATA/flight_* | head -1)
if [ -d "$LATEST_DIR" ]; then
    echo "Latest recording: $LATEST_DIR"
    echo "File size: $(ls -lh "$LATEST_DIR"/video.svo* | awk '{print $5}')"
    echo
    echo "🎯 NEXT STEPS:"
    echo "1. Open file in ZED Explorer"
    echo "2. Check for corruption dialog"
    echo "3. Verify full 3-minute duration available"
    echo
    echo "File path: $LATEST_DIR/video.svo2"
else
    echo "❌ No recording directory found"
fi