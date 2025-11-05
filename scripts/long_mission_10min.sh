#!/bin/bash

# 10-Minuten Mission mit 2-Minuten-Segmenten
# Usage: ./long_mission_10min.sh

echo "=== LONG MISSION: 10 MINUTES (5x 2-minute segments) ==="
echo "Segment type: HD720@30fps LOSSLESS"
echo "Total segments: 5"
echo "Total duration: 10 minutes"
echo "===================================================="

EXECUTABLE_PATH="/home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder/smart_recorder"

# Überprüfe ob das Executable existiert
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "ERROR: smart_recorder executable not found at $EXECUTABLE_PATH"
    echo "Please run: ./scripts/build.sh first"
    exit 1
fi

# Erstelle Hauptverzeichnis für 10-Minuten-Mission
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
MAIN_DIR="/media/angelo/DRONE_DATA/long_mission_${TIMESTAMP}_10min"
mkdir -p "$MAIN_DIR"

echo "Creating 10-minute mission recording in: $MAIN_DIR"
echo ""

# Master Info Datei erstellen
cat > "$MAIN_DIR/mission_info.txt" << EOF
=== LONG MISSION RECORDING ===
Date: $(date)
Total Duration: 10 minutes
Segments: 5x 2-minute recordings
Profile: long_mission (HD720@30fps LOSSLESS)
Quality: Maximum (no compression artifacts)
Storage: Segmented for reliability

Segments:
EOF

# 5 Segmente à 2 Minuten
for i in {1..5}; do
    echo "=== SEGMENT $i/5 ==="
    echo "Starting segment $i at $(date)"
    echo "Expected duration: 2 minutes"
    
    # Segment Info zur Master-Datei hinzufügen
    echo "Segment $i: $(date)" >> "$MAIN_DIR/mission_info.txt"
    
    # Starte 2-Minuten-Aufnahme mit long_mission Profil
    echo "Executing: $EXECUTABLE_PATH long_mission"
    "$EXECUTABLE_PATH" long_mission
    
    RESULT=$?
    if [ $RESULT -ne 0 ]; then
        echo "ERROR: Segment $i failed with exit code $RESULT"
        echo "Trying USB recovery and retry..."
        
        # USB Recovery: Kurze Pause und Retry
        sleep 10
        echo "Retry attempt for segment $i..."
        "$EXECUTABLE_PATH" long_mission
        RESULT=$?
        
        if [ $RESULT -ne 0 ]; then
            echo "Segment $i: FAILED after retry (exit code $RESULT)" >> "$MAIN_DIR/mission_info.txt"
            echo "Mission aborted due to persistent camera failure"
            break
        else
            echo "Segment $i: SUCCESS after retry" >> "$MAIN_DIR/mission_info.txt"
        fi
    else
        echo "Segment $i completed successfully"
        echo "Segment $i: SUCCESS" >> "$MAIN_DIR/mission_info.txt"
    fi
    
    # Finde das neueste flight_ Verzeichnis
    LATEST_FLIGHT=$(ls -1t /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -1)
    if [ -n "$LATEST_FLIGHT" ]; then
        # Benenne es zu segment_X um und verschiebe es
        SEGMENT_DIR="$MAIN_DIR/segment_$(printf "%02d" $i)"
        mv "$LATEST_FLIGHT" "$SEGMENT_DIR"
        echo "Segment moved to: $SEGMENT_DIR"
        
        # Zeige Dateigröße
        if [ -f "$SEGMENT_DIR/video.svo2" ]; then
            SIZE=$(du -sh "$SEGMENT_DIR/video.svo2" | cut -f1)
            echo "Video file size: $SIZE"
            echo "Segment $i: File size $SIZE" >> "$MAIN_DIR/mission_info.txt"
        fi
    fi
    
    # USB-Recovery Pause zwischen Segmenten (15 Sekunden)
    if [ $i -lt 5 ]; then
        echo "USB recovery pause: waiting 15 seconds before next segment..."
        sleep 15
    fi
    
    echo ""
done

# Mission Summary
echo "=== MISSION COMPLETE ==="
echo "Total segments recorded: $i"
echo "Mission directory: $MAIN_DIR"

# Finalize mission info
cat >> "$MAIN_DIR/mission_info.txt" << EOF

=== MISSION SUMMARY ===
Completion time: $(date)
Total segments: $i/5
Mission directory: $MAIN_DIR

=== FILE STRUCTURE ===
EOF

# Liste alle Segmente in der Info-Datei auf
ls -la "$MAIN_DIR"/ >> "$MAIN_DIR/mission_info.txt"

echo ""
echo "Mission info saved to: $MAIN_DIR/mission_info.txt"
echo "All segments stored in: $MAIN_DIR/"

# Zeige Gesamtgröße
TOTAL_SIZE=$(du -sh "$MAIN_DIR" | cut -f1)
echo "Total mission size: $TOTAL_SIZE"

echo ""
echo "🚁 LONG MISSION COMPLETE! 🚁"