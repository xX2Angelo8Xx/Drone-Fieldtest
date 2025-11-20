#!/bin/bash

# SVO Image Extraction Script - Organized Output
# Usage: ./extract_svo_images.sh <input_svo_file> [frame_skip]

if [ $# -lt 1 ]; then
    echo "Usage: $0 <input_svo_file> [frame_skip]"
    echo "Example: $0 /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2 5"
    echo "         $0 /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2    (uses default frame skip of 10)"
    echo ""
    echo "Output: Creates organized folders in /home/angelo/Projects/Drone-Fieldtest/extracted_images/"
    echo "        Format: extracted_images/flight_YYYYMMDD_HHMMSS_video/"
    exit 1
fi

INPUT_SVO="$1"
FRAME_SKIP="${2:-10}"

# Check if input file exists
if [ ! -f "$INPUT_SVO" ]; then
    echo "Error: SVO file '$INPUT_SVO' not found!"
    exit 1
fi

echo "Extracting images from: $INPUT_SVO"
echo "Frame skip: $FRAME_SKIP"
echo "Organized output in: /home/angelo/Projects/Drone-Fieldtest/extracted_images/"
echo "----------------------------------------"

# Run the SVO extractor with new organized structure
/home/angelo/Projects/Drone-Fieldtest/build/tools/svo_extractor "$INPUT_SVO" "$FRAME_SKIP"

if [ $? -eq 0 ]; then
    echo "----------------------------------------"
    echo "Extraction completed successfully!"
    echo "Check organized folders in: /home/angelo/Projects/Drone-Fieldtest/extracted_images/"
    
    # Find the most recently created directory for image count
    LATEST_DIR=$(find /home/angelo/Projects/Drone-Fieldtest/extracted_images/ -type d -name "*" -not -path "*/.*" | sort | tail -1)
    if [ -n "$LATEST_DIR" ]; then
        echo "Latest extraction folder: $(basename "$LATEST_DIR")"
        echo "Total images extracted: $(ls -1 "$LATEST_DIR"/*.jpg 2>/dev/null | wc -l)"
    fi
else
    echo "Error: Extraction failed!"
    exit 1
fi