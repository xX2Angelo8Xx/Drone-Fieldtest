#!/bin/bash

# 3-Minuten Performance Test mit 1-Minuten-Segmenten
# Usage: ./quick_performance_test.sh

echo "=== PERFORMANCE TEST: 3 MINUTES (3x 1-minute segments) ==="
echo "Testing optimized stop times between segments"
echo "Segment type: HD720@30fps LOSSLESS"
echo "Total segments: 3"
echo "Test duration: ~3-4 minutes total"
echo "=========================================================="

EXECUTABLE_PATH="/home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder/smart_recorder"

# Überprüfe ob das Executable existiert
if [ ! -f "$EXECUTABLE_PATH" ]; then
    echo "ERROR: smart_recorder executable not found at $EXECUTABLE_PATH"
    echo "Please run: ./scripts/build.sh first"
    exit 1
fi

# Erstelle Test-Verzeichnis
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
MAIN_DIR="/media/angelo/DRONE_DATA/perf_test_${TIMESTAMP}_3min"
mkdir -p "$MAIN_DIR"

echo "Creating performance test in: $MAIN_DIR"
echo ""

# Master Info Datei für Performance-Tracking
cat > "$MAIN_DIR/performance_test.txt" << EOF
=== PERFORMANCE TEST RECORDING ===
Date: $(date)
Test Duration: 3 minutes
Segments: 3x 1-minute recordings
Profile: realtime_30fps (HD720@30fps LOSSLESS)
Purpose: Test optimized stop times

Performance Metrics:
EOF

# 3 Segmente à 1 Minute
for i in {1..3}; do
    echo "=== SEGMENT $i/3 ==="
    echo "Starting segment $i at $(date)"
    
    # Zeit-Tracking für Performance-Messung
    start_segment=$(date +%s)
    echo "Segment $i start: $(date)" >> "$MAIN_DIR/performance_test.txt"
    
    # Starte 1-Minuten-Aufnahme mit realtime_30fps Profil
    echo "Executing: $EXECUTABLE_PATH realtime_30fps"
    "$EXECUTABLE_PATH" realtime_30fps
    
    end_segment=$(date +%s)
    segment_duration=$((end_segment - start_segment))
    
    RESULT=$?
    if [ $RESULT -ne 0 ]; then
        echo "ERROR: Segment $i failed with exit code $RESULT"
        echo "Segment $i: FAILED (exit code $RESULT) - Duration: ${segment_duration}s" >> "$MAIN_DIR/performance_test.txt"
        break
    else
        echo "Segment $i completed successfully in ${segment_duration} seconds"
        echo "Segment $i: SUCCESS - Duration: ${segment_duration}s" >> "$MAIN_DIR/performance_test.txt"
    fi
    
    # Finde das neueste flight_ Verzeichnis und organisiere es
    LATEST_FLIGHT=$(ls -1t /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -1)
    if [ -n "$LATEST_FLIGHT" ]; then
        # Benenne es zu segment_X um und verschiebe es
        SEGMENT_DIR="$MAIN_DIR/segment_$(printf "%02d" $i)"
        mv "$LATEST_FLIGHT" "$SEGMENT_DIR"
        echo "Segment moved to: $SEGMENT_DIR"
        
        # Zeige Dateigröße für Performance-Analyse
        if [ -f "$SEGMENT_DIR/video.svo2" ]; then
            SIZE=$(du -sh "$SEGMENT_DIR/video.svo2" | cut -f1)
            echo "Video file size: $SIZE"
            echo "Segment $i: File size $SIZE" >> "$MAIN_DIR/performance_test.txt"
        fi
    fi
    
    # OPTIMIERT: Minimale Pause zwischen Segmenten (2 Sekunden)
    if [ $i -lt 3 ]; then
        echo "Quick transition: waiting 2 seconds before next segment..."
        sleep 2
    fi
    
    echo ""
done

# Performance Summary
end_test=$(date +%s)
echo "=== PERFORMANCE TEST COMPLETE ==="
echo "Total segments recorded: $i"
echo "Test directory: $MAIN_DIR"

# Finalize performance info
cat >> "$MAIN_DIR/performance_test.txt" << EOF

=== PERFORMANCE SUMMARY ===
Completion time: $(date)
Total segments: $i/3
Test directory: $MAIN_DIR

=== OPTIMIZATION RESULTS ===
Expected improvements:
- Stop times: ~15-25s (vs. previous 75s)
- Inter-segment pause: 2s (vs. previous 15s)
- Total overhead reduction: ~50-60s saved

=== FILE STRUCTURE ===
EOF

# Liste alle Segmente in der Performance-Info auf
ls -la "$MAIN_DIR"/ >> "$MAIN_DIR/performance_test.txt"

echo ""
echo "Performance test info saved to: $MAIN_DIR/performance_test.txt"
echo "All segments stored in: $MAIN_DIR/"

# Zeige Gesamtgröße und Performance-Metriken
TOTAL_SIZE=$(du -sh "$MAIN_DIR" | cut -f1)
echo "Total test size: $TOTAL_SIZE"

echo ""
echo "🚀 PERFORMANCE TEST COMPLETE! 🚀"
echo "Check the performance_test.txt file for detailed timing analysis"