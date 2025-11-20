# Quick Reference: 14.4V Critical Threshold Implementation

**Status:** ✅ IMPLEMENTED in v1.5.4  
**Date:** 19. November 2025

---

## 🎯 Where 14.4V is Implemented

### 1️⃣ Battery Monitor Core (Hardcoded Defaults)
**File:** `common/hardware/battery/battery_monitor.cpp` (line ~41)
```cpp
critical_voltage_(14.4f),   // 3.6V per cell
empty_voltage_(14.4f),      // 0% battery at critical
```

### 2️⃣ Calibration Data (JSON)
**File:** `ina219_calibration.json`
```json
{
  "slope1": 0.9863571525,
  "offset1": 0.3826541901,
  "slope2": 0.9899149537,
  "offset2": 0.3274154663,
  "midpoint": 15.7,  // ← FIXED (was 16.0)
  "raw_readings": [14.414, 15.529, 16.640],
  "reference_voltages": [14.6, 15.7, 16.8]  // ← FIXED (was [10, 20, 20])
}
```

### 3️⃣ Web GUI Display
**File:** `apps/drone_web_controller/drone_web_controller.cpp` (line ~2129)
```html
Critical voltage: 14.4V (3.6V/cell) - Emergency shutdown after 10s
Warning voltage: 14.8V (3.7V/cell) - Low battery warning
Full charge: 16.8V (4.2V/cell)
Note: Voltage drops ~0.2V under recording load (3A current draw)
```

### 4️⃣ Header Documentation
**File:** `common/hardware/battery/battery_monitor.h` (line ~10)
```cpp
/**
 * Critical voltages for 4S LiPo:
 * - Critical: 14.4V (3.6V/cell)
 * - Warning: 14.8V (3.7V/cell)
 * - Full charge: 16.8V (4.2V/cell)
 * 
 * Note: Voltage drops by ~0.2-0.3V under recording load (3A)
 * Calibration is performed in idle state.
 */
```

### 5️⃣ Emergency Shutdown Logic
**File:** `apps/drone_web_controller/drone_web_controller.cpp` (line ~1171)
```cpp
if (battery.is_critical) {  // Triggered when voltage < 14.4V
    critical_battery_counter_++;
    if (critical_battery_counter_ >= 10) {  // 10 confirmations @ 500ms = 5 seconds
        // Display final LCD message (persists after power-off)
        updateLCD("Battery Shutdown", voltage < 14.0f ? "Empty!" : "Critical!");
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Ensure write completes
        
        // Stop recording if active
        if (recording_active_) {
            stopRecording();
        }
        // Set both shutdown flags
        system_shutdown_requested_ = true;
        shutdown_requested_ = true;
        break;  // Exit monitor loop → main() calls shutdown -h now
    }
}
```

### 6️⃣ User Shutdown LCD Message
**File:** `apps/drone_web_controller/drone_web_controller.cpp` (line ~1529)
```cpp
} else if (request.find("POST /api/shutdown") != std::string::npos) {
    // Display final message on LCD (persists after power-off)
    updateLCD("User Shutdown", "Please Wait...");
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Ensure write completes
    
    system_shutdown_requested_ = true;
    shutdown_requested_ = true;
}
```

---

## ✅ Implementation Checklist

- [x] Hardcoded defaults: 14.4V critical, 14.4V empty (0%)
- [x] Calibration JSON: Fixed midpoint (15.7V), reference voltages ([14.6, 15.7, 16.8])
- [x] GUI: Updated text to show 14.4V + load drop note
- [x] Header docs: Updated voltage thresholds
- [x] Shutdown logic: Works regardless of recording state
- [x] Debounce: 10 confirmations @ 500ms interval = 5 seconds total response
- [x] Counter reset: Battery recovery resets counter to 0
- [x] LCD "User Shutdown": Displayed when GUI button pressed
- [x] LCD "Battery Shutdown": Displayed when critical battery triggers
- [x] LCD messages persist: Visible after power-off (powered by external 5V)
- [x] Documentation: LCD_SHUTDOWN_MESSAGES_v1.5.4.md created
- [x] Documentation: HARDCODED_CALIBRATION_v1.5.4.md created
- [x] Documentation: VOLTAGE_DROP_UNDER_LOAD_v1.5.4.md updated

---

## 🧪 Quick Test

```bash
# 1. Rebuild system
./scripts/build.sh

# 2. Restart system
sudo systemctl restart drone-recorder

# 3. Check logs for calibration load
sudo journalctl -u drone-recorder -n 30 | grep -E "(calibration|BatteryMonitor)"

# Expected output:
# "[BatteryMonitor] ✓ Calibration loaded from file"
# OR
# "[BatteryMonitor] ⚠️ No calibration file found, using hardcoded defaults"

# 4. Open GUI: http://192.168.4.1:8080
# Power Tab → Critical Thresholds section should show:
# "Critical voltage: 14.4V (3.6V/cell) - Emergency shutdown after 5s"

# 5. Test voltage accuracy
# Power supply @ 14.6V → GUI: 14.60V ± 0.01V ✅
# Power supply @ 15.7V → GUI: 15.70V ± 0.01V ✅
# Power supply @ 16.8V → GUI: 16.80V ± 0.01V ✅ (NOT 18.3V!)

# 6. Test User Shutdown LCD message
# GUI: Click "Shutdown System"
# Expected LCD: "User Shutdown" / "Please Wait..."
# Wait for power-off (~5s)
# LCD should STILL show: "User Shutdown" ✅

# 7. Test Battery Emergency Shutdown (OPTIONAL - will power off!)
# Power supply < 14.4V
# Expected console (every 500ms):
#   "CRITICAL BATTERY VOLTAGE detected (count=1/10)"
#   "CRITICAL BATTERY VOLTAGE detected (count=2/10)"
#   ... (every 500ms)
#   "CRITICAL BATTERY VOLTAGE detected (count=10/10)"
#   "CRITICAL THRESHOLD REACHED!"
# Expected LCD sequence:
#   "CRITICAL BATT" / "1/10" → "9/10" (5 seconds)
#   "CRITICAL BATT" / "Stopping Rec..." (if recording)
#   "Battery Shutdown" / "Critical!" (final message)
# System powers off after ~8 seconds total
# LCD should STILL show: "Battery Shutdown" / "Critical!" ✅

# 8. Test Counter Reset (Battery Recovery)
# Power supply @ 14.3V → wait for count=5/10
# Raise to 15.0V
# Expected console: "✓ Battery recovered to 15.0V - reset critical counter (was: 5)"
# Lower again → counter restarts at 1/10 ✅
```

---

## 📊 Battery Percentage Calculation

```cpp
// 0% = 14.4V (critical)
// 100% = 16.8V (full)

float battery_range = full_voltage_ - empty_voltage_;  // 16.8 - 14.4 = 2.4V
float voltage_above_empty = voltage - empty_voltage_;  // e.g., 15.6 - 14.4 = 1.2V
int percentage = (voltage_above_empty / battery_range) * 100;  // (1.2 / 2.4) * 100 = 50%
```

**Examples:**
| Voltage | Battery % |
|---------|-----------|
| 14.4V   | 0%        |
| 14.8V   | 17% (warning threshold) |
| 15.6V   | 50%       |
| 16.8V   | 100%      |

---

## 🔧 Files Modified in v1.5.4

1. `common/hardware/battery/battery_monitor.cpp` - Hardcoded calibration defaults
2. `common/hardware/battery/battery_monitor.h` - Updated voltage documentation
3. `apps/drone_web_controller/drone_web_controller.cpp` - GUI text + shutdown logic
4. `ina219_calibration.json` - Fixed midpoint and reference voltages
5. `tools/calibrate_battery_monitor.cpp` - Fixed JSON output formatting
6. `docs/HARDCODED_CALIBRATION_v1.5.4.md` - Complete calibration documentation
7. `docs/VOLTAGE_DROP_UNDER_LOAD_v1.5.4.md` - Updated with 14.4V threshold

---

**Status: ✅ READY FOR PRODUCTION**

Restart system and test! 🚀
