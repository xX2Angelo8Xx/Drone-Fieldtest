#!/bin/bash

# Segmentierte Aufnahme - Mehrere 1-Minuten-Segmente für längere Gesamtaufnahme

TOTAL_MINUTES=${1:-5}  # Default: 5 Minuten total
SEGMENT_SECONDS=60     # 1 Minute pro Segment

echo "=== SEGMENTED RECORDING ==="
echo "Total duration: ${TOTAL_MINUTES} minutes"
echo "Segment length: ${SEGMENT_SECONDS} seconds"
echo "Number of segments: ${TOTAL_MINUTES}"

EXECUTABLE_PATH="/home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder/smart_recorder"

# Erstelle Hauptverzeichnis für segmentierte Aufnahme
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
MAIN_DIR="/media/angelo/DRONE_DATA/segmented_${TIMESTAMP}_${TOTAL_MINUTES}min"
mkdir -p "$MAIN_DIR"

echo "Creating segmented recording in: $MAIN_DIR"

for i in $(seq 1 $TOTAL_MINUTES); do
    echo ""
    echo "=== SEGMENT $i/$TOTAL_MINUTES ==="
    echo "Starting segment $i at $(date)"
    
    # Starte 1-Minuten-Aufnahme
    "$EXECUTABLE_PATH" long_mission
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Segment $i failed!"
        exit 1
    fi
    
    # Finde die neueste Aufnahme und verschiebe sie ins Segmentverzeichnis
    LATEST_FLIGHT=$(ls -td /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -1)
    if [ -n "$LATEST_FLIGHT" ]; then
        SEGMENT_NAME="segment_$(printf "%02d" $i)_of_${TOTAL_MINUTES}"
        mv "$LATEST_FLIGHT" "$MAIN_DIR/$SEGMENT_NAME"
        echo "Moved recording to: $MAIN_DIR/$SEGMENT_NAME"
        
        # Erstelle Info-Datei für das Segment
        echo "Segment: $i/$TOTAL_MINUTES" > "$MAIN_DIR/$SEGMENT_NAME/segment_info.txt"
        echo "Recorded: $(date)" >> "$MAIN_DIR/$SEGMENT_NAME/segment_info.txt"
        echo "Duration: ${SEGMENT_SECONDS}s" >> "$MAIN_DIR/$SEGMENT_NAME/segment_info.txt"
        echo "Part of: segmented_${TIMESTAMP}_${TOTAL_MINUTES}min" >> "$MAIN_DIR/$SEGMENT_NAME/segment_info.txt"
    fi
    
    echo "Segment $i completed at $(date)"
    
    # Kurze Pause zwischen Segmenten (5 Sekunden)
    if [ $i -lt $TOTAL_MINUTES ]; then
        echo "Waiting 5 seconds before next segment..."
        sleep 5
    fi
done

echo ""
echo "=== SEGMENTED RECORDING COMPLETED ==="
echo "Recorded $TOTAL_MINUTES segments successfully"
echo "Location: $MAIN_DIR"
echo ""
echo "Segments created:"
ls -la "$MAIN_DIR"
echo ""
echo "Total size:"
du -sh "$MAIN_DIR"

# Erstelle Master-Info-Datei
echo "=== SEGMENTED RECORDING MASTER INFO ===" > "$MAIN_DIR/recording_info.txt"
echo "Created: $(date)" >> "$MAIN_DIR/recording_info.txt"
echo "Total duration: ${TOTAL_MINUTES} minutes" >> "$MAIN_DIR/recording_info.txt"
echo "Segments: ${TOTAL_MINUTES}" >> "$MAIN_DIR/recording_info.txt"
echo "Segment duration: ${SEGMENT_SECONDS}s each" >> "$MAIN_DIR/recording_info.txt"
echo "Recording mode: HD720@30fps LOSSLESS" >> "$MAIN_DIR/recording_info.txt"
echo "" >> "$MAIN_DIR/recording_info.txt"
echo "Segments:" >> "$MAIN_DIR/recording_info.txt"
for j in $(seq 1 $TOTAL_MINUTES); do
    SEGMENT_NAME="segment_$(printf "%02d" $j)_of_${TOTAL_MINUTES}"
    if [ -d "$MAIN_DIR/$SEGMENT_NAME" ]; then
        echo "  $SEGMENT_NAME/ - Contains video.svo2 and sensors.csv" >> "$MAIN_DIR/recording_info.txt"
    fi
done

echo "Master info file created: $MAIN_DIR/recording_info.txt"