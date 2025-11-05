# NVENC Hardware-Encoder Investigation Results

## Executive Summary

After extensive investigation, **NVENC hardware encoding is NOT accessible** through the ZED SDK on the Jetson Orin Nano system. The solution for longer recordings is to use **segmented recording** rather than attempting hardware acceleration.

## Investigation Results

### Hardware Encoder Availability
- **System**: Jetson Orin Nano (15W mode)
- **NVIDIA Driver**: 540.4.0
- **CUDA Version**: 12.6
- **Hardware Encoder**: Present but not accessible via ZED SDK

### Attempted Solutions

#### 1. H264 Hardware Encoding (FAILED)
```cpp
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::H264;
rec_params.bitrate = 2000;  // 2Mbps
```
**Error**: `[ZED][Encoding] Critical Error : No Video Enc [enc_0]`

#### 2. H265 Software Encoding (FAILED)
```cpp
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::H265;
rec_params.bitrate = 12000;  // 12Mbps
```
**Error**: `[ZED][Encoding] Critical Error : No Video Enc [enc_0]`

#### 3. MONO Recording Attempt (FAILED)
- Tried single-eye recording to reduce data
- Same encoding errors occurred

### Working Solution: LOSSLESS + Segmented Recording

#### Current Configuration
```cpp
rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::LOSSLESS;
rec_params.target_framerate = (current_mode_ == RecordingMode::HD720_30FPS) ? 30 : 15;
```

#### Available Profiles

| Profile | Resolution/FPS | Duration | Use Case |
|---------|---------------|----------|----------|
| `realtime_light` | HD720@15fps | 30s | Maximum reliability |
| `realtime_30fps` | HD720@30fps | 30s | Field optimized |
| `long_mission` | HD720@30fps | 2min | LOSSLESS safe |
| `extended_mission` | HD720@15fps | 3min | Memory optimized |
| `endurance_mission` | HD720@15fps | 4min | Maximum single recording |

### Segmented Recording for Longer Missions

For recordings longer than 4 minutes, use the segmented recording script:

```bash
./scripts/segmented_recording.sh long_mission 10  # 10x 2-minute segments = 20 minutes
```

**Benefits:**
- No corruption issues
- Fast stop times (15-20 seconds per segment)
- Automatic organization
- Master info files for easy management

## Technical Findings

### ZED SDK Limitations
1. **Hardware Encoder Access**: ZED SDK cannot access NVENC on Jetson systems
2. **32-bit File Pointers**: 4GB file size limit causes corruption
3. **Memory Usage**: LOSSLESS mode requires ~3GB RAM for 4-minute recordings

### Performance Metrics
- **LOSSLESS HD720@30fps**: ~13 MB/s write rate
- **Maximum Safe Duration**: 4 minutes single recording
- **Stop Time**: 15-20 seconds (much better than previous 60+ seconds)

## Recommendations

### For Flight Operations

1. **Short Missions (< 4 minutes)**:
   - Use `endurance_mission` profile
   - Single file, fast processing

2. **Long Missions (> 4 minutes)**:
   - Use segmented recording script
   - Multiple 2-minute segments
   - No corruption risk

3. **Critical Operations**:
   - Use `realtime_light` (30s, HD720@15fps)
   - Maximum reliability, lowest resource usage

### Future Improvements

1. **Hardware Encoder Access**: Investigate direct NVENC integration bypassing ZED SDK
2. **Custom Pipeline**: Develop custom recording pipeline with hardware acceleration
3. **Codec Support**: Test other compression modes if ZED SDK adds support

## Conclusion

While NVENC hardware encoding is not available through the ZED SDK, the current **LOSSLESS + segmented recording** solution provides:

- ✅ **Reliable recording** up to 4 minutes per segment
- ✅ **Fast stop times** (15-20 seconds vs. 60+ seconds)
- ✅ **No corruption** issues
- ✅ **Automatic organization** of long recordings
- ✅ **Production-ready** for drone field operations

The system is now optimized for your flight operations with minimal interruption times.