#!/bin/bash

# Test if large SVO2 file can be opened without corruption
TEST_FILE="/media/angelo/DRONE_DATA/flight_20251027_120532/video.svo2"

echo "🧪 TESTING 4GB+ SVO2 FILE INTEGRITY"
echo "=================================="
echo "File: $TEST_FILE"
echo "Size: $(ls -lh "$TEST_FILE" | awk '{print $5}')"
echo

if [ ! -f "$TEST_FILE" ]; then
    echo "❌ Test file not found!"
    exit 1
fi

echo "📊 File Statistics:"
echo "Size in bytes: $(stat -c%s "$TEST_FILE")"
echo "Last modified: $(stat -c%y "$TEST_FILE")"
echo

# Check if ZED tools can read the file header
echo "🔍 Testing file with ZED tools..."

# Try to use ZED_Explorer to get basic info (this will test if file opens without corruption)
if command -v /usr/local/zed/tools/ZED_Explorer >/dev/null 2>&1; then
    echo "ZED_Explorer found. Opening file..."
    echo "Note: ZED Explorer should open without showing corruption dialog"
    echo "If corruption dialog appears, our 4GB+ fix failed."
    echo "If it opens normally, our 4GB+ fix worked!"
    echo
    echo "Opening ZED Explorer now..."
    /usr/local/zed/tools/ZED_Explorer "$TEST_FILE" &
    echo "ZED_Explorer started with PID $!"
else
    echo "❌ ZED_Explorer not found in expected location"
fi

echo
echo "🎯 MANUAL TEST INSTRUCTIONS:"
echo "1. ZED Explorer should open automatically"
echo "2. If you see a 'File corruption detected' dialog = FIX FAILED ❌"
echo "3. If video plays normally without errors = FIX WORKED ✅"
echo "4. Check if full 3-minute duration is available"