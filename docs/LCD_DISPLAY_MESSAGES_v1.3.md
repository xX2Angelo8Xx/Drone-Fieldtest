# LCD Display Messages - v1.3 Final

## Bootup Sequence (Optimized - 4 messages total)

### Message 1 (800ms)
```
System Bootup
Initializing...
```

### Message 2 (during camera init)
```
Init Code
Loading camera..
```

### Message 3 (800ms)
```
Launching
Web Server...
```

### Message 4 (2 seconds, then transitions to status display)
```
Ready!
10.42.0.1:8080
```

---

## Recording Display (2-page alternating cycle, 3 seconds per page)

### Page 1: Recording Mode
```
Line 1: Time 45/240s
Line 2: SVO2
```

Or for depth modes:
```
Line 1: Time 45/240s
Line 2: SVO2+RawDepth
```

Or:
```
Line 1: Time 45/240s
Line 2: SVO2+DepthPNG
```

### Page 2: Camera Settings (Resolution @ FPS + Exposure)
```
Line 1: Time 48/240s
Line 2: 720@60 E:Auto
```

Or with manual exposure:
```
Line 1: Time 48/240s
Line 2: 720@60 E:50
```

Examples for different resolutions:
```
720@60 E:Auto
720@30 E:75
1080@30 E:Auto
2K@15 E:40
VGA@100 E:Auto
```

---

## Display Cycle Timing

- **Bootup total:** ~4 seconds (4 messages)
- **Recording display:** 2 pages × 3 seconds = 6 seconds per full cycle
- **Update frequency:** Every 3 seconds (smooth, readable)
- **Character limit:** 16 characters per line (2×16 LCD)

---

## Character Count Examples

### Line 2 Examples (must fit in 16 chars)
```
"SVO2"            →  4 chars ✅
"SVO2+RawDepth"   → 14 chars ✅
"SVO2+DepthPNG"   → 14 chars ✅
"RAW"             →  3 chars ✅
"720@60 E:Auto"   → 14 chars ✅
"720@60 E:50"     → 12 chars ✅
"1080@30 E:75"    → 13 chars ✅
"2K@15 E:100"     → 12 chars ✅
"VGA@100 E:Auto"  → 15 chars ✅
```

All messages fit within 16-character limit! ✅

---

## Benefits of Optimized Display

1. **Bootup:**
   - Reduced from many messages to 4 clear stages
   - User sees: System → Code → Web → Ready
   - Web address visible for 2 seconds
   - No spam, clear progress

2. **Recording:**
   - Only 2 pages instead of 3
   - Each update shows complete info in line 2
   - Mode + Settings both visible every 6 seconds
   - Exposure value included (Auto or manual)
   - More information density per page

3. **Readability:**
   - 3-second display time per page (comfortable)
   - 16-char limit respected
   - Compact but clear abbreviations
   - Time always visible in line 1

---

## Technical Implementation

### Bootup (initialize() function)
```cpp
lcd_->displayMessage("System Bootup", "Initializing...");
std::this_thread::sleep_for(std::chrono::milliseconds(800));

lcd_->displayMessage("Init Code", "Loading camera..");
// Camera initialization...

lcd_->displayMessage("Launching", "Web Server...");
std::this_thread::sleep_for(std::chrono::milliseconds(800));

updateLCD("Ready!", "10.42.0.1:8080");
std::this_thread::sleep_for(std::chrono::seconds(2));
```

### Recording (recordingMonitorLoop() function)
```cpp
// Line 1: Always time
line1 << "Time " << elapsed << "/" << recording_duration_seconds_ << "s";

// Line 2: Alternate between 2 pages (0 and 1)
if (lcd_display_cycle_ == 0) {
    // Page 1: Mode
    line2 << "SVO2" or "SVO2+RawDepth" or ...
} else {
    // Page 2: Settings
    line2 << res << "@" << fps << " E:" << (auto ? "Auto" : exposure);
}

// Toggle: 0 → 1 → 0 → 1 (not 0 → 1 → 2)
lcd_display_cycle_ = (lcd_display_cycle_ + 1) % 2;
```

---

## User Experience

**Scenario: User starts recording at 720@60 with exposure 50**

```
00:00 - Time 0/240s | SVO2
00:03 - Time 3/240s | 720@60 E:50
00:06 - Time 6/240s | SVO2
00:09 - Time 9/240s | 720@60 E:50
...
```

User sees:
- Recording progress (line 1)
- Mode confirmation every 6 seconds
- Settings confirmation every 6 seconds
- Complete picture in 2 pages, not 3

**Much cleaner and more informative!** ✅
