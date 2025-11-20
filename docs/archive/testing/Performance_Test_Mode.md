> Note (archived): Superseded by `docs/guides/testing/TESTING_AND_PERFORMANCE_GUIDE.md`. This file remains for historical details.

# Performance Test Mode - Depth Computation

## Problem Analysis

RAW recording mode achieves only **~7 FPS** due to multiple bottlenecks:
- **Depth computation** (most expensive operation)
- **2x JPEG encoding** (left + right images)
- **Disk I/O** (writing thousands of individual files)
- **USB throughput** limitations

## Solution: SVO2 + Depth Computation Test Mode

### Concept
Record in **SVO2 format** (30 FPS, compressed, single file) while **simultaneously computing depth maps** but **NOT saving them**. This allows:

✅ **Full 30 FPS recording** (SVO2 has minimal overhead)  
✅ **Real depth computation performance test** (tests Jetson GPU/CPU load)  
✅ **No disk I/O bottleneck** (only SVO2 file written)  
✅ **Separate depth FPS metric** (shows achievable depth computation rate)

### Key Insight
SVO2 recording does **NOT** compute depth during capture - depth is calculated later during playback/extraction. This mode adds depth computation to test real-world AI/computer vision performance while maintaining high-quality video recording.

## Implementation

### API Usage

```cpp
// Enable depth computation during SVO2 recording
recorder.enableDepthComputation(true, sl::DEPTH_MODE::NEURAL);

// Start normal SVO2 recording
recorder.startRecording("flight.svo2", "sensor.csv");

// Monitor depth computation performance
float depth_fps = recorder.getDepthComputationFPS();
std::cout << "Depth computation: " << depth_fps << " FPS" << std::endl;

// Disable for normal recording
recorder.enableDepthComputation(false);
```

### Depth Modes Performance (Expected)

| Mode | Expected FPS | Quality | Use Case |
|------|--------------|---------|----------|
| `NEURAL_PLUS` | 5-8 FPS | Highest | Maximum accuracy |
| `NEURAL` | 7-12 FPS | High | Balanced AI mode |
| `NEURAL_LITE` | 15-20 FPS | Good | **Recommended for testing** |
| `ULTRA` | 20-25 FPS | Moderate | Traditional stereo |
| `QUALITY` | 25-28 FPS | Moderate | Fast traditional |
| `PERFORMANCE` | 28-30 FPS | Lower | Real-time priority |
| `NONE` | 30 FPS | N/A | No depth computation |

### Logging Output

```
[ZED] Depth computation enabled (mode: NEURAL_LITE) - Testing Jetson performance without saving depth
[ZED PERF] Depth computation: 18.5 FPS (took 54ms)
[ZED PERF] Depth computation: 19.2 FPS (took 52ms)
```

## Benefits Over RAW Mode

| Feature | RAW Mode | Performance Test Mode |
|---------|----------|----------------------|
| Recording FPS | ~7 FPS | **30 FPS** |
| Depth computation | Yes (saved) | Yes (discarded) |
| Video quality | JPEG 90% | **Lossless SVO2** |
| Disk usage | 3 files/frame | **1 file total** |
| File size | Large (fragmented) | **Optimal (single)** |
| Performance test | ✅ | ✅ |
| Training data | ✅ | ❌ (use RAW for this) |

## When to Use Each Mode

### Use **Performance Test Mode** (SVO2 + Depth) when:
- Testing Jetson Orin Nano depth computation capabilities
- Benchmarking different depth modes
- Recording mission footage while monitoring AI performance
- Preparing for object detection integration

### Use **RAW Mode** when:
- Need individual frames for training data
- Require saved depth maps for later analysis
- Custom post-processing of images
- Frame-by-frame analysis required

### Use **Standard SVO2** when:
- Maximum recording quality needed
- No real-time depth computation required
- Post-flight depth extraction is acceptable
- Minimal system load during flight

## Technical Notes

- Depth computation happens in `recordingLoop()` via `retrieveMeasure(DEPTH)`
- FPS calculated as rolling average (90% previous + 10% current)
- Depth map buffer reused to minimize memory allocation
- Logged every ~3 seconds (90 frames at 30fps)
- No performance impact when disabled (`compute_depth_ = false`)

## Future Integration

This mode prepares the codebase for:
- Real-time object detection during recording
- Obstacle avoidance systems
- AI-based flight assistance
- Live depth streaming to GUI

---

**Recommendation**: Start testing with `NEURAL_LITE` mode as baseline, then experiment with `NEURAL` and `NEURAL_PLUS` if performance allows.
