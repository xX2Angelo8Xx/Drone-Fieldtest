# Gap Reduction Analysis: Achieving <10s Segmentation Gaps

## Current Status (October 27, 2025)
- **Best achieved gap**: 55-60 seconds with overlapped recording approach
- **Root cause**: ZED SDK `disableRecording()` takes 50+ seconds inherently
- **Target**: <10 seconds for production drone missions

## Tested Approaches

### 1. Overlapped Recording (IMPLEMENTED ✅)
**Method**: Start new recording BEFORE stopping old one
**Result**: 55-60 second gaps (11-second improvement over baseline)
**Limitation**: `disableRecording()` still blocks for 50+ seconds

```cpp
// Key insight: ZED SDK supports simultaneous recordings briefly
[ZED] PERFECT OVERLAP: Both recordings active simultaneously!
[ZED] OVERLAPPED SWITCH SUCCESS: 56893ms total transition!
```

### 2. Dual Camera Instance (TESTED ❌)
**Method**: Use two ZED camera instances for instant switching
**Result**: Failed - "CAMERA STREAM FAILED TO START"
**Limitation**: Hardware/driver doesn't support multiple camera instances

### 3. Memory Buffer Approach (THEORETICAL 🔄)
**Method**: Buffer frames in memory during slow switch, replay to new recording
**Challenges**: 
- Complex SVO file manipulation required
- Memory usage for 10+ seconds of HD720 frames (~300MB)
- No direct ZED SDK support for frame injection

## Alternative Strategies for <10s Gaps

### 4. ZED SDK Parameter Optimization (UNEXPLORED)
**Potential optimizations**:
```cpp
// Reduce buffer sizes for faster shutdown
recording_params.target_compression_bitrate = 0; // Minimal compression processing
recording_params.transcode_streaming_input = false; // Disable transcoding

// Force immediate flush
zed_.pauseRecording(true);  // This is fast
// Replace disableRecording() with custom rapid shutdown?
```

### 5. Stream Capture Approach (REVOLUTIONARY)
**Method**: Bypass ZED recording, use direct camera stream capture
```cpp
// Instead of zed_.enableRecording(), use:
while(recording) {
    if (zed_.grab() == SUCCESS) {
        zed_.retrieveImage(frame, VIEW::LEFT);
        // Write frame directly to custom video file
        // No ZED SDK recording buffer = No 50s shutdown delay
    }
}
```

**Benefits**:
- Complete control over file switching (could be <1 second)
- No ZED SDK recording buffer limitations
- Custom compression and file format control

**Challenges**:
- Need to implement video encoding (H.264/H.265)
- Handle frame timing and synchronization
- Sensor data correlation more complex

### 6. Filesystem-Level Optimization (QUICK WIN)
**Method**: Use faster file operations and storage optimizations
```cpp
// Use O_DIRECT for bypassing filesystem cache
// Pre-allocate file space to avoid allocation delays
// Use dedicated SSD partition for recording files
```

### 7. External Recording Pipeline (ADVANCED)
**Method**: Stream camera data to external recording process
- ZED camera streams to named pipe/shared memory
- Separate process handles file recording with instant switching
- Main process never blocks on file operations

## Recommended Next Steps

### Immediate (<1 hour):
1. **Test ZED SDK parameter optimization** - try different recording parameters to speed up `disableRecording()`
2. **Filesystem optimization** - pre-allocate files, use faster I/O flags

### Short-term (<1 day):
3. **Stream capture proof-of-concept** - implement direct frame capture without ZED recording system
4. **Benchmark different compression modes** - see if LOSSLESS is causing the delay

### Long-term (<1 week):
5. **External pipeline implementation** - if stream capture works, build robust production system
6. **Custom video encoding** - implement H.264 encoding for optimal file sizes and switching

## Technical Deep Dive: Why 50+ Seconds?

The ZED SDK `disableRecording()` likely performs:
1. **Buffer flush** (largest component - 30-40s)
2. **File finalization** (SVO2 format metadata writing - 10-15s)  
3. **Compression finalization** (LOSSLESS processing - 5-10s)
4. **Resource cleanup** (camera buffers, memory - 1-2s)

**Key insight**: If we can bypass steps 1-3 by using stream capture, we could achieve <5s gaps.

## Conclusion

**For <10s gaps**, we need to move away from ZED SDK's built-in recording system. The **stream capture approach** offers the most promise for achieving true <10s segmentation while maintaining file integrity and corruption prevention.

The current overlapped approach (55s gaps) is production-ready for most applications, but achieving <10s requires a more fundamental architectural change.