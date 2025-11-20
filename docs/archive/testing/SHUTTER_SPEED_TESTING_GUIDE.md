> Note (archived): Shutter testing merged into `docs/guides/testing/TESTING_AND_PERFORMANCE_GUIDE.md`.

# Shutter Speed Implementation - Testing Guide

## Quick Test Commands

### Start System
```bash
# Manual start (for testing)
sudo ./build/apps/drone_web_controller/drone_web_controller

# Or using systemd service
sudo systemctl start drone-recorder
sudo journalctl -u drone-recorder -f
```

### Access Web UI
```
http://192.168.4.1:8080  (from WiFi AP)
http://10.42.0.1:8080    (from Ethernet)
```

## Test Scenarios

### 1. Default Exposure on Startup
**Expected:**
- Slider at position 3 (middle-right)
- Display shows "Shutter Speed: 1/120 (E:50)"
- LCD during recording shows "720@60 E:50"

**Steps:**
1. Start drone_web_controller
2. Open web UI in browser
3. Check slider position (should be at 3)
4. Check display text
5. Start recording
6. Check LCD second page

### 2. Slider Movement
**Test all positions:**

| Move slider to | Expected display | Expected exposure |
|----------------|------------------|-------------------|
| 0 (leftmost) | Shutter Speed: Auto (E:-1) | -1 |
| 1 | Shutter Speed: 1/60 (E:100) | 100 |
| 2 | Shutter Speed: 1/90 (E:67) | 67 |
| 3 | Shutter Speed: 1/120 (E:50) | 50 |
| 4 | Shutter Speed: 1/150 (E:40) | 40 |
| 5 | Shutter Speed: 1/180 (E:33) | 33 |
| 6 | Shutter Speed: 1/240 (E:25) | 25 |
| 7 | Shutter Speed: 1/480 (E:13) | 13 |
| 8 (rightmost) | Shutter Speed: 1/1000 (E:6) | 6 |

**Steps:**
1. Move slider to each position
2. Verify display updates immediately
3. Verify gray text shows correct exposure value
4. Start recording and check LCD matches

### 3. Page Refresh Sync
**Expected:**
- Slider position maintained after refresh
- Display shows correct shutter speed
- Settings persist

**Steps:**
1. Set slider to position 6 (1/240)
2. Refresh browser (F5 or Ctrl+R)
3. Wait for updateStatus() to complete
4. Verify slider still at position 6
5. Verify display shows "1/240 (E:25)"

### 4. Different Recording Modes
**Expected:**
- Exposure settings work with all recording modes
- LCD shows exposure on all modes
- SVO2, SVO2+Depth, RAW all respect exposure

**Steps:**
1. Set shutter to 1/120
2. Select "SVO2 Only" mode → Start recording → Check LCD
3. Stop recording
4. Select "SVO2 + Raw Depth" → Start recording → Check LCD
5. Stop recording
6. Select "SVO2 + Depth PNG" → Start recording → Check LCD

### 5. Auto Mode
**Expected:**
- Camera uses automatic exposure
- LCD shows "E:Auto"
- Camera adapts to lighting conditions

**Steps:**
1. Set slider to position 0 (Auto)
2. Start recording
3. Check LCD shows "720@60 E:Auto"
4. Cover camera lens → brightness should adapt
5. Remove cover → brightness should adapt

### 6. Multiple Shutter Speed Changes
**Expected:**
- Each change applies immediately
- No lag or incorrect values
- Display always matches slider

**Steps:**
1. Start recording
2. Move slider: 3 → 6 → 2 → 4 → 3
3. For each change, verify display updates
4. Check video brightness changes appropriately
5. Stop recording

## Visual Verification

### Web UI Elements
Check these elements exist and look correct:
- [ ] Label says "Shutter Speed:" (not "Exposure:")
- [ ] Main value (e.g., "1/120") is normal font size
- [ ] Actual exposure (e.g., "(E:50)") is smaller and gray
- [ ] Slider has 10 discrete positions (0-9)
- [ ] Mode info lists all speeds with ⭐ on 1/120

### LCD Display
Check LCD shows correct format:
- [ ] Page 1: "Time n/240s" / "SVO2"
- [ ] Page 2: "Time n/240s" / "720@60 E:50"
- [ ] Page 2 with Auto: "Time n/240s" / "720@60 E:Auto"
- [ ] Display cycles every 3 seconds
- [ ] Fits within 16 characters

## Exposure Behavior Verification

### Brightness Tests
**Test different lighting conditions:**

1. **Bright outdoor:** Try 1/240 or 1/480 (faster shutter, darker image)
2. **Indoor:** Try 1/120 or 1/60 (slower shutter, brighter image)
3. **Auto mode:** Should adapt automatically

**Expected:**
- Faster shutter (1/240, 1/480) = darker image, less motion blur
- Slower shutter (1/60, 1/90) = brighter image, more motion blur
- 1/120 at 60 FPS = balanced (follows 2× rule)

### Motion Blur Tests
**Record moving object:**

1. **1/60 shutter:** Moderate motion blur
2. **1/120 shutter:** Minimal motion blur (default)
3. **1/240 shutter:** Very sharp, frozen motion

**Expected:**
- Higher shutter speed = less blur, sharper motion
- Lower shutter speed = more blur, smoother motion

## Known Limitations

### Frame Rate Dependencies
- At 30 FPS: 1/60 is maximum practical shutter (100% exposure)
- At 60 FPS: 1/120 is maximum practical shutter (100% exposure)
- At 100 FPS: 1/200 is maximum practical shutter (100% exposure)
- Higher shutters are possible but may underexpose

### Conversion Approximations
- Some shutters use rounded exposure values:
  - 1/90 = 67% (actually 66.67%)
  - 1/180 = 33% (actually 33.33%)
- Close enough for practical use
- Exact value shown in gray text

## Troubleshooting

### Slider Not Moving
**Problem:** Slider doesn't move or jumps back
**Solution:** Check browser console for JavaScript errors

### Display Not Updating
**Problem:** Display shows wrong shutter speed
**Solution:** 
1. Check `/api/status` returns `camera_exposure`
2. Verify `updateStatus()` is called on page load
3. Check browser console for fetch errors

### Exposure Not Applied
**Problem:** Camera brightness doesn't change
**Solution:**
1. Check `/api/set_camera_exposure` endpoint works
2. Verify ZED camera accepts exposure value
3. Check logs for exposure setting errors

### Wrong Default
**Problem:** Doesn't default to 1/120
**Solution:**
1. Verify slider `value='3'` in HTML
2. Check C++ code sets 50% after camera init
3. Verify resolution is HD720_60FPS

### LCD Shows Wrong Value
**Problem:** LCD shows "E:0" or incorrect exposure
**Solution:**
1. Check `svo_recorder_->getCurrentExposure()` method exists
2. Verify exposure value is updated in status API
3. Check LCD display code reads correct variable

## Success Criteria

✅ **Implementation Complete When:**
1. Slider has 10 positions (0-9)
2. Display shows shutter speed notation (1/60, 1/120, etc.)
3. Gray text shows actual exposure value
4. Default is 1/120 (E:50) for 60 FPS
5. Page refresh syncs slider with camera state
6. LCD shows "720@60 E:50" during recording
7. All shutter speeds apply correctly to camera
8. Brightness changes match shutter speed selection

## Testing Completed By
- [ ] Angelo (field test)
- [ ] Date: ___________
- [ ] Notes: _________________________________

## Field Test Notes
_Use this space to record observations during field testing:_
- Preferred shutter speed for different scenarios
- Lighting conditions where Auto mode works well
- Any unexpected behavior
- Suggestions for additional shutter speeds
