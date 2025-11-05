# SVO Image Extraction Tool - Documentation

## Overview
Successfully created a tool to extract left camera images from ZED SVO recordings for AI training data preparation.

## Components Created
1. **svo_extractor.cpp** - C++ tool using ZED SDK and OpenCV
2. **tools/CMakeLists.txt** - Build configuration
3. **extract_svo_images.sh** - Wrapper script for easy usage

## Build Status
✅ **SUCCESSFULLY COMPILED** after resolving OpenCV dependency issues
- Used Ubuntu OpenCV 4.5.4d libraries directly
- Linked against versioned .so files to avoid broken symlinks

## Testing Results
✅ **EXTRACTION WORKING** - Tested with gap-free recording from flight_20251027_132504
- Total frames in recording: 1822 frames
- Extracted 19 images with frame skip of 100
- Output format: 1280x720 JPEG images (~250KB each)
- Processing time: ~45 seconds for 2-minute recording

## Usage Examples

### Direct Tool Usage
```bash
./build/tools/svo_extractor <input.svo2> <output_dir/> <frame_skip>
```

### Using Wrapper Script  
```bash
# Extract every 10th frame to default directory
./extract_svo_images.sh /path/to/video.svo2

# Extract every 30th frame to custom directory
./extract_svo_images.sh /path/to/video.svo2 ./training_images 30

# High-density extraction (every 5th frame)
./extract_svo_images.sh /path/to/video.svo2 ./dense_images 5
```

## Performance Characteristics
- **Frame Rate**: Processes ~40 frames/second
- **Extraction Ratio**: 1:frame_skip (configurable)
- **Output Quality**: Original ZED left camera quality preserved
- **File Size**: ~250KB per 1280x720 JPEG image

## Integration with Existing System
- Compatible with all existing SVO recordings
- Works with gap-free recordings from proven HD720@15fps profile
- Can process recordings from flight directories: `/media/angelo/DRONE_DATA/flight_*/video.svo2`

## Next Steps for AI Training Pipeline
1. ✅ Extract images from training flights
2. ⏳ Label/annotate extracted images for object detection
3. ⏳ Train AI models using extracted training data
4. ⏳ Deploy trained models back to drone system

## Technical Notes
- Extracts only left camera images (stereo recordings contain both left/right)
- BGRA to BGR conversion for OpenCV compatibility
- Memory efficient - processes frame by frame
- Handles end-of-file gracefully
- Preserves original image quality and resolution