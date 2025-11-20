# INA219 Battery Monitor Setup Guide

## Hardware: SOLDERED INA219 Power Monitor

### Wiring Connections

**Jetson Orin Nano 40-Pin Header → INA219:**

| Jetson Pin | Pin Name | INA219 Pin | Purpose |
|------------|----------|------------|---------|
| Pin 1 or 17 | 3.3V | VCC | Power for INA219 chip |
| Pin 6, 9, 14, 20, 25, 30, 34, or 39 | GND | GND | Ground |
| Pin 27 | I2C_GP0_DAT (SDA) | SDA | I2C Data |
| Pin 28 | I2C_GP0_CLK (SCL) | SCL | I2C Clock |

**Power Monitoring Connections:**

| INA219 Pin | Connection |
|------------|------------|
| VIN+ | Battery positive (or load positive) |
| VIN- | Through shunt resistor to load |

**Note:** The INA219 measures current through its internal shunt resistor between VIN+ and VIN-.

### I2C Address Configuration

The INA219 supports 4 addresses via solder jumpers A0 and A1:

| A0 | A1 | Address |
|----|----|---------| 
| Open | Open | 0x40 (default) |
| Closed | Open | 0x41 |
| Open | Closed | 0x44 |
| Closed | Closed | 0x45 |

**Default:** 0x40 (no jumpers soldered)

### Specifications (SOLDERED INA219)

- **Voltage Range:** 0-26V
- **Current Range:** Depends on shunt resistor
- **Shunt Resistor:** Check board marking (typically 0.1Ω or 0.01Ω)
- **I2C Interface:** 3.3V logic level
- **I2C Speed:** Up to 400kHz (Fast Mode)

### Troubleshooting

#### INA219 Not Detected

Run diagnostic script:
```bash
python3 diagnose_ina219.py
```

**Common Issues:**

1. **Wrong I2C Bus**
   - Pin 27/28 = Bus 0 (I2C_GP0)
   - Pin 3/5 = Bus 1 (I2C_GP1) 
   - Make sure you're using pins 27/28!

2. **No Power to INA219**
   - Measure voltage between VCC and GND pins (should be 3.3V)
   - Check 3.3V rail on Jetson is working

3. **SDA/SCL Swapped**
   - Try swapping SDA and SCL connections

4. **Address Conflict**
   - Bus 1 address 0x40 is used by Jetson's INA3221
   - Make sure INA219 is on bus 0 (pins 27/28)

5. **Faulty Module**
   - Test INA219 on known-working system (Raspberry Pi)
   - Check for physical damage

#### Verifying Connection

Scan I2C bus 0:
```bash
sudo i2cdetect -y -r 0
```

You should see a device at 0x40 (or 0x41, 0x44, 0x45 depending on jumpers).

### Alternative: Use Jetson's Built-in INA3221

If the external INA219 doesn't work, the Jetson Orin Nano has a built-in INA3221 power monitor:

```bash
python3 test_jetson_power.py
```

**Limitation:** INA3221 monitors Jetson's input voltage (VDD_IN), not the battery directly. You'll need a voltage divider or separate battery monitoring if using this approach.

### Test Scripts

| Script | Purpose |
|--------|---------|
| `scan_ina219.py` | Auto-detect INA219 on all buses |
| `diagnose_ina219.py` | Comprehensive hardware diagnostic |
| `test_ina219.py` | Test script for INA219 (needs detection first) |
| `test_jetson_power.py` | Uses Jetson's built-in INA3221 |
| `scan_i2c_bus0.sh` | Scan all addresses on bus 0 |

### Next Steps

1. **Verify hardware connection** using diagnostic script
2. **Confirm I2C address** shows up in i2cdetect
3. **Run test script** to verify voltage/current readings
4. **Integrate into main application** once verified

### Integration into Drone Controller

Once INA219 is detected and working:

1. Add battery monitoring class to `common/hardware/`
2. Integrate with `drone_web_controller` for:
   - Real-time battery voltage display
   - Low battery warnings (< 14.8V for 4S)
   - Critical battery shutdown (< 14.6V for 4S)
   - Automatic recording stop on low battery
3. Add battery status to web UI
4. Log battery data with recordings

### Safety Thresholds (4S LiPo)

| Condition | Voltage/Cell | Total (4S) | Action |
|-----------|--------------|------------|--------|
| Fully Charged | 4.2V | 16.8V | Normal operation |
| Nominal | 3.7V | 14.8V | Normal operation |
| Low Battery | 3.7V | 14.8V | Warning message |
| Critical | 3.65V | 14.6V | Stop recording, shutdown |
| Damaged | < 3.0V | < 12.0V | Battery may be damaged |

**Never discharge below 3.0V per cell!**
