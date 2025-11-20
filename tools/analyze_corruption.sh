#!/bin/bash

echo "=== SVO FILE CORRUPTION ANALYSIS ==="
echo "Date: $(date)"

LATEST_DIR=$(ls -td /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -1)

if [ -z "$LATEST_DIR" ]; then
    echo "No flight directories found"
    exit 1
fi

echo "Latest recording: $LATEST_DIR"

# Check if SVO file exists (try both .svo and .svo2)
SVO_FILE="$LATEST_DIR/video.svo"
if [ ! -f "$SVO_FILE" ]; then
    SVO_FILE="$LATEST_DIR/video.svo2"
fi

if [ -f "$SVO_FILE" ]; then
    echo "SVO File: $SVO_FILE"
    echo "File size: $(du -h "$SVO_FILE" | cut -f1)"
    echo "File permissions: $(ls -l "$SVO_FILE")"
    
    # Try to get basic file info
    echo -e "\n=== FILE ANALYSIS ==="
    echo "File type: $(file "$SVO_FILE")"
    
    # Check if file is completely written (no partial write)
    echo "Last modification: $(stat -c '%y' "$SVO_FILE")"
    
    # Try hexdump of first and last bytes
    echo -e "\n=== HEADER (first 32 bytes) ==="
    hexdump -C "$SVO_FILE" | head -3
    
    echo -e "\n=== FOOTER (last 32 bytes) ==="
    tail -c 32 "$SVO_FILE" | hexdump -C
    
    # Check for abrupt endings or incomplete writes
    echo -e "\n=== INTEGRITY CHECK ==="
    if [ -s "$SVO_FILE" ]; then
        echo "File has content (not zero-sized)"
        
        # Simple corruption detection: check if file ends abruptly
        FILESIZE=$(stat -c%s "$SVO_FILE")
        echo "Exact file size: $FILESIZE bytes"
        
        if [ $FILESIZE -gt 1000000 ]; then
            echo "File size seems reasonable for 7min recording"
        else
            echo "WARNING: File size seems too small"
        fi
    else
        echo "ERROR: File is zero-sized"
    fi
    
else
    echo "No SVO file found in $LATEST_DIR"
fi

# Check sensor file too
SENSOR_FILE="$LATEST_DIR/sensors.csv"
if [ -f "$SENSOR_FILE" ]; then
    echo -e "\n=== SENSOR FILE ==="
    echo "Sensor file: $SENSOR_FILE"
    echo "Size: $(du -h "$SENSOR_FILE" | cut -f1)"
    echo "Lines: $(wc -l < "$SENSOR_FILE")"
    echo "Last few entries:"
    tail -3 "$SENSOR_FILE"
fi

echo -e "\n=== SYSTEM INFO ==="
echo "Current mount status:"
mount | grep DRONE_DATA

echo "USB device status:"
lsusb | grep -i stereo