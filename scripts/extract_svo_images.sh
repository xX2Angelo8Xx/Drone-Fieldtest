#!/bin/bash

# SVO TO IMAGES EXTRACTOR SCRIPT
# ==============================
# Extract left camera images from SVO files for AI training

echo "🖼️  SVO IMAGE EXTRACTION TOOL"
echo "=============================="

# Check if SVO file path provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <svo_file_path> [output_directory] [skip_frames]"
    echo
    echo "Examples:"
    echo "  $0 /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2"
    echo "  $0 /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2 /home/angelo/training_data/"
    echo "  $0 /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2 /home/angelo/training_data/ 5"
    echo
    echo "Available SVO files:"
    find /media/angelo/DRONE_DATA -name "*.svo*" | head -5
    exit 1
fi

SVO_FILE="$1"
OUTPUT_DIR="${2:-/home/angelo/training_images/$(basename "$SVO_FILE" .svo2)}"
SKIP_FRAMES="${3:-1}"  # Extract every Nth frame (1 = all frames)

# Validate SVO file exists
if [ ! -f "$SVO_FILE" ]; then
    echo "❌ SVO file not found: $SVO_FILE"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "📁 Input SVO: $SVO_FILE"
echo "📁 Output Dir: $OUTPUT_DIR"
echo "⏭️  Skip Frames: $SKIP_FRAMES (extract every ${SKIP_FRAMES}th frame)"
echo

# Get SVO file info
echo "📊 SVO File Information:"
ls -lh "$SVO_FILE"
echo

# METHOD 1: Use ZED_Explorer to export (if available)
if command -v /usr/local/zed/tools/ZED_SVO_Export >/dev/null 2>&1; then
    echo "🔧 Using ZED_SVO_Export tool..."
    
    # Export left images
    /usr/local/zed/tools/ZED_SVO_Export "$SVO_FILE" "$OUTPUT_DIR" LEFT_IMAGE
    
    echo "✅ Export completed using ZED tools"
    
elif command -v ffmpeg >/dev/null 2>&1; then
    echo "🔧 Using FFmpeg for extraction..."
    
    # Try to extract using ffmpeg (may work for some SVO formats)
    ffmpeg -i "$SVO_FILE" -vf "select=not(mod(n\\,$SKIP_FRAMES))" "$OUTPUT_DIR/frame_%06d.jpg"
    
    echo "✅ Export completed using FFmpeg"
    
else
    echo "⚠️  Neither ZED tools nor FFmpeg available"
    echo "💡 Manual extraction options:"
    echo
    echo "1. Use ZED Explorer GUI:"
    echo "   - Open: /usr/local/zed/tools/ZED_Explorer $SVO_FILE"
    echo "   - Go to Export menu"
    echo "   - Select 'Left Images'"
    echo
    echo "2. Build custom extractor:"
    echo "   - Use the C++ code in svo_image_extractor.cpp"
    echo "   - Compile with ZED SDK and OpenCV"
fi

# Check results
if [ -d "$OUTPUT_DIR" ] && [ "$(ls -A "$OUTPUT_DIR" 2>/dev/null)" ]; then
    FRAME_COUNT=$(ls "$OUTPUT_DIR"/*.jpg 2>/dev/null | wc -l || echo "0")
    echo
    echo "📈 EXTRACTION RESULTS:"
    echo "   Frames extracted: $FRAME_COUNT"
    echo "   Output directory: $OUTPUT_DIR"
    echo "   Sample files:"
    ls -la "$OUTPUT_DIR" | head -5
    
    if [ "$FRAME_COUNT" -gt 0 ]; then
        echo "✅ Success! Images ready for training data preparation"
    fi
else
    echo "❌ No images extracted. Check SVO file format compatibility."
fi