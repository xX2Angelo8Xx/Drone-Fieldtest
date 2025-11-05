#!/bin/bash

# DRONE LARGE FILE RECORDING SOLUTION
# ===================================
# Since LOSSLESS compression creates ~40MB/second files,
# we need to segment recordings to avoid >4GB corruption

SMART_RECORDER="/home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder/smart_recorder"
SEGMENT_DURATION=90  # 90 seconds = ~3.6GB per segment (safe margin)
TOTAL_SEGMENTS=5     # Total 7.5 minutes of recording
SEGMENT_PROFILE="long_mission"  # 120s profile, but we'll stop at 90s

echo "🎬 SEGMENTED LARGE FILE RECORDING"
echo "================================="
echo "Segment Duration: ${SEGMENT_DURATION}s"
echo "Total Segments: ${TOTAL_SEGMENTS}"
echo "Total Recording Time: $((SEGMENT_DURATION * TOTAL_SEGMENTS))s"
echo

for i in $(seq 1 $TOTAL_SEGMENTS); do
    echo "📹 Starting Segment $i/$TOTAL_SEGMENTS..."
    
    # Start recording in background
    timeout ${SEGMENT_DURATION}s sudo "$SMART_RECORDER" "$SEGMENT_PROFILE" &
    RECORDER_PID=$!
    
    # Wait for segment to complete
    wait $RECORDER_PID
    RESULT=$?
    
    if [ $RESULT -eq 0 ] || [ $RESULT -eq 124 ]; then  # 124 = timeout (expected)
        echo "✅ Segment $i completed successfully"
        
        # Brief pause between segments
        if [ $i -lt $TOTAL_SEGMENTS ]; then
            echo "⏸️  Pause before next segment..."
            sleep 5
        fi
    else
        echo "❌ Segment $i failed with code $RESULT"
        echo "Stopping segmented recording."
        exit 1
    fi
done

echo "🎉 All segments completed successfully!"
echo "Check recordings: ls -lh /media/angelo/DRONE_DATA/flight_*/"