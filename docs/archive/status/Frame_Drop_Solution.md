# Frame Drop Resolution - Drone Recording System

## Problem Analysis
Originally, the ZED recordings were experiencing frame drops when viewed in ZED Explorer, particularly at higher frame rates (30fps and above) with LOSSLESS compression.

## Root Cause
The frame drops were caused by:
1. **Compression bottleneck**: LOSSLESS compression at 30fps was too demanding for real-time recording
2. **High data throughput**: HD720@30fps with LOSSLESS compression generated excessive data rates
3. **Hardware limitations**: Even with Jetson Orin Nano in 15W mode, the compression overhead was too high

## Solution Implemented

### 1. Frame Rate Optimization
- **Changed**: `realtime_light` profile from HD720_30FPS → **HD720_15FPS**
- **Benefit**: Reduces data throughput by 50% while maintaining HD resolution
- **Impact**: Eliminates compression bottleneck while preserving video quality

### 2. Profile Updates
```cpp
{"realtime_light", {
    RecordingMode::HD720_15FPS,  // Reduced from 30FPS
    30,
    "Real-time Light AI Model (No Frame Drops)", 
    "Für leichte AI-Modelle in Echtzeit - optimiert gegen Frame-Drops"
}},
```

### 3. Performance Optimizations
```cpp
// In zed_recorder.cpp init()
init_params.depth_mode = sl::DEPTH_MODE::NONE;          // Depth disabled
init_params.depth_stabilization = false;                // Performance boost
init_params.enable_image_enhancement = false;           // Enhancement disabled
```

## Test Results

### Before Optimization
- **Mode**: HD720_30FPS with LOSSLESS compression
- **Issue**: Frame drops visible in ZED Explorer
- **Status**: Unusable for AI training data

### After Optimization  
- **Mode**: HD720_15FPS with LOSSLESS compression
- **Recording Duration**: 30 seconds successful
- **File Size**: 505MB (normal for HD720@15fps)
- **Status**: ✅ **No frame drops, smooth recording**

## Performance Data
```
Recording Profile: realtime_light
- Resolution: HD720 (1280x720)
- Frame Rate: 15 FPS
- Duration: 30 seconds
- File Size: 505MB
- Compression: LOSSLESS
- Result: SUCCESS - No frame drops
```

## Alternative Profiles Available
For different use cases:
- **VGA_100FPS**: Ultra-high speed, minimal resolution
- **HD1080_30FPS**: Higher resolution for training data
- **HD2K_15FPS**: Maximum resolution for special applications

## Recommendations
1. **Field Recording**: Use `realtime_light` profile (HD720_15FPS)
2. **AI Training**: Use `training` profile (HD1080_30FPS) for better data quality
3. **Development**: Use `development` profile for quick tests

## Technical Notes
- LOSSLESS compression retained for maximum data quality
- 15fps still provides sufficient temporal resolution for most AI training
- Hardware encoding (H264/H265) was tested but caused encoding errors on this Jetson setup
- Current solution balances quality, performance, and reliability

## Verification
Run ZED Explorer on recorded files to confirm smooth playback without frame drops:
```bash
ZED_Explorer /media/angelo/DRONE_DATA/flight_*/video.svo2
```

**Status**: ✅ Frame drop issue resolved
**Date**: October 18, 2024
**System**: Jetson Orin Nano, ZED 2i, 15W Performance Mode