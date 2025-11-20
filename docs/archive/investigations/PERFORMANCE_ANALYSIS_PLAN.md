# Performance Analysis Plan - 40 FPS Bottleneck Investigation
**Date:** November 17, 2025  
**Target:** Understand why 60 FPS setting achieves only ~40 FPS actual  
**Goal:** Identify optimization opportunities or confirm hardware limitations

---

## 🎯 Problem Statement

**Observed Behavior:**
- Camera setting: HD720 @ 60 FPS
- Expected FPS: 60 (or close to it)
- Actual FPS: 35-40 FPS in SVO2-only mode
- Gap: ~20-25 FPS missing (33-42% performance loss)

**Impact:**
- Medium severity - system is functional but underperforming
- May limit use cases requiring high frame rates
- Affects data quality for high-speed drone maneuvers

**Current Workaround:**
- Use 30 FPS setting for stable 28-30 FPS actual
- Works well but limits high-speed capture capability

---

## 🔬 Investigation Methodology

### Phase 1: Baseline Measurements (1 hour)

#### 1.1 ZED Explorer Analysis
**Tool:** `/usr/local/zed/tools/ZED_Explorer`  
**Method:**
1. Record 4-minute test: HD720 @ 60 FPS, SVO2 mode
2. Open `video.svo2` in ZED Explorer
3. Navigate to Info panel
4. Record:
   - Target FPS (should be 60)
   - Actual FPS (expected ~40)
   - Frame count
   - Duration
   - Compression ratio

**Expected Output:**
```
Target FPS: 60
Actual FPS: 38.5
Frames: 9240 (should be 14400 for true 60 FPS)
Duration: 240s
Missing: 5160 frames (35.8% loss)
```

#### 1.2 System Resource Monitoring
**Tools:** `htop`, `tegrastats`, `iostat`  
**Method:**
1. Start recording in terminal 1
2. In terminal 2, run:
   ```bash
   # CPU monitoring
   htop > cpu_log.txt
   
   # GPU/thermal monitoring
   sudo tegrastats --interval 1000 > gpu_log.txt
   
   # I/O monitoring
   iostat -x 1 > io_log.txt
   ```
3. Let run for full 4-minute recording
4. Analyze logs for bottlenecks

**Look For:**
- CPU usage per core (is any core at 100%?)
- GPU utilization (should be high during depth computation)
- USB write speed (MB/s to storage)
- Memory usage (swap activity = problem)
- Temperature throttling (>80°C = thermal limits)

#### 1.3 Console Log Analysis
**Method:**
1. Record with verbose logging:
   ```bash
   sudo ./build/apps/drone_web_controller/drone_web_controller 2>&1 | tee recording_log.txt
   ```
2. Search for patterns:
   - Frame drop messages
   - "WARNING" or "ERROR" entries
   - Timing information
   - Buffer full messages

**Look For:**
```
[ZED_RECORDER] Frame capture took: Xms
[ZED_RECORDER] Write took: Yms
[STORAGE] Buffer full, waiting...
```

---

### Phase 2: Isolation Testing (2 hours)

#### 2.1 Storage Speed Test
**Hypothesis:** USB write speed limiting FPS  
**Method:**
```bash
# Test USB write speed
sudo hdparm -tT /dev/sda1

# Test sustained write
dd if=/dev/zero of=/media/angelo/DRONE_DATA/testfile bs=1M count=8000 oflag=direct
# Calculate MB/s, compare to recording requirement

# Required bandwidth for 60 FPS:
# HD720 (1280x720) × 2 bytes × 60 FPS × 2 cameras × compression(~0.4) = ~88 MB/s
```

**Expected Result:**
- If USB speed < 100 MB/s → Storage is bottleneck
- If USB speed > 200 MB/s → Storage is NOT bottleneck

#### 2.2 Camera-Only Test (No Recording)
**Hypothesis:** Frame capture itself has overhead  
**Method:**
1. Modify code to grab() frames but not record
2. Count actual frames captured in 60 seconds
3. Calculate real capture rate

**Code Modification:**
```cpp
// In zed_recorder.cpp startRecording()
int frame_count = 0;
auto start = std::chrono::steady_clock::now();

while (g_running && frame_count < 3600) {  // 60 seconds × 60 FPS
    if (zed_->grab() == sl::ERROR_CODE::SUCCESS) {
        frame_count++;
        // DON'T record, just count
    }
}

auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
std::cout << "Captured " << frame_count << " frames in " << duration << "s" << std::endl;
std::cout << "Actual FPS: " << (frame_count / duration) << std::endl;
```

**Expected Result:**
- If actual FPS ≈ 60 → Recording pipeline is bottleneck
- If actual FPS ≈ 40 → Camera/grab() is bottleneck

#### 2.3 Minimal Recording Test
**Hypothesis:** SVO2 compression overhead  
**Method:**
1. Record RAW frames (uncompressed) at 60 FPS
2. Compare actual FPS to SVO2 mode
3. If RAW achieves 60 FPS → compression is bottleneck

**Test Command:**
```bash
# In Web UI:
# 1. Select RAW mode
# 2. Set HD720 @ 60 FPS
# 3. Record for 1 minute
# 4. Check frame count in output directory
```

**Expected Result:**
- If RAW achieves 60 FPS → SVO2 compression CPU-limited
- If RAW also achieves 40 FPS → Bottleneck is earlier in pipeline

#### 2.4 LCD Interference Test
**Hypothesis:** LCD updates blocking main thread  
**Method:**
1. Comment out all LCD updates in recordingMonitorLoop()
2. Record 4 minutes
3. Compare FPS to baseline

**Code Modification:**
```cpp
// In recordingMonitorLoop()
// COMMENT OUT:
// updateLCD(line1.str(), line2.str());
```

**Expected Result:**
- If FPS increases → LCD is interfering
- If FPS unchanged → LCD is not the issue

#### 2.5 GPU Utilization Test
**Hypothesis:** GPU memory bandwidth or compute limiting  
**Method:**
```bash
# Monitor GPU during recording
sudo tegrastats --interval 100

# Look for:
# GR3D_FREQ: GPU frequency (should be at max)
# EMC_FREQ: Memory controller frequency
# RAM: Memory usage
```

**Expected Result:**
- If GPU < 90% utilization → GPU has headroom
- If GPU ≈ 100% utilization → GPU is bottleneck

---

### Phase 3: ZED SDK Profiling (2 hours)

#### 3.1 grab() Timing Analysis
**Method:**
1. Add high-resolution timing around grab() call
2. Log timing for 1000 frames
3. Calculate average, min, max, stddev

**Code Modification:**
```cpp
// In zed_recorder.cpp
#include <chrono>

auto grab_start = std::chrono::high_resolution_clock::now();
sl::ERROR_CODE err = zed_->grab();
auto grab_end = std::chrono::high_resolution_clock::now();

auto grab_us = std::chrono::duration_cast<std::chrono::microseconds>(grab_end - grab_start).count();
if (frame_count % 100 == 0) {
    std::cout << "[ZED_RECORDER] grab() took: " << grab_us << " μs" << std::endl;
}
```

**Expected at 60 FPS:**
- Frame budget: 16.67ms (16,667 μs)
- grab() should take < 10ms ideally
- If grab() > 15ms → That's the bottleneck

#### 3.2 record() Timing Analysis
**Method:**
1. Add timing around recording.record() call
2. Log timing for 1000 frames

**Code Modification:**
```cpp
auto record_start = std::chrono::high_resolution_clock::now();
recording_->record();
auto record_end = std::chrono::high_resolution_clock::now();

auto record_us = std::chrono::duration_cast<std::chrono::microseconds>(record_end - record_start).count();
if (frame_count % 100 == 0) {
    std::cout << "[ZED_RECORDER] record() took: " << record_us << " μs" << std::endl;
}
```

#### 3.3 ZED Camera Parameters Review
**Method:**
Check if camera parameters are optimal for 60 FPS

**Review in code:**
```cpp
sl::InitParameters init_params;
init_params.camera_resolution = sl::RESOLUTION::HD720;
init_params.camera_fps = 60;
init_params.depth_mode = sl::DEPTH_MODE::NONE;  // For SVO2-only test
init_params.coordinate_units = sl::UNIT::METER;
init_params.coordinate_system = sl::COORDINATE_SYSTEM::RIGHT_HANDED_Y_UP;

// CHECK THESE:
init_params.sdk_verbose = false;  // Verbose logging can slow things down
init_params.sdk_gpu_id = 0;       // Correct GPU?
init_params.camera_image_flip = sl::FLIP_MODE::OFF;
init_params.enable_image_enhancement = true;  // Can this be disabled for speed?
```

**Try:**
- Disable image enhancement
- Disable verbose logging
- Minimize processing in grab() call

---

### Phase 4: Comparative Testing (1 hour)

#### 4.1 ZED Tools Baseline
**Method:**
Use official ZED tools to record at 60 FPS, compare results

```bash
# ZED CLI recorder
/usr/local/zed/tools/ZED_SVO_Recording \
  --resolution HD720 \
  --fps 60 \
  --output test_zed_cli.svo2 \
  --duration 240

# Check actual FPS in ZED Explorer
```

**Expected Result:**
- If ZED CLI achieves 60 FPS → Our code has overhead
- If ZED CLI also achieves 40 FPS → Camera/SDK limitation

#### 4.2 Lower Resolution Test
**Method:**
Record at VGA @ 100 FPS (simpler resolution)

**Hypothesis:** If VGA achieves 100 FPS, HD720 should achieve 60 FPS (similar pixel throughput)

**Calculation:**
- VGA: 672×376 = 252,672 pixels × 100 FPS = 25.3 Mpixels/s
- HD720: 1280×720 = 921,600 pixels × 60 FPS = 55.3 Mpixels/s
- HD720 needs 2.2× more throughput

---

### Phase 5: Optimization Attempts (3 hours)

Based on Phase 1-4 findings, try optimizations:

#### 5.1 If Storage is Bottleneck
- Test with SSD instead of USB drive
- Try different USB 3.0 ports
- Disable filesystem journaling (risky but faster)

#### 5.2 If GPU is Bottleneck
- Increase GPU clock: `sudo jetson_clocks`
- Set max power mode: `sudo nvpmodel -m 0`
- Disable power management

#### 5.3 If Compression is Bottleneck
- Try different compression settings in SVO2
- Reduce compression quality for speed
- Test H.264/H.265 (if NVENC available)

#### 5.4 If grab() is Bottleneck
- Reduce grab() timeout
- Disable depth computation
- Simplify camera parameters

#### 5.5 If CPU is Bottleneck
- Profile with `perf` to find hot spots
- Optimize hot loops
- Reduce logging overhead

---

## 📊 Data Collection Template

### Test Recording Template
```
Test ID: PA-001
Date: YYYY-MM-DD HH:MM
Configuration:
  - Resolution: HD720 (1280×720)
  - FPS Setting: 60
  - Mode: SVO2
  - Depth: Disabled
  - Duration: 240s

Environment:
  - Power Mode: [MAXN/5W/10W/15W]
  - GPU Clock: [XXX MHz]
  - CPU Clock: [XXX MHz]
  - Temperature: [XX°C]
  - Storage: [USB 3.0 / SSD / Other]

Results:
  - Target FPS: 60
  - Actual FPS: XX.X
  - Frame Count: XXXX
  - Missing Frames: XXXX
  - File Size: X.XX GB
  - Avg Write Speed: XXX MB/s

Observations:
  - [Any warnings/errors]
  - [CPU/GPU utilization notes]
  - [Temperature throttling?]

Conclusion:
  - [Identified bottleneck or limitation]
```

---

## 🎯 Success Criteria

### Optimization Goals
1. **Minimum Acceptable:** 50+ FPS actual (83% of target)
2. **Good:** 55+ FPS actual (92% of target)
3. **Excellent:** 58+ FPS actual (97% of target)
4. **Perfect:** 60 FPS actual (100% of target)

### Decision Points
- **If 60 FPS achievable:** Update docs, remove limitation warning
- **If 50-59 FPS achievable:** Update docs with realistic expectations
- **If <50 FPS achievable:** Document as hardware limitation, recommend 30 FPS default

---

## 📝 Deliverables

### Phase Outputs
1. **Baseline Report:** Current FPS performance with detailed measurements
2. **Bottleneck Analysis:** Identified primary limiting factor(s)
3. **Optimization Results:** Performance improvements achieved (if any)
4. **Final Recommendation:** Update defaults or accept limitation

### Documentation Updates
1. Update `PRE_RELEASE_v1.3_STABLE.md` with findings
2. Create `PERFORMANCE_OPTIMIZATION_RESULTS.md`
3. Update Web UI with realistic FPS expectations
4. Update field testing guide with optimized settings

---

## 🚀 Next Steps After Analysis

### If Optimization Possible
1. Implement identified optimizations
2. Re-test all recording modes
3. Update documentation
4. Publish v1.3 Stable

### If Hardware Limited
1. Document hardware limitation clearly
2. Recommend 30 FPS as default (stable 28-30 FPS)
3. Add FPS performance table to docs
4. Consider "Performance Mode" vs "Stable Mode" presets
5. Publish v1.3 Stable with clarified expectations

---

## ⏱️ Estimated Timeline

- **Phase 1 (Baseline):** 1 hour
- **Phase 2 (Isolation):** 2 hours
- **Phase 3 (Profiling):** 2 hours
- **Phase 4 (Comparison):** 1 hour
- **Phase 5 (Optimization):** 3 hours
- **Documentation:** 1 hour

**Total:** ~10 hours of focused investigation

---

## 🔧 Quick Start Commands

```bash
# Baseline test
sudo ./build/apps/drone_web_controller/drone_web_controller 2>&1 | tee baseline_log.txt

# Resource monitoring (separate terminal)
sudo tegrastats --interval 1000 | tee gpu_baseline.txt

# Storage speed test
sudo hdparm -tT /dev/sda1

# ZED CLI comparison
/usr/local/zed/tools/ZED_SVO_Recording --resolution HD720 --fps 60 --output test.svo2 --duration 60

# Max performance mode
sudo jetson_clocks
sudo nvpmodel -m 0

# Check current clocks
sudo jetson_clocks --show
```

---

**Ready to Begin Performance Analysis!**
