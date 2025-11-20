# Jetson Orin Nano I2C Pinout Reference

## 📌 I2C Port Locations

### I2C_IDA_0 (Bus 0)
**Location:** 4-pin JST connector on Jetson carrier board  
**Linux Device:** `/dev/i2c-0`  
**Hardware Address:** `0x3160000.i2c`  
**GPIO Pins:** Pin 27 (SDA_0), Pin 28 (SCL_0)

### I2C_IDA_1 (Bus 7)  
**Location:** 4-pin JST connector on Jetson carrier board  
**Linux Device:** `/dev/i2c-7`  
**Hardware Address:** `0xc250000.i2c`  
**GPIO Pins:** Pin 29 (SDA_1), Pin 30 (SCL_1)

## 🔌 Pin Configuration (Both Ports - Same Layout)

```
┌─────────────────────┐
│  1   2   3   4      │  ← Connector View (looking at board)
│ VCC SDA SCL GND     │
└─────────────────────┘

Pin 1: VCC (3.3V Power)    - Red wire typical
Pin 2: SDA (Data Line)     - White/Yellow wire
Pin 3: SCL (Clock Line)    - Green/Blue wire  
Pin 4: GND (Ground)        - Black wire
```

## ⚡ Electrical Specifications

- **Voltage Level:** 3.3V (DO NOT connect 5V devices without level shifter!)
- **Pull-up Resistors:** Built-in on most modules, 2.2kΩ - 4.7kΩ typical
- **Clock Speed:** Standard 100kHz, Fast 400kHz supported
- **Max Current per Pin:** 50mA (check Jetson datasheet)

## 📍 Common I2C Device Addresses

### Display Devices
- **LCD 16x2 with PCF8574 I2C backpack:** 0x27 or 0x3F
- **OLED SSD1306 128x64:** 0x3C or 0x3D

### Sensors
- **INA219 Power Monitor:** 0x40 (default), 0x41, 0x44, 0x45 (with jumpers)
- **BMP280 Pressure Sensor:** 0x76 or 0x77
- **MPU6050 IMU:** 0x68 or 0x69
- **ADS1115 ADC:** 0x48, 0x49, 0x4A, 0x4B

### System Reserved (Jetson Internal)
- **0x50, 0x57:** EEPROM (shows as UU in i2cdetect on Bus 0)

## 🔧 Wiring Tips

### Good Wiring Practice
✅ Keep I2C wires short (<30cm for reliable 100kHz)  
✅ Use twisted pair for SDA/SCL to reduce noise  
✅ Connect GND first, VCC last (assembly order)  
✅ Disconnect VCC first, GND last (disassembly order)  
✅ Double-check polarity before powering on  

### Bad Wiring Practice
❌ Long wires (>1m) without proper termination  
❌ Parallel routing with power cables (interference)  
❌ Mixing 3.3V and 5V devices without level shifter  
❌ Multiple pull-up resistors on same bus (too low resistance)  
❌ Hot-plugging devices while powered  

## 🧪 Testing Procedure

### 1. Visual Inspection
- [ ] All pins properly seated in connector
- [ ] No bent or broken pins
- [ ] Correct polarity (VCC to VCC, GND to GND)
- [ ] No solder bridges on device PCB

### 2. Power Check (Multimeter Required)
```
With device connected but system powered OFF:
- Resistance between VCC and GND: Should be >100kΩ (no short circuit)

With system powered ON:
- Voltage on Pin 1 (VCC): Should read 3.3V ±0.1V
- Voltage on Pin 4 (GND): Should read 0V
- Voltage on Pin 2 (SDA) idle: Should read ~3.3V (pulled high)
- Voltage on Pin 3 (SCL) idle: Should read ~3.3V (pulled high)
```

### 3. Device Detection
```bash
# Scan all I2C buses
sudo i2cdetect -y 0  # Bus 0 (I2C_IDA_0)
sudo i2cdetect -y 7  # Bus 7 (I2C_IDA_1)

# Quick device-specific test
sudo i2cget -y 0 0x40 0x00  # Read from INA219
sudo i2cget -y 7 0x27 0x00  # Read from LCD
```

### 4. Communication Test
```bash
# Run diagnostic tools
sudo ./diagnose_i2c_bus0           # Hardware diagnostic
sudo ./quick_i2c_test.sh           # Quick device scan
sudo python3 test_ina219.py        # INA219 specific test
sudo ./simple_lcd_test             # LCD test
```

## ⚠️ Troubleshooting Guide

### Symptom: No devices detected on bus

**Possible Causes:**
1. **Loose connection** → Reseat connector firmly
2. **Wrong bus number** → Try other buses (0, 1, 2, 7)
3. **Device not powered** → Check 3.3V on VCC pin
4. **Wrong I2C address** → Check device documentation
5. **Bus hardware failure** → Test on known working bus first

**Solutions:**
```bash
# Check if device appears on any bus
for bus in 0 1 2 7; do
    echo "=== Bus $bus ==="
    sudo i2cdetect -y $bus 2>&1 | grep -v Warning
done

# Test device on different bus
# If device works on Bus 7 but not Bus 0 → Bus 0 hardware issue
```

### Symptom: Device detected but read/write fails

**Possible Causes:**
1. **Timing issues** → Try slower clock speed
2. **Voltage too low** → Measure VCC (should be 3.3V)
3. **Interference** → Check wiring, add capacitor near device
4. **Driver conflict** → Check dmesg for errors

**Solutions:**
```bash
# Check kernel messages
dmesg | grep i2c

# Try manual read
sudo i2cget -y 0 0x40 0x00  # Should return hex value

# Check for driver conflicts
lsmod | grep i2c
```

### Symptom: Bus shows "UU" at address

**Meaning:** A kernel driver is using that address  
**Example:** Bus 0 shows UU at 0x50, 0x57 (system EEPROM)  
**Action:** Use different address for your device, or different bus

## 📊 Current Project Configuration

**drone_web_controller Production Setup:**
- **LCD Display (16x2):** Bus 7, Address 0x27
- **INA219 (planned):** Bus 7, Address 0x40 (different address, no conflict!)

**Why both on Bus 7:**
- Bus 0 suspected hardware issue under investigation
- Different I2C addresses = No conflict
- Simpler wiring (single cable bundle)
- Proven stable in field testing

## 🔗 Useful Commands

```bash
# List all I2C buses
ls -la /sys/class/i2c-adapter/

# Check I2C kernel modules
lsmod | grep i2c

# Detailed device info
cat /sys/class/i2c-adapter/i2c-0/name
cat /sys/class/i2c-adapter/i2c-7/name

# Monitor I2C traffic (if i2c-tools-dev installed)
sudo i2cdump -y 0 0x40  # Dump all registers from INA219

# Check device tree
ls /proc/device-tree/ | grep i2c
```

## 📚 References

- [Jetson Orin Nano Developer Kit Carrier Board Spec](https://developer.nvidia.com/embedded/downloads)
- [I2C Specification (NXP)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [Linux I2C Tools Documentation](https://www.kernel.org/doc/Documentation/i2c/i2c-tools)

---

**Last Updated:** November 19, 2025  
**Applicable to:** Jetson Orin Nano Developer Kit
