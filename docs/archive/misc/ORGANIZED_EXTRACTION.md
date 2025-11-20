# SVO Image Extraction - Organized Output Structure

## ✅ IMPLEMENTED: Organized Folder Structure

The SVO extractor now automatically creates organized folders based on the source SVO filename and flight information.

## 📁 Output Location
```
/home/angelo/Projects/Drone-Fieldtest/extracted_images/
├── flight_20251019_014658_video/     (7 images, every 100th frame)
│   ├── frame_000000.jpg
│   ├── frame_000001.jpg
│   └── ...
└── flight_20251027_132504_video/     (37 images, every 50th frame)
    ├── frame_000000.jpg
    ├── frame_000001.jpg
    └── ...
```

## 🔧 Usage (Updated)

### Simple Extraction
```bash
# Extract every 10th frame (default)
./extract_svo_images.sh /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2

# Extract every 50th frame  
./extract_svo_images.sh /media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2 50
```

### Direct Tool Usage
```bash
# Extract every 30th frame
./build/tools/svo_extractor /path/to/video.svo2 30
```

## 📋 Folder Naming Convention
- **Format**: `flight_YYYYMMDD_HHMMSS_video/`
- **Source**: Automatically extracted from SVO file path
- **Benefits**: 
  - Easy identification of source flight
  - No folder name conflicts
  - Organized by date/time
  - Batch processing friendly

## 📊 Current Extractions
| Flight | Date | Images | Frame Skip | Size |
|--------|------|---------|------------|------|
| flight_20251019_014658 | 2025-10-19 | 7 images | Every 100th | ~1.6MB |
| flight_20251027_132504 | 2025-10-27 | 37 images | Every 50th | ~9.3MB |

## 🎯 AI Training Workflow
1. **Record flights** using gap-free profiles (HD720@15fps)
2. **Extract images** using organized structure
3. **Label/annotate** images in each flight folder
4. **Train models** using organized dataset
5. **Track progress** by flight session

## 🔍 Finding Your Images
All extracted images are organized in:
```bash
/home/angelo/Projects/Drone-Fieldtest/extracted_images/
```

Each flight gets its own folder with the pattern:
```
flight_[DATE]_[TIME]_video/
```

This makes it easy to:
- Process multiple flights
- Keep training data organized
- Track extraction progress
- Prepare datasets for AI training