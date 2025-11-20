#!/bin/bash
# ZED CLI Recorder Test Script für 4GB+ Aufnahmen
# Verwendung: ./test_zed_cli_4min.sh

echo "🎬 ZED CLI RECORDER - 4GB+ CORRUPTION TEST"
echo "========================================"
echo "Testing ZED Explorer backend for corruption-free >4GB recording"
echo ""

PROJECT_ROOT="/home/angelo/Projects/Drone-Fieldtest"
ZED_CLI_RECORDER="$PROJECT_ROOT/build/apps/zed_cli_recorder/zed_cli_recorder"

# Check if binary exists
if [ ! -f "$ZED_CLI_RECORDER" ]; then
    echo "❌ ZED CLI Recorder not found. Building..."
    cd "$PROJECT_ROOT"
    ./scripts/build.sh
fi

echo "🚀 Starting 4-minute HD720@15fps recording..."
echo "   This should create a ~3.5GB file without corruption"
echo "   (Previous tests showed corruption at ~4.3GB)"
echo ""

# Run the recorder for 4 minutes (240 seconds)
cd "$PROJECT_ROOT"
timeout 250s ./build/apps/zed_cli_recorder/zed_cli_recorder 240 HD720 15

echo ""
echo "📊 ANALYZING RESULT..."

# Find the latest recording
LATEST_FLIGHT=$(ls -td /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -1)

if [ -n "$LATEST_FLIGHT" ]; then
    echo "Latest recording: $LATEST_FLIGHT"
    
    if [ -f "$LATEST_FLIGHT/video.svo" ]; then
        FILE_SIZE=$(stat -c%s "$LATEST_FLIGHT/video.svo")
        FILE_SIZE_MB=$((FILE_SIZE / 1024 / 1024))
        FILE_SIZE_GB=$((FILE_SIZE_MB / 1024))
        
        echo "📄 Video file: $LATEST_FLIGHT/video.svo"
        echo "📊 File size: ${FILE_SIZE_MB}MB (${FILE_SIZE_GB}.${FILE_SIZE_MB%???}GB)"
        
        # Check if file is >4GB (4294967296 bytes)
        if [ $FILE_SIZE -gt 4294967296 ]; then
            echo "🎯 SUCCESS: File is >4GB - testing for corruption..."
        elif [ $FILE_SIZE -gt 3000000000 ]; then
            echo "✅ Large file created (>3GB) - should be safe from corruption"
        else
            echo "⚠️  File smaller than expected"
        fi
        
        # Test file integrity with ZED Explorer
        echo ""
        echo "🔍 Testing file integrity with ZED Explorer..."
        echo "   (Will try to open file - if it fails, file is corrupted)"
        
        # Use timeout to prevent hanging if file is corrupted
        timeout 10s /usr/local/zed/tools/ZED_Explorer --input "$LATEST_FLIGHT/video.svo" &
        ZED_PID=$!
        sleep 3
        
        # Check if ZED Explorer is still running (good sign)
        if kill -0 $ZED_PID 2>/dev/null; then
            echo "✅ ZED Explorer can open the file - integrity looks good!"
            kill $ZED_PID 2>/dev/null
        else
            echo "❌ ZED Explorer crashed or failed to open - possible corruption"
        fi
        
    else
        echo "❌ No video.svo file found in latest recording"
    fi
else
    echo "❌ No recording directory found"
fi

echo ""
echo "🏁 Test completed. Check results above."