#!/usr/bin/env python3
"""
INA219 Voltage Calibration Tool
Compares INA219 reading with reference voltage (multimeter/power supply)
Calculates and applies calibration offset
"""

import sys
import time

def main():
    print("=" * 70)
    print("  INA219 Voltage Calibration Tool")
    print("=" * 70)
    print()
    print("This tool helps you calibrate the INA219 voltage reading.")
    print()
    print("You will need:")
    print("  1. A reliable voltage reference (multimeter or calibrated power supply)")
    print("  2. The INA219 connected to your battery/power source")
    print()
    print("Steps:")
    print("  1. Measure the actual voltage with your reference device")
    print("  2. Compare with INA219 reading")
    print("  3. Tool calculates the calibration offset")
    print("=" * 70)
    print()
    
    # Read current INA219 value
    try:
        import sys
        sys.path.insert(0, '/home/angelo/Projects/Drone-Fieldtest/.venv/lib/python3.10/site-packages')
        from ina219 import INA219
        
        ina = INA219(shunt_ohms=0.1, max_expected_amps=3.0, busnum=7, address=0x40)
        ina.configure(ina.RANGE_16V)
        
        print("Reading INA219 (taking 5 samples for accuracy)...")
        samples = []
        for i in range(5):
            voltage = ina.voltage()
            samples.append(voltage)
            print(f"  Sample {i+1}: {voltage:.3f}V")
            time.sleep(0.2)
        
        ina219_voltage = sum(samples) / len(samples)
        print()
        print(f"✓ Average INA219 reading: {ina219_voltage:.3f}V")
        print()
        
    except Exception as e:
        print(f"❌ Error reading INA219: {e}")
        print()
        print("Using manual input mode...")
        ina219_voltage = None
    
    # Get reference voltage from user
    print("What is the ACTUAL voltage measured with your reference device?")
    print("(Enter voltage in V, e.g., 16.0 for 16V)")
    print()
    
    while True:
        try:
            reference_voltage_str = input("Reference voltage (V): ").strip()
            reference_voltage = float(reference_voltage_str)
            
            if reference_voltage < 0 or reference_voltage > 32:
                print("⚠️  Voltage out of range (0-32V). Please try again.")
                continue
            
            break
        except ValueError:
            print("⚠️  Invalid input. Please enter a number (e.g., 16.0)")
        except KeyboardInterrupt:
            print("\n\nCalibration cancelled.")
            return 1
    
    print()
    
    # If we couldn't read INA219, ask for its value
    if ina219_voltage is None:
        print("What voltage does the INA219 currently show?")
        while True:
            try:
                ina219_voltage_str = input("INA219 reading (V): ").strip()
                ina219_voltage = float(ina219_voltage_str)
                break
            except ValueError:
                print("⚠️  Invalid input. Please enter a number (e.g., 15.8)")
            except KeyboardInterrupt:
                print("\n\nCalibration cancelled.")
                return 1
        print()
    
    # Calculate offset
    offset = reference_voltage - ina219_voltage
    error_percent = (abs(offset) / reference_voltage) * 100
    
    print("=" * 70)
    print("  Calibration Results")
    print("=" * 70)
    print(f"  Reference voltage (actual):  {reference_voltage:.3f}V")
    print(f"  INA219 reading (measured):   {ina219_voltage:.3f}V")
    print(f"  Difference:                  {offset:+.3f}V ({error_percent:.2f}% error)")
    print()
    print(f"  ➜ Calibration offset needed: {offset:+.3f}V")
    print("=" * 70)
    print()
    
    # Provide instructions
    print("How to apply this calibration:")
    print()
    print("Option 1: Via C++ code (drone_web_controller initialization)")
    print("-" * 70)
    print("Add this line after battery_monitor_->initialize():")
    print()
    print(f"    battery_monitor_->setVoltageCalibration({offset:.3f}f);")
    print()
    print()
    print("Option 2: Test with Python script")
    print("-" * 70)
    print("Modify battery_monitor.py and add after ina.configure():")
    print()
    print(f"    # Apply calibration offset")
    print(f"    calibration_offset = {offset:.3f}")
    print(f"    voltage = ina.voltage() + calibration_offset")
    print()
    print()
    print("After applying calibration, INA219 should read: {:.3f}V".format(reference_voltage))
    print()
    
    # Verify calculation
    print("Verification:")
    print(f"  {ina219_voltage:.3f}V (raw) + {offset:+.3f}V (offset) = {ina219_voltage + offset:.3f}V (calibrated)")
    print()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
