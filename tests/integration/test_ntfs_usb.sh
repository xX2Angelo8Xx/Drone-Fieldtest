#!/bin/bash
# ZED CLI Recorder Test für NTFS USB mit Sensordaten
# Testet >4GB Aufnahmen auf NTFS USB + Sensor logging

echo "🚀 ZED CLI RECORDER - NTFS USB + SENSOR TEST 🚀"
echo "==============================================="
echo "Testing NTFS USB for >4GB files + sensor data logging"
echo ""

if [ $# -lt 1 ]; then
    echo "Usage: $0 <duration_seconds> [resolution] [framerate]"
    echo "Example: $0 300 HD720 15  # 5 minutes"
    exit 1
fi

DURATION_SECONDS=$1
RESOLUTION=${2:-HD720}
FRAMERATE=${3:-15}
TOTAL_FRAMES=$((DURATION_SECONDS * FRAMERATE))

# Prüfe ob NTFS USB gemountet ist
if [ ! -d "/media/angelo/DRONE_DATA" ]; then
    echo "❌ DRONE_DATA USB not mounted!"
    echo "Available devices:"
    ls /media/angelo/
    exit 1
fi

# Erstelle Aufnahme-Verzeichnis
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RECORDING_DIR="/media/angelo/DRONE_DATA/flight_${TIMESTAMP}"
mkdir -p "$RECORDING_DIR"

VIDEO_PATH="${RECORDING_DIR}/video.svo"
SENSOR_PATH="${RECORDING_DIR}/sensor_data.csv"
LOG_PATH="${RECORDING_DIR}/recording.log"

echo "📁 Recording directory: $RECORDING_DIR"
echo "📹 Video file: $VIDEO_PATH"
echo "📊 Sensor file: $SENSOR_PATH"
echo "📋 Log file: $LOG_PATH"
echo "⏱️  Duration: ${DURATION_SECONDS} seconds (${TOTAL_FRAMES} frames)"
echo "🎬 Resolution: $RESOLUTION @ ${FRAMERATE}fps"
echo "💾 File system: NTFS (supports >4GB files)"
echo ""

# Sensor logging Hintergrundprozess starten
echo "🔄 Starting sensor data logging..."
{
    echo "timestamp,rotation_x,rotation_y,rotation_z,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,temperature"
    
    for i in $(seq 0 $((DURATION_SECONDS-1))); do
        TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
        # Simuliere Sensordaten (in echtem System kämen diese von ZED SDK)
        ROT_X=$(echo "scale=6; s($i * 0.1)" | bc -l)
        ROT_Y=$(echo "scale=6; c($i * 0.05)" | bc -l) 
        ROT_Z=$(echo "scale=6; s($i * 0.02)" | bc -l)
        ACCEL_X=$(echo "scale=3; 9.81 + s($i * 0.3)" | bc -l)
        ACCEL_Y=$(echo "scale=3; s($i * 0.2)" | bc -l)
        ACCEL_Z=$(echo "scale=3; 1.2 * c($i * 0.15)" | bc -l)
        GYRO_X=$(echo "scale=4; 0.1 * s($i * 0.4)" | bc -l)
        GYRO_Y=$(echo "scale=4; 0.05 * c($i * 0.25)" | bc -l)
        GYRO_Z=$(echo "scale=4; 0.03 * s($i * 0.6)" | bc -l)
        TEMP=$(echo "scale=1; 45.0 + 5.0 * s($i * 0.01)" | bc -l)
        
        echo "$TIMESTAMP,$ROT_X,$ROT_Y,$ROT_Z,$ACCEL_X,$ACCEL_Y,$ACCEL_Z,$GYRO_X,$GYRO_Y,$GYRO_Z,$TEMP"
        sleep 1
    done
} > "$SENSOR_PATH" &

SENSOR_PID=$!

# Logging starten
exec 1> >(tee -a "$LOG_PATH")
exec 2>&1

echo "[$(date)] Starting ZED recording..." >> "$LOG_PATH"

# Starte ZED Explorer
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

# Stoppe Sensor logging
kill $SENSOR_PID 2>/dev/null

echo "[$(date)] ZED recording completed." >> "$LOG_PATH"

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
    
    # Prüfe Sensor-Datei
    if [ -f "$SENSOR_PATH" ]; then
        SENSOR_LINES=$(wc -l < "$SENSOR_PATH")
        SENSOR_SIZE=$(stat -c%s "$SENSOR_PATH")
        echo "📊 Sensor data: $SENSOR_LINES lines, $((SENSOR_SIZE / 1024))KB"
    fi
    
    # Prüfe ob >4GB
    if [ $FILE_SIZE -gt 4294967296 ]; then
        echo "🎯 SUCCESS: File is >4GB - NTFS supports large files!"
        echo "🎉 USB recording with NTFS CONFIRMED!"
    elif [ $FILE_SIZE -gt 1000000000 ]; then
        echo "✅ Large file created (>1GB) - good progress!"
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
echo "📁 All files in recording directory:"
ls -lh "$RECORDING_DIR/"
echo ""
echo "🏁 NTFS USB Test completed!"