#!/usr/bin/env python3
"""
INA219 Hardware Diagnostic Tool
Tests physical connection and I2C communication
"""

import sys
import time

print("=" * 70)
print("INA219 HARDWARE DIAGNOSTIC TOOL")
print("=" * 70)
print()

# Step 1: Check if INA219 library is available
print("[STEP 1] Checking INA219 Python library...")
try:
    from ina219 import INA219
    print("  ✓ INA219 library is installed")
except ImportError:
    print("  ✗ INA219 library NOT installed")
    print("  Install with: pip3 install pi-ina219")
    sys.exit(1)

print()

# Step 2: Try to communicate with INA219 on different buses and addresses
print("[STEP 2] Scanning for INA219 on all buses and addresses...")
print()

test_configs = [
    # (bus, address, description)
    (0, 0x40, "Bus 0, Default address (no jumpers)"),
    (0, 0x41, "Bus 0, A0 shorted"),
    (0, 0x44, "Bus 0, A1 shorted"),
    (0, 0x45, "Bus 0, A0+A1 shorted"),
    (1, 0x40, "Bus 1, Default address"),
    (1, 0x41, "Bus 1, A0 shorted"),
    (1, 0x44, "Bus 1, A1 shorted"),
    (1, 0x45, "Bus 1, A0+A1 shorted"),
    (7, 0x40, "Bus 7, Default address"),
    (7, 0x41, "Bus 7, A0 shorted"),
]

found_devices = []

for bus, addr, desc in test_configs:
    try:
        # Create INA219 instance with conservative settings
        ina = INA219(shunt_ohms=0.1, max_expected_amps=2.0, busnum=bus, address=addr)
        
        # Try to configure (this will fail if device doesn't exist)
        ina.configure(ina.RANGE_16V, ina.GAIN_1_40MV)
        
        # Try to read voltage
        voltage = ina.voltage()
        current = ina.current()
        
        print(f"  ✓ {desc}")
        print(f"    Voltage: {voltage:.3f}V")
        print(f"    Current: {current:.1f}mA")
        print()
        
        found_devices.append((bus, addr, voltage))
        
    except OSError as e:
        error_msg = str(e)
        if "Device or resource busy" in error_msg:
            print(f"  ⚠ {desc}")
            print(f"    Device exists but is used by kernel driver")
            print()
        elif "No such file or directory" in error_msg:
            # Bus doesn't exist
            pass
        # else: device not present (don't print anything)
    except Exception as e:
        error_msg = str(e)
        if "out of range" not in error_msg.lower():
            print(f"  ✗ {desc}")
            print(f"    Error: {error_msg}")
            print()

print("=" * 70)

# Step 3: Results
if found_devices:
    print("[RESULT] ✓ Found INA219 device(s)!")
    print()
    for bus, addr, voltage in found_devices:
        print(f"  Bus: {bus}")
        print(f"  Address: 0x{addr:02X}")
        print(f"  Voltage: {voltage:.3f}V")
        print()
    print("Use these settings in your monitoring script.")
else:
    print("[RESULT] ✗ NO INA219 devices found!")
    print()
    print("TROUBLESHOOTING CHECKLIST:")
    print()
    print("1. POWER:")
    print("   - VCC connected to Jetson 3.3V (Pin 1 or 17)")
    print("   - GND connected to Jetson GND (Pin 6, 9, 14, etc.)")
    print("   - Measure voltage between VCC and GND (should be 3.3V)")
    print()
    print("2. I2C CONNECTIONS:")
    print("   - SDA connected to Pin 27 (I2C_GP0_DAT)")
    print("   - SCL connected to Pin 28 (I2C_GP0_CLK)")
    print("   - Double-check you're not using Pin 3/5 (that's bus 1)")
    print()
    print("3. ADDRESS CONFIGURATION:")
    print("   - Default is 0x40 (no solder jumpers)")
    print("   - Check if A0 or A1 jumpers are accidentally bridged")
    print()
    print("4. HARDWARE TEST:")
    print("   - Try connecting INA219 to a known-working I2C device")
    print("   - Or try the INA219 on a Raspberry Pi to verify it works")
    print()
    print("5. ALTERNATIVE:")
    print("   - Use Jetson's built-in INA3221 for power monitoring")
    print("   - Run: python3 test_jetson_power.py")

print("=" * 70)

sys.exit(0 if found_devices else 1)
