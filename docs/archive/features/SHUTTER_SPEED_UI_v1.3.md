# Shutter Speed UI Implementation (v1.3)

## Overview
Replaced raw 0-100 exposure slider with conventional photography shutter speed notation. Backend still uses ZED's 0-100 exposure system, but frontend presents photographer-friendly interface.

## Conversion Formula
```
Exposure% = (FPS / shutter_denominator) × 100
```

**Examples at 60 FPS:**
- 1/60 shutter → 100% exposure
- 1/120 shutter → 50% exposure  ⭐ (default)
- 1/240 shutter → 25% exposure

## Standard Shutter Speeds

| Index | Shutter | Exposure | Label | Notes |
|-------|---------|----------|-------|-------|
| 0 | - | -1 | Auto | ZED auto-exposure |
| 1 | 1/60 | 100 | 1/60 | Maximum exposure at 60 FPS |
| 2 | 1/90 | 67 | 1/90 | |
| 3 | 1/120 | 50 | 1/120 | ⭐ Default (2× shutter rule) |
| 4 | 1/150 | 40 | 1/150 | |
| 5 | 1/180 | 33 | 1/180 | |
| 6 | 1/240 | 25 | 1/240 | Fast action |
| 7 | 1/480 | 13 | 1/480 | Very fast action |
| 8 | 1/1000 | 6 | 1/1000 | Maximum shutter speed |

## UI Components

### Slider Configuration
```html
<input type='range' id='exposureSlider' 
       min='0' max='9' value='3' step='1' 
       oninput='setCameraExposure(this.value)'>
```
- **Range**: 0-9 (10 discrete positions)
- **Default**: 3 (1/120 shutter speed)
- **Step**: 1 (discrete jumps only)

### Display Format
```
Shutter Speed: 1/120 (E:50)
               ^^^^^   ^^^^
               Label   Actual exposure (gray text)
```
- **Main label**: Photography-standard notation (1/60, 1/120, etc.)
- **Debug info**: Actual ZED exposure value in parentheses (gray, 11px font)

### Mode Info
```
Shutter speeds: Auto, 1/60, 1/90, 1/120 ⭐, 1/150, 1/180, 1/240, 1/480, 1/1000
```
- Lists all available shutter speeds
- Star (⭐) marks recommended default

## JavaScript Implementation

### Data Structure
```javascript
const shutterSpeeds = [
    {s: 0, e: -1, label: 'Auto'},
    {s: 60, e: 100, label: '1/60'},
    {s: 90, e: 67, label: '1/90'},
    {s: 120, e: 50, label: '1/120'},
    {s: 150, e: 40, label: '1/150'},
    {s: 180, e: 33, label: '1/180'},
    {s: 240, e: 25, label: '1/240'},
    {s: 480, e: 13, label: '1/480'},
    {s: 1000, e: 6, label: '1/1000'}
];
```
- **s**: Shutter denominator (0 = auto)
- **e**: ZED exposure value (-1 = auto, 0-100 = percentage)
- **label**: Display string for UI

### Setting Exposure (User Interaction)
```javascript
function setCameraExposure(shutterIndex) {
    let selected = shutterSpeeds[shutterIndex];
    document.getElementById('exposureValue').textContent = selected.label;
    document.getElementById('exposureActual').textContent = '(E:' + selected.e + ')';
    fetch('/api/set_camera_exposure', {
        method: 'POST',
        body: 'exposure=' + selected.e
    });
}
```
1. User moves slider → gets index (0-9)
2. Looks up shutter speed in array
3. Updates display (label + actual exposure)
4. Sends exposure value to backend API

### Syncing from Camera State (Page Load)
```javascript
function updateStatus() {
    fetch('/api/status').then(r => r.json()).then(data => {
        if (data.camera_exposure !== undefined) {
            let exposure = data.camera_exposure;
            let shutterIndex = 0; // Default to Auto
            
            if (exposure === -1) {
                shutterIndex = 0;
            } else {
                // Find closest matching shutter speed
                let minDiff = 999;
                for (let i = 1; i < shutterSpeeds.length; i++) {
                    let diff = Math.abs(shutterSpeeds[i].e - exposure);
                    if (diff < minDiff) {
                        minDiff = diff;
                        shutterIndex = i;
                    }
                }
            }
            
            // Update UI to match camera state
            document.getElementById('exposureSlider').value = shutterIndex;
            document.getElementById('exposureValue').textContent = shutterSpeeds[shutterIndex].label;
            document.getElementById('exposureActual').textContent = '(E:' + exposure + ')';
        }
    });
}
```
1. Fetches current camera state from API
2. Reads exposure value (-1 or 0-100)
3. Finds closest matching shutter speed
4. Updates slider position and display

## Default Exposure Setting

### C++ Implementation
```cpp
// After camera initialization in initialize()
if (camera_resolution_ == RecordingMode::HD720_60FPS || 
    camera_resolution_ == RecordingMode::VGA_100FPS) {
    svo_recorder_->setCameraExposure(50);  // 1/120 at 60fps
    std::cout << "[Controller] Set default exposure: 50% (1/120 shutter)" << std::endl;
}
```

### Rationale
- **60 FPS**: 1/120 shutter (50% exposure)
- **Follows 2× shutter rule**: Shutter speed should be approximately 2× frame rate
- **Good motion capture**: Balances sharpness and light gathering
- **Photography standard**: Common default for action/sports photography

## LCD Display Integration

### Recording Display (2-Page Cycle)
**Page 1 - Mode:**
```
Time 45/240s
SVO2
```

**Page 2 - Settings:**
```
Time 45/240s
720@60 E:50
```
or
```
Time 45/240s
720@60 E:Auto
```

### Display Format
- **Resolution**: 720, 1080, 2K, VGA
- **FPS**: @15, @30, @60, @100
- **Exposure**: E:50 (numeric) or E:Auto

## Testing Checklist

### Slider Positions
- [ ] Index 0: "Auto (E:-1)" → camera uses auto-exposure
- [ ] Index 1: "1/60 (E:100)" → maximum exposure at 60 FPS
- [ ] Index 2: "1/90 (E:67)" → mid-range
- [ ] Index 3: "1/120 (E:50)" → default, motion capture
- [ ] Index 4: "1/150 (E:40)" → faster shutter
- [ ] Index 5: "1/180 (E:33)" → even faster
- [ ] Index 6: "1/240 (E:25)" → fast action
- [ ] Index 7: "1/480 (E:13)" → very fast action
- [ ] Index 8: "1/1000 (E:6)" → maximum shutter speed

### Default Behavior
- [ ] On fresh startup: slider at position 3 (1/120)
- [ ] Display shows "1/120 (E:50)"
- [ ] 60 FPS profile: sets 50% exposure automatically
- [ ] Page refresh: slider syncs with current camera state

### API Response
- [ ] `/api/status` includes `camera_exposure` field
- [ ] `/api/set_camera_exposure` accepts exposure parameter
- [ ] Exposure value correctly applied to ZED camera
- [ ] LCD shows updated exposure value during recording

### Different FPS Settings
- [ ] 30 FPS + 1/120 shutter = 25% exposure (appropriate)
- [ ] 60 FPS + 1/120 shutter = 50% exposure (default)
- [ ] 100 FPS + 1/120 shutter = 83% exposure (near maximum)
- [ ] Auto mode works at all FPS settings

## Benefits

### For Photographers
- ✅ Familiar notation (1/60, 1/120, etc.)
- ✅ Standard shutter speed increments
- ✅ 2× shutter rule default (1/120 at 60 FPS)
- ✅ Quick access to common action photography settings

### For Debugging
- ✅ Actual exposure value visible in gray text
- ✅ Can verify ZED API receives correct value
- ✅ Easy to diagnose conversion issues

### For Field Testing
- ✅ Faster adjustments (discrete steps, no fine-tuning)
- ✅ Less cognitive load (photography language, not percentages)
- ✅ Better motion capture defaults

## Future Enhancements (Optional)

### Additional Shutter Speeds
- Slower: 1/15, 1/8 (low light, motion blur effects)
- Faster: 1/1500, 1/2000, 1/4000 (extreme action)

### FPS-Aware Display
- Show which shutter speeds are valid for current FPS
- Disable/gray out unrealistic combinations
- Example: At 30 FPS, 1/60 is maximum practical shutter

### Presets
- "Motion Capture": 1/120 (default)
- "Low Light": Auto or 1/60
- "Fast Action": 1/240
- "Extreme Action": 1/480

## Technical Notes

### Why Not Use Frame Time Directly?
- ZED API uses percentage-based exposure (0-100)
- Percentage is consistent across frame rates
- Simpler for internal camera logic
- We convert to shutter speed only in UI layer

### Precision
- Exposure percentages are approximations
- Example: 1/180 at 60 FPS = 33.33% → rounded to 33%
- Close enough for photography use cases
- Exact value shown in debug text if needed

### Auto Mode
- Exposure = -1 triggers ZED's auto-exposure algorithm
- Camera dynamically adjusts based on scene brightness
- Useful for varying lighting conditions
- May not be ideal for consistent motion capture

## Version History
- **v1.3**: Initial shutter speed UI implementation
- Default: 1/120 at 60 FPS
- 10 discrete positions (Auto + 8 shutter speeds)
- Sync on page load
- Gray text for actual exposure value
