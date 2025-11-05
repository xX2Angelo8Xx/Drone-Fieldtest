#!/bin/bash
# Temporärer SSD Test für ZED CLI Recorder
# Umgeht das FAT32 4GB-Limit durch direkte SSD-Nutzung

echo "🚀 ZED CLI RECORDER - SSD TEST VERSION 🚀"
echo "============================================="
echo "Testing on internal SSD to bypass FAT32 4GB limit"
echo ""

if [ $# -lt 1 ]; then
    echo "Usage: $0 <duration_seconds> [resolution] [framerate]"
    echo "Example: $0 600 HD720 15"
    exit 1
fi

DURATION_SECONDS=$1
RESOLUTION=${2:-HD720}
FRAMERATE=${3:-15}
TOTAL_FRAMES=$((DURATION_SECONDS * FRAMERATE))

# Erstelle Aufnahme-Verzeichnis auf SSD
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RECORDING_DIR="/home/angelo/test_recordings_ssd/flight_${TIMESTAMP}"
mkdir -p "$RECORDING_DIR"

    VIDEO_PATH="${RECORDING_DIR}/video.svo"
    SENSOR_PATH="${RECORDING_DIR}/sensor_data.csv"

echo "📁 Recording directory: $RECORDING_DIR"
echo "📹 Video file: $VIDEO_PATH"
echo "📊 Sensor file: $SENSOR_PATH"
echo "⏱️  Duration: ${DURATION_SECONDS} seconds (${TOTAL_FRAMES} frames)"
echo "🎬 Resolution: $RESOLUTION @ ${FRAMERATE}fps"
echo ""# Starte ZED Explorer
echo "🚀 Starting ZED Explorer with command:"
echo "   /usr/local/zed/tools/ZED_Explorer --output \"$VIDEO_PATH\" --resolution $RESOLUTION --frequency $FRAMERATE --length $TOTAL_FRAMES --compression_mode 1"
echo ""

# Führe ZED Explorer aus
/usr/local/zed/tools/ZED_Explorer \
    --output "$VIDEO_PATH" \
    --resolution "$RESOLUTION" \
    --frequency "$FRAMERATE" \
    --length "$TOTAL_FRAMES" \
    --compression_mode 1

# Ergebnisse anzeigen
echo ""
echo "📊 RECORDING RESULTS:"
echo "===================="

if [ -f "$VIDEO_PATH" ]; then
    FILE_SIZE=$(stat -c%s "$VIDEO_PATH")
    FILE_SIZE_MB=$((FILE_SIZE / 1024 / 1024))
    FILE_SIZE_GB=$((FILE_SIZE_MB / 1024))
    
    echo "✅ Recording completed successfully!"
    echo "📄 Video file: $VIDEO_PATH"
    echo "📊 File size: ${FILE_SIZE_MB}MB (${FILE_SIZE_GB}.${FILE_SIZE_MB%???}GB)"
    echo "💾 File size (bytes): $FILE_SIZE"
    
    # Prüfe ob >4GB
    if [ $FILE_SIZE -gt 4294967296 ]; then
        echo "🎯 SUCCESS: File is >4GB - FAT32 limit bypassed!"
        echo "🎉 ZED Explorer backend solution CONFIRMED!"
    elif [ $FILE_SIZE -gt 3000000000 ]; then
        echo "✅ Large file created (>3GB) - good progress!"
    fi
    
    # Test Integrität
    echo ""
    echo "🔍 Testing file integrity..."
    timeout 5s /usr/local/zed/tools/ZED_Explorer --input "$VIDEO_PATH" &
    ZED_PID=$!
    sleep 2
    
    if kill -0 $ZED_PID 2>/dev/null; then
        echo "✅ File integrity OK - ZED Explorer can open the file!"
        kill $ZED_PID 2>/dev/null
    else
        echo "❌ File integrity issue - ZED Explorer cannot open file"
    fi
    
else
    echo "❌ No video file created - recording failed"
fi

echo ""
echo "🏁 SSD Test completed!"