#!/usr/bin/env python3
"""
Scan for INA219 power monitor on all I2C buses and addresses
"""

import sys

try:
    from ina219 import INA219
except ImportError:
    print("ERROR: pi-ina219 library not installed")
    print("Install with: pip3 install pi-ina219")
    sys.exit(1)

# INA219 can be at these addresses (configured via solder jumpers)
POSSIBLE_ADDRESSES = [0x40, 0x41, 0x44, 0x45]
POSSIBLE_BUSES = [0, 1, 7, 8]  # Common I2C buses on Jetson

print("Scanning for INA219 power monitor...")
print("=" * 60)

found_devices = []

for bus in POSSIBLE_BUSES:
    for addr in POSSIBLE_ADDRESSES:
        try:
            # Try to initialize INA219 with minimal config
            ina = INA219(shunt_ohms=0.1, max_expected_amps=3.0, busnum=bus, address=addr)
            ina.configure(ina.RANGE_16V)
            
            # Try to read voltage (will fail if device doesn't exist)
            voltage = ina.voltage()
            
            print(f"✓ Found INA219 at bus {bus}, address 0x{addr:02X}")
            print(f"  Voltage: {voltage:.3f}V")
            found_devices.append((bus, addr, voltage))
            
        except Exception as e:
            # Device not found or error reading
            error_str = str(e)
            if "Device or resource busy" in error_str:
                print(f"⚠ Bus {bus}, addr 0x{addr:02X}: Device exists but busy (kernel driver)")
            # Silently skip other errors (device not present)
            pass

print("=" * 60)

if found_devices:
    print(f"\nFound {len(found_devices)} INA219 device(s):")
    for bus, addr, voltage in found_devices:
        print(f"  - Bus {bus}, Address 0x{addr:02X} (Voltage: {voltage:.3f}V)")
    print("\nUse these values in your monitoring script.")
else:
    print("\nNo INA219 devices found!")
    print("\nTroubleshooting:")
    print("1. Check wiring: SDA, SCL, VCC (3.3V), GND")
    print("2. Check I2C bus number (usually I2C_0 = bus 0, I2C_1 = bus 1)")
    print("3. Check address configuration (solder jumpers A0, A1):")
    print("   - No jumpers: 0x40 (default)")
    print("   - A0 bridged: 0x41")
    print("   - A1 bridged: 0x44")
    print("   - A0+A1 bridged: 0x45")
    print("4. Run 'i2cdetect -y -r 0' to see devices on bus 0")
    print("5. Check if device shows as 'UU' (used by kernel driver)")

sys.exit(0 if found_devices else 1)
