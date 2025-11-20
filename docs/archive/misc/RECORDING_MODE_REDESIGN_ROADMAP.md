# 🚧 IMPLEMENTATION ROADMAP - Recording Mode Redesign
**Created: November 17, 2024**
**Status: In Progress**

---

## ✅ COMPLETED

### 1. New Recording Mode Enum
- ✅ Updated `RecordingModeType` enum in `drone_web_controller.h`
  - `SVO2` - Standard SVO2 (no depth)
  - `SVO2_DEPTH_INFO` - SVO2 + raw 32-bit depth data (.depth files)
  - `SVO2_DEPTH_IMAGES` - SVO2 + depth visualization PNG images

### 2. Depth Data Writer Class
- ✅ Created `depth_data_writer.h` - Interface for raw depth saving
- ✅ Created `depth_data_writer.cpp` - Implementation
  - Binary .depth file format (header + 32-bit float array)
  - Configurable FPS (default 10 FPS)
  - Thread-safe capture loop
  - Atomic frame counter and FPS tracking

### 3. Depth Viewer Tool
- ✅ Created `tools/depth_viewer.cpp`
  - Commands: view, convert, batch, info
  - Reads .depth binary files
  - Converts to colorized PNG (COLORMAP_JET)
  - Statistical analysis (min/max/avg depth)

### 4. CMakeLists Updates
- ✅ Added `depth_data_writer.cpp` to `zed_camera` library
- ✅ Added `depth_viewer` target to `tools/CMakeLists.txt`

---

## 🔧 TODO - HIGH PRIORITY

### 5. Update Web Controller Implementation

#### A. Replace all `RecordingModeType::SVO2_DEPTH_VIZ` → `SVO2_DEPTH_IMAGES`
**Files to modify:**
- `apps/drone_web_controller/drone_web_controller.cpp`

**Changes needed:**
```cpp
// OLD
if (recording_mode_ == RecordingModeType::SVO2_DEPTH_VIZ)

// NEW
if (recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES)
```

**Occurrences**: ~15-20 places

#### B. Add `SVO2_DEPTH_INFO` Mode Handler
**Location**: `drone_web_controller.cpp` - `startRecording()` function

**Implementation**:
```cpp
} else if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO) {
    // SVO2 + Raw Depth Data Mode
    
    // Create depth_data directory
    std::string depth_dir = storage_->getRecordingDir() + "/depth_data";
    fs::create_directories(depth_dir);
    
    // Initialize DepthDataWriter
    depth_data_writer_ = std::make_unique<DepthDataWriter>();
    int fps = depth_recording_fps_.load();
    if (!depth_data_writer_->init(depth_dir, fps)) {
        std::cerr << "[WEB_CONTROLLER] Failed to init depth data writer" << std::endl;
        updateLCD("Recording Error", "Depth Init Failed");
        return false;
    }
    
    // Start depth data capture
    depth_data_writer_->start(svo_recorder_->getCamera());
    std::cout << "[WEB_CONTROLLER] Depth data capture started (" << fps << " FPS)" << std::endl;
}
```

**Additional member variable needed**:
```cpp
// In drone_web_controller.h
#include "depth_data_writer.h"

// In private members:
std::unique_ptr<DepthDataWriter> depth_data_writer_;
```

#### C. Update `stopRecording()` for Depth Data Writer
**Location**: `drone_web_controller.cpp` - `stopRecording()` function

**Add**:
```cpp
// Stop depth data writer if running
if (depth_data_writer_) {
    std::cout << "[WEB_CONTROLLER] Stopping depth data writer..." << std::endl;
    depth_data_writer_->stop();
    depth_data_writer_.reset();
}
```

---

### 6. LCD Display Improvements During Recording

#### Current behavior:
```
Line 1: "Recording"
Line 2: "Active"
```

#### New behavior (alternating every 2 seconds):
```
State 1:
Line 1: "REC 045/240"        (frame 45 of 240)
Line 2: "HD720@30fps"        (recording mode)

State 2:
Line 1: "REC 046/240"
Line 2: "NEURAL_LITE"        (depth mode, if enabled)

State 3 (if depth mode):
Line 1: "REC 047/240"
Line 2: "Depth: 9.8fps"      (actual depth capture FPS)
```

#### Implementation:
**Add to `drone_web_controller.h`**:
```cpp
private:
    std::atomic<int> lcd_display_state_{0};  // 0, 1, 2 for cycling
    std::atomic<int> recording_frame_count_{0};
```

**Modify `recordingMonitorLoop()`**:
```cpp
void DroneWebController::recordingMonitorLoop() {
    int lcd_update_counter = 0;
    
    while (recording_active_ && !shutdown_requested_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        
        // Check if recording duration reached
        if (elapsed >= recording_duration_seconds_) {
            // ... existing stop logic
        }
        
        // Update LCD with cycling information
        updateRecordingLCD(elapsed);
        
        updateRecordingStatus();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        lcd_update_counter++;
        if (lcd_update_counter >= 2) {  // Switch every 2 seconds
            lcd_update_counter = 0;
            int current_state = lcd_display_state_.load();
            lcd_display_state_ = (current_state + 1) % 3;
        }
    }
}
```

**Add new function `updateRecordingLCD()`**:
```cpp
void DroneWebController::updateRecordingLCD(int elapsed_seconds) {
    int remaining = recording_duration_seconds_ - elapsed_seconds;
    
    // Line 1: Always show time
    char line1[17];
    snprintf(line1, sizeof(line1), "REC %03d/%03ds", elapsed_seconds, recording_duration_seconds_);
    
    // Line 2: Cycle through different info
    char line2[17];
    int display_state = lcd_display_state_.load();
    
    switch (display_state) {
        case 0: {
            // Show recording mode
            std::string mode_str = getCurrentModeString();  // e.g. "HD720@30fps"
            snprintf(line2, sizeof(line2), "%-16s", mode_str.c_str());
            break;
        }
        case 1: {
            // Show depth mode (if enabled)
            if (recording_mode_ != RecordingModeType::SVO2) {
                std::string depth_str = getDepthModeShortName(depth_mode_);
                snprintf(line2, sizeof(line2), "%-16s", depth_str.c_str());
            } else {
                snprintf(line2, sizeof(line2), "%-16s", "Standard SVO2");
            }
            break;
        }
        case 2: {
            // Show depth FPS (if capturing depth)
            if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO && depth_data_writer_) {
                float depth_fps = depth_data_writer_->getCurrentFPS();
                snprintf(line2, sizeof(line2), "Depth: %.1f fps", depth_fps);
            } else if (recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
                // Get current depth viz FPS from thread
                snprintf(line2, sizeof(line2), "Viz: %d FPS", depth_recording_fps_.load());
            } else {
                snprintf(line2, sizeof(line2), "%-16s", "No Depth");
            }
            break;
        }
    }
    
    updateLCD(line1, line2);
}

std::string DroneWebController::getCurrentModeString() const {
    if (!svo_recorder_) return "Unknown";
    
    RecordingMode mode = svo_recorder_->getCurrentMode();
    switch (mode) {
        case RecordingMode::HD720_30FPS: return "HD720@30fps";
        case RecordingMode::HD720_60FPS: return "HD720@60fps";
        case RecordingMode::HD1080_30FPS: return "HD1080@30fps";
        case RecordingMode::HD2K_15FPS: return "HD2K@15fps";
        case RecordingMode::SVGA_60FPS: return "SVGA@60fps";
        case RecordingMode::VGA_100FPS: return "VGA@100fps";
        default: return "Unknown";
    }
}
```

---

### 7. Web UI Updates

#### Update HTML Form
**File**: `drone_web_controller.cpp` - `generateMainPage()`

**OLD**:
```html
<option value="svo2">SVO2 (Standard)</option>
<option value="svo2_depth_viz">SVO2 + Depth (10 FPS)</option>
<option value="raw">RAW Frames</option>
```

**NEW**:
```html
<option value="svo2">SVO2 (Standard)</option>
<option value="svo2_depth_info">SVO2 + Depth Info (Fast, 32-bit raw)</option>
<option value="svo2_depth_images">SVO2 + Depth Images (PNG visualization)</option>
```

#### Update API Handler
**Location**: `handleClientRequest()` - `/api/set_recording_mode`

**Add**:
```cpp
} else if (mode_str.find("svo2_depth_info") != std::string::npos) {
    setRecordingMode(RecordingModeType::SVO2_DEPTH_INFO);
    response = generateAPIResponse("Recording mode set to SVO2 + Depth Info");
} else if (mode_str.find("svo2_depth_images") != std::string::npos) {
    setRecordingMode(RecordingModeType::SVO2_DEPTH_IMAGES);
    response = generateAPIResponse("Recording mode set to SVO2 + Depth Images");
```

---

### 8. Update Mode Switch Logic

**File**: `drone_web_controller.cpp` - `setRecordingMode()`

**Current**:
```cpp
if (mode == RecordingModeType::RAW_FRAMES && svo_recorder_) {
    // Close SVO recorder, will open raw recorder
    svo_recorder_->close();
    svo_recorder_.reset();
}
else if (mode == RecordingModeType::SVO2 && raw_recorder_) {
    // Close raw recorder, will open SVO recorder
    raw_recorder_->close();
    raw_recorder_.reset();
}
```

**NEW** (simplified, all modes use SVO recorder):
```cpp
// No need to switch recorders - all modes use svo_recorder_
// Only difference is what we do with depth data:
// - SVO2: No depth computation
// - SVO2_DEPTH_INFO: Depth to .depth files
// - SVO2_DEPTH_IMAGES: Depth to PNG images

// Just enable/disable depth computation based on mode
if (mode == RecordingModeType::SVO2) {
    svo_recorder_->enableDepthComputation(false);
} else {
    sl::DEPTH_MODE zed_depth_mode = convertDepthMode(depth_mode_);
    svo_recorder_->enableDepthComputation(true, zed_depth_mode);
}
```

---

### 9. ZEDRecorder getCamera() Method

**Add to** `zed_recorder.h`:
```cpp
public:
    sl::Camera& getCamera() { return zed_; }
```

This allows `DepthDataWriter` to access the camera for depth retrieval.

---

### 10. Build and Test

#### Build Steps:
```bash
cd /home/angelo/Projects/Drone-Fieldtest
./scripts/build.sh
```

#### Test Sequence:
1. **SVO2 Mode** (no depth):
   - Start recording
   - Check LCD: should show "HD720@30fps" / "Standard SVO2"
   - Check output: only `video.svo2` file
   
2. **SVO2_DEPTH_INFO Mode**:
   - Start recording
   - Check LCD: should cycle through mode/depth mode/FPS
   - Check output: `video.svo2` + `depth_data/*.depth` files
   - Test viewer: `./build/tools/depth_viewer info depth_data/depth_000001.depth`
   
3. **SVO2_DEPTH_IMAGES Mode**:
   - Start recording
   - Check LCD: should show visualization FPS
   - Check output: `video.svo2` + `depth_viz/*.png` files

---

## 📝 IMPLEMENTATION PRIORITY

1. ✅ Core infrastructure (DepthDataWriter, viewer) - **DONE**
2. ⏳ Update RecordingModeType references - **NEXT**
3. ⏳ Add SVO2_DEPTH_INFO mode handler - **NEXT**
4. ⏳ LCD display improvements - **NEXT**
5. ⏳ Web UI updates - **AFTER TESTING**
6. ⏳ Build and test - **FINAL**

---

## 🎯 ESTIMATED TIME

- Recording mode updates: **30 minutes**
- LCD improvements: **20 minutes**
- Web UI updates: **15 minutes**
- Build & debug: **30 minutes**
- Testing all 3 modes: **30 minutes**

**Total: ~2 hours**

---

## 💡 NEXT STEPS

Want me to:
1. **Continue with implementation** (do all the changes above)
2. **Stop here and test autostart first** (Phase 1 complete)
3. **Document and commit current progress** (safe checkpoint)

**Your choice!** 🚀
