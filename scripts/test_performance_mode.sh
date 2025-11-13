#!/bin/bash
# Quick test of Performance Test Mode (SVO2 + Depth Computation)

echo "🧪 Performance Test Mode - Quick Test"
echo "======================================"
echo ""
echo "This test records in SVO2 format at 30 FPS while computing depth maps"
echo "in the background (depth maps are NOT saved, only performance is measured)"
echo ""

# Test configuration
TEST_DURATION=60  # 60 seconds test
OUTPUT_DIR="/media/angelo/DRONE_DATA1"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="${OUTPUT_DIR}/perf_test_${TIMESTAMP}.svo2"

echo "Test Configuration:"
echo "  Duration: ${TEST_DURATION} seconds"
echo "  Output: ${OUTPUT_FILE}"
echo "  Depth Mode: NEURAL_LITE (recommended for testing)"
echo ""

# Check if USB is mounted
if [ ! -d "${OUTPUT_DIR}" ]; then
    echo "❌ ERROR: USB drive not found at ${OUTPUT_DIR}"
    echo "   Please ensure DRONE_DATA1 is mounted"
    exit 1
fi

echo "✅ USB drive found"
echo ""
echo "Starting performance test..."
echo "Watch for '[ZED PERF]' lines showing depth computation FPS"
echo ""
echo "Expected results:"
echo "  - Recording FPS: 30 FPS (SVO2)"
echo "  - Depth computation: 15-20 FPS (NEURAL_LITE)"
echo ""
echo "Press Ctrl+C to stop early"
echo ""

# Note: This would require a CLI tool to test the performance mode
# For now, use the web interface:
echo "⚠️  This feature requires integration into drone_web_controller"
echo ""
echo "To test manually:"
echo "1. Run: sudo ./build/apps/drone_web_controller/drone_web_controller"
echo "2. Connect to WiFi: DroneController (password: drone123)"
echo "3. Open: http://192.168.4.1:8080"
echo "4. Enable Performance Test mode in GUI (future feature)"
echo ""
echo "Alternatively, integrate into C++ app with:"
echo ""
echo "  recorder.enableDepthComputation(true, sl::DEPTH_MODE::NEURAL_LITE);"
echo "  recorder.startRecording(\"${OUTPUT_FILE}\", \"sensor.csv\");"
echo ""
echo "Monitor console for depth FPS output"
