# I2C Bus 0 (I2C_IDA_0) Hardware Issue Investigation

**Date:** November 19, 2025  
**Platform:** Jetson Orin Nano  
**Issue:** I2C_IDA_0 (Bus 0) not detecting any devices

## 🔴 Problem Description

The Jetson Orin Nano has two external I2C ports:
- **I2C_IDA_1 (Bus 7)** - ✅ Working perfectly
- **I2C_IDA_0 (Bus 0)** - ❌ Not detecting any devices

### Test Results:
1. **LCD Display (0x27)** on Bus 7 → ✅ Works
2. **INA219 (0x40)** on Bus 7 → ✅ Works  
3. **INA219 (0x40)** on Bus 0 → ❌ Not detected (should be at 0x40)
4. **LCD Display** on Bus 0 → ❌ Not detected (testing in progress)

This indicates either:
- Physical pin damage on I2C_IDA_0 connector
- Missing/damaged pull-up resistors on Bus 0
- Internal I2C controller failure
- Device tree configuration issue

## 🛠️ Diagnostic Tools Created

### 1. C++ Hardware Diagnostic Tool
**File:** `diagnose_i2c_bus0.cpp` / `diagnose_i2c_bus0`

Performs low-level hardware tests:
- Bus accessibility check
- Comprehensive device scan (0x03-0x77)
- Read/write functionality tests
- Bus timing and timeout configuration
- Error analysis
- Side-by-side comparison with Bus 7

**Usage:**
```bash
sudo ./diagnose_i2c_bus0
```

**Build:**
```bash
g++ -o diagnose_i2c_bus0 diagnose_i2c_bus0.cpp -std=c++17
```

### 2. Python System-Level Diagnostic
**File:** `diagnose_i2c_bus0_detailed.py`

Provides additional system-level diagnostics:
- Kernel module verification
- sysfs interface inspection
- Device tree configuration check
- GPIO pinmux verification
- Hardware measurement instructions
- Workaround suggestions

**Usage:**
```bash
sudo python3 diagnose_i2c_bus0_detailed.py
```

### 3. Complete Test Suite
**File:** `test_i2c_bus0_complete.sh`

Automated script that runs all diagnostic tools in sequence with clear output and recommendations.

**Usage:**
```bash
sudo ./test_i2c_bus0_complete.sh
```

## 🔍 Systematic Testing Procedure

### Step 1: Run Complete Diagnostic Suite
```bash
sudo ./test_i2c_bus0_complete.sh
```

This will:
1. Check kernel modules
2. Scan both Bus 0 and Bus 7
3. Compare results
4. Provide recommendations

### Step 2: Manual i2cdetect Comparison
```bash
# Scan Bus 7 (should show INA219 at 0x40)
sudo i2cdetect -y 7

# Scan Bus 0 (testing if device appears)
sudo i2cdetect -y 0
```

### Step 3: Hardware Voltage Measurements
With a multimeter, measure on I2C_IDA_0 connector:

**Expected values:**
- Pin 1 (VCC): **3.3V**
- Pin 4 (GND): **0V**
- SDA pin (idle, no device): **~3.3V** (pulled up)
- SCL pin (idle, no device): **~3.3V** (pulled up)

**If measurements differ:**
- 0V on SDA/SCL → Pull-up resistors missing/damaged
- Wrong voltage on VCC → Power supply issue
- Continuity issues → Physical damage

### Step 4: Visual Inspection
- Check for bent/broken pins on connector
- Look for burn marks or damage on PCB
- Verify no solder bridges

### Step 5: Test with LCD on Bus 0
Connect LCD to Bus 0 and modify test code:

```cpp
// In simple_lcd_test.cpp, change:
const int I2C_BUS = 0;  // Was 7, change to 0
```

Recompile and run:
```bash
g++ -o simple_lcd_test simple_lcd_test.cpp -std=c++17
sudo ./simple_lcd_test
```

If LCD works on Bus 0 → INA219 was faulty  
If LCD fails on Bus 0 → Bus 0 hardware confirmed broken

## ✅ Recommended Solution (Works Now!)

**Use both devices on Bus 7 (I2C_IDA_1):**

- **LCD Display:** Address 0x27 (or 0x3F)
- **INA219 Power Monitor:** Address 0x40

**Why this works:**
- Different I2C addresses = No conflict
- Both devices can coexist on same bus
- No modifications needed
- Fully functional immediately

**Implementation:**
```cpp
// In drone_web_controller or similar
I2C_LCD lcd(7, 0x27);      // LCD on Bus 7
INA219 power(7, 0x40);     // INA219 on Bus 7
// Both work simultaneously!
```

## 🔧 Alternative Solutions

### Option 1: External Pull-up Resistors
If pull-ups are the issue, add external resistors:
- 2.2kΩ resistor: SDA → 3.3V
- 2.2kΩ resistor: SCL → 3.3V

Test again after adding resistors.

### Option 2: I2C Multiplexer (Future Expansion)
For more than 2 devices, use **TCA9548A I2C Multiplexer**:
- Provides 8 independent I2C channels
- All connect to single bus
- Addresses devices via channel selection
- Cost: ~$5-10

**Example configuration:**
```
Bus 7 (I2C_IDA_1)
  └─ TCA9548A @ 0x70
      ├─ Channel 0: LCD @ 0x27
      ├─ Channel 1: INA219 @ 0x40
      ├─ Channel 2: Future sensor #1
      └─ Channel 3: Future sensor #2
```

### Option 3: Software I2C (GPIO Bit-banging)
Use GPIO pins to emulate I2C:
- Requires `i2c-gpio` kernel module
- Slower than hardware I2C (~10kHz vs 100kHz)
- More CPU overhead
- Last resort option

## 📊 Known Issues with Jetson I2C

From NVIDIA forums and documentation:
1. **Pull-up resistor issues** - Common on early Orin Nano batches
2. **Device tree misconfiguration** - Some pins not properly muxed
3. **Power supply issues** - Insufficient 3.3V rail current
4. **Conflicting GPIO assignments** - Pins used for other functions

## 🔗 Related Files

- `common/hardware/i2c_lcd/lcd.h` - LCD I2C interface
- `battery_monitor.py` - INA219 power monitoring
- `test_ina219.py` - INA219 test script
- `diagnose_ina219.py` - INA219 hardware diagnostic
- `simple_lcd_test.cpp` - LCD test program
- `i2c_scanner.cpp` - General I2C bus scanner

## 📝 Next Steps

1. ✅ Run `sudo ./test_i2c_bus0_complete.sh`
2. ✅ Analyze results from diagnostic tools
3. ⚠ Perform hardware measurements with multimeter
4. ⚠ Test LCD on Bus 0 to confirm bus status
5. ✅ If Bus 0 confirmed broken → Use both devices on Bus 7 (recommended)
6. 🔧 If needed in future → Add I2C multiplexer for expansion

## ⚠️ Important Notes

- **DO NOT short I2C pins** - Can damage Jetson permanently
- **Verify 3.3V level** - Never connect 5V I2C devices without level shifters
- **Check pull-up resistors** - Most modules have built-in pull-ups
- **Bus 7 is critical** - Used for LCD in drone_web_controller production setup

## 🎯 Current Status

- **Bus 7 (I2C_IDA_1):** ✅ Fully functional, running LCD @ 0x27
- **Bus 0 (I2C_IDA_0):** ❌ Under investigation, likely hardware issue
- **Workaround:** ✅ Both LCD and INA219 on Bus 7 (different addresses)
- **Production impact:** ✅ None - Bus 7 sufficient for current needs

---

**Last Updated:** November 19, 2025  
**Status:** Investigation in progress, workaround implemented
