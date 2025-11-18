# GUI Restructuring Plan - v1.4

## 📊 Current Structure Analysis (v1.3)

### Existing Sections (Single-Page Layout)
1. **Status Display** - IDLE/RECORDING/STOPPING with animations
2. **Recording Mode** - SVO2, SVO2+Depth Info, SVO2+Depth Images, RAW
3. **Depth Settings** - Mode selection, FPS slider (conditional display)
4. **Camera Settings** - Resolution/FPS dropdown, Shutter speed slider
5. **Progress Display** - Progress bar, elapsed/remaining time, file size, speed
6. **Control Buttons** - START, STOP, SHUTDOWN

### Current Constraints
- Mobile-optimized (max-width: 500px)
- Single-page scroll layout
- Conditional sections (depth settings only visible when needed)
- No navigation structure
- Limited space for new features

### Current Strengths
- Clean, professional design
- Good color coding (green=idle, yellow=recording, red=error)
- Responsive button states
- Real-time updates (1s interval)

---

## 🎯 New Requirements

1. **Livestream View**
   - MJPEG/JPEG snapshot display (target 1Hz)
   - Fullscreen button (landscape mode, manual trigger)
   - Enable/disable toggle
   - Low latency preview

2. **Gain Control**
   - Slider 0-100 (ZED SDK range)
   - Suggested ranges:
     - **0-20**: Low gain (bright conditions, outdoor)
     - **21-50**: Medium gain (indoor, normal light) ⭐ Default: 30
     - **51-80**: High gain (low light, indoor dark)
     - **81-100**: Maximum gain (extreme low light, noisy)

3. **Battery Monitor** (future)
   - Voltage display
   - Current draw
   - Power consumption
   - Battery percentage (calculated)
   - Warning indicators

---

## 🎨 Proposed Design: Tab-Based Layout

### Tab Structure

```
┌────────────────────────────────────────────┐
│  [Recording] [Livestream] [System] [Power] │ ← Tab Navigation
├────────────────────────────────────────────┤
│                                            │
│            Tab Content Area                │
│                                            │
│                                            │
└────────────────────────────────────────────┘
```

### Tab 1: Recording 🎥
**Primary recording controls (current functionality)**

```
Status: RECORDING ⏺
━━━━━━━━━━━━━━━━━ 75% ━━━━━━━━━━

Recording Mode
○ SVO2 (Standard)
○ SVO2 + Depth Info
○ SVO2 + Depth Images
○ RAW Frames

[Depth Settings] (collapsible)
┌─────────────────────────┐
│ Mode: Neural Lite ▼     │
│ FPS: [====●====] 10     │
└─────────────────────────┘

Progress
┌─────────────────────────┐
│ Elapsed:    180s        │
│ Remaining:   60s        │
│ File Size: 6.8 GB       │
│ Speed:    25.3 MB/s     │
└─────────────────────────┘

[START RECORDING]  [STOP]
```

### Tab 2: Livestream 📹
**Live preview + camera settings**

```
┌───────────────────────────────┐
│                               │
│      [LIVESTREAM PREVIEW]     │
│         640×360               │
│                               │
└───────────────────────────────┘

☐ Enable Livestream (1 FPS)
[⛶ Fullscreen]  [📷 Snapshot]

Camera Settings
┌─────────────────────────┐
│ Resolution: HD720@60 ▼  │
│ Shutter: [===●==] 1/120 │
│ Gain:    [===●==] 30    │
│                         │
│ Auto | Low | Med | High │
│       (Suggestions)     │
└─────────────────────────┘

Live Stats
• FPS: 60.2 • Exposure: 50%
• Gain: 30 • Temp: 45°C
```

### Tab 3: System ⚙️
**System status, network, storage**

```
System Status
━━━━━━━━━━━━━━━━━━━━━━━━━━
WiFi AP: ✅ Active
SSID: DroneController
IP: 192.168.4.1:8080

Storage
━━━━━━━━━━━━━━━━━━━━━━━━━━
USB: DRONE_DATA (NTFS)
Free: 87.3 GB / 128 GB
[████████░░] 68%

Temperature: 48°C 🌡️
CPU Usage: 45%
Memory: 3.2GB / 8GB

[SHUTDOWN SYSTEM]
```

### Tab 4: Power 🔋
**Battery monitoring (future)**

```
Battery Status
━━━━━━━━━━━━━━━━━━━━━━━━━━
┌─────────────────────────┐
│    ⚡ 12.4V  ⚠️ 45%     │
│   [████████░░░░] 45%   │
└─────────────────────────┘

Voltage:  12.4V (3S LiPo)
Current:  2.3A
Power:    28.5W
Capacity: 4500mAh (est.)

Time Remaining: ~1h 15m

Emergency Shutdown: 10.5V
Status: ✅ OK

[TEST EMERGENCY]  [SETTINGS]
```

---

## 🎨 Design Specifications

### Tab Navigation Bar
```css
.tabs {
    display: flex;
    background: #343a40;
    border-radius: 8px 8px 0 0;
    overflow: hidden;
}

.tab {
    flex: 1;
    padding: 12px 8px;
    color: #adb5bd;
    background: #495057;
    border: none;
    cursor: pointer;
    font-weight: bold;
    font-size: 14px;
    transition: all 0.3s;
}

.tab.active {
    color: white;
    background: #28a745;
}

.tab:hover {
    background: #5a6268;
}
```

### Tab Content
```css
.tab-content {
    display: none;
    padding: 20px;
    animation: fadeIn 0.3s;
}

.tab-content.active {
    display: block;
}

@keyframes fadeIn {
    from { opacity: 0; transform: translateY(-10px); }
    to { opacity: 1; transform: translateY(0); }
}
```

### Livestream Fullscreen
```css
.fullscreen-container {
    position: fixed;
    top: 0; left: 0;
    width: 100vw;
    height: 100vh;
    background: black;
    z-index: 9999;
    display: none;
}

.fullscreen-container.active {
    display: flex;
    justify-content: center;
    align-items: center;
}

.fullscreen-image {
    max-width: 100%;
    max-height: 100%;
    object-fit: contain;
}

.fullscreen-exit {
    position: absolute;
    top: 20px;
    right: 20px;
    background: rgba(220, 53, 69, 0.9);
    color: white;
    border: none;
    padding: 10px 20px;
    border-radius: 6px;
    cursor: pointer;
    font-weight: bold;
}
```

### Gain Slider with Visual Ranges
```html
<div class='select-group'>
    <label>Gain: <span id='gainValue'>30</span></label>
    <input type='range' id='gainSlider' min='0' max='100' value='30' 
           oninput='setCameraGain(this.value)'>
    <div class='gain-guide'>
        <span style='color:#28a745'>●</span> 0-20 Bright
        <span style='color:#ffc107'>●</span> 21-50 Normal
        <span style='color:#ff9800'>●</span> 51-80 Low Light
        <span style='color:#dc3545'>●</span> 81-100 Dark
    </div>
</div>
```

---

## 🔧 Implementation Steps

### Phase 1: Tab Structure (Foundation)
1. Add tab navigation HTML
2. Add tab switching JavaScript
3. Split current content into Tab 1 (Recording)
4. Test navigation, ensure all functions work

### Phase 2: Livestream Tab
1. Add `/api/snapshot` endpoint (C++)
2. Create Tab 2 with image display
3. Add JavaScript polling (1Hz)
4. Implement fullscreen mode
5. Move camera settings to this tab

### Phase 3: System Tab
1. Create Tab 3 layout
2. Add storage info to status API
3. Add temperature monitoring
4. Move shutdown button here

### Phase 4: Gain Control
1. Add gain slider to Livestream tab
2. Implement `setCameraGain()` in ZEDRecorder
3. Add `/api/set_camera_gain` endpoint
4. Test gain ranges in different lighting

### Phase 5: Power Tab (Future)
1. Integrate I2C battery monitor
2. Create Tab 4 with battery display
3. Add voltage/current/power readings
4. Implement emergency shutdown logic

---

## 📱 Mobile Considerations

### Responsive Breakpoints
```css
/* Mobile portrait (default) */
.container { max-width: 500px; }

/* Mobile landscape (fullscreen livestream) */
@media (orientation: landscape) {
    .fullscreen-container {
        /* Optimize for 16:9 landscape */
    }
}

/* Tablet (if needed) */
@media (min-width: 768px) {
    .container { max-width: 700px; }
    .tabs { font-size: 16px; }
}
```

### Touch Optimization
- Larger tap targets (min 44×44px)
- Swipe gestures for tab switching (optional)
- Pinch-to-zoom in fullscreen (optional)

---

## 🎯 Benefits of New Design

### User Experience
✅ Less scrolling - everything in logical tabs
✅ Dedicated livestream view
✅ Professional multi-page feel
✅ Fullscreen for better framing
✅ Clear visual gain guidance

### Technical
✅ Modular structure (easy to add tabs)
✅ Lazy loading potential (only active tab updates)
✅ Better separation of concerns
✅ Easier to maintain/extend

### Future-Proof
✅ Ready for battery tab
✅ Room for more features (GPS, telemetry, etc.)
✅ Can add settings/preferences tab
✅ Scalable architecture

---

## 📝 API Extensions Required

### New Endpoints
```cpp
GET  /api/snapshot              // Return JPEG from ZED left camera
POST /api/set_camera_gain       // Set gain (0-100)
GET  /api/system_info           // Storage, temp, network status
GET  /api/battery_status        // Voltage, current, power (future)
```

### Enhanced Status API
```json
{
    "camera_gain": 30,
    "camera_temperature": 45.2,
    "storage_free_gb": 87.3,
    "storage_total_gb": 128.0,
    "cpu_usage_percent": 45,
    "memory_used_gb": 3.2
}
```

---

**Next Step:** Implement Phase 1 (Tab Structure) while keeping all existing functionality intact.
