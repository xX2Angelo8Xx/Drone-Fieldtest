#!/usr/bin/env python3
"""
Battery Power Monitor Test Script
Supports both INA219 (via I2C) and INA3221 (via hwmon sysfs)
Tests voltage, current, and power monitoring for 4S LiPo battery
Exits on Ctrl+C or when voltage drops below critical threshold (3.65V * 4 = 14.6V)
"""

import sys
import time
import signal
import os
import glob

# Configuration
# Try INA219 first, fallback to INA3221 hwmon
INA219_BUS = 1
INA219_ADDRESS = 0x41  # Try 0x41 (alternate address), 0x40 is taken by INA3221
SHUNT_OHMS = 0.1  # 100mOhm shunt resistor
MAX_EXPECTED_AMPS = 5.0  # Maximum expected current draw

# Battery parameters (4S LiPo)
CELLS = 4
CRITICAL_VOLTAGE_PER_CELL = 3.65  # Volts per cell
CRITICAL_VOLTAGE_TOTAL = CRITICAL_VOLTAGE_PER_CELL * CELLS  # 14.6V
LOW_VOLTAGE_PER_CELL = 3.7  # Warning threshold
LOW_VOLTAGE_TOTAL = LOW_VOLTAGE_PER_CELL * CELLS  # 14.8V

# Global flag for graceful shutdown
shutdown_requested = False
power_monitor = None

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global shutdown_requested
    print("\n[POWER_MONITOR] Ctrl+C detected - shutting down gracefully...")
    shutdown_requested = True

class INA219Monitor:
    """Power monitoring via INA219 I2C sensor"""
    
    def __init__(self, bus, address, shunt_ohms, max_amps):
        from ina219 import INA219, DeviceRangeError
        self.DeviceRangeError = DeviceRangeError
        self.ina = INA219(shunt_ohms, max_amps, busnum=bus, address=address)
        self.ina.configure(self.ina.RANGE_32V)  # 32V range for flexibility
        self.name = f"INA219 (I2C bus {bus}, addr 0x{address:02X})"
        
    def read_voltage(self):
        """Return voltage in volts"""
        return self.ina.voltage()
    
    def read_current(self):
        """Return current in amps"""
        return self.ina.current() / 1000.0  # Convert mA to A
        
    def read_power(self):
        """Return power in watts"""
        return self.ina.power() / 1000.0  # Convert mW to W

class INA3221Monitor:
    """Power monitoring via INA3221 hwmon sysfs"""
    
    def __init__(self, hwmon_path):
        self.hwmon_path = hwmon_path
        self.name = f"INA3221 (hwmon: {os.path.basename(hwmon_path)})"
        
        # Find voltage and current files
        self.voltage_file = None
        self.current_file = None
        
        # Look for battery-related channel (usually in1)
        for in_num in [1, 2, 3]:
            label_file = os.path.join(hwmon_path, f"in{in_num}_label")
            if os.path.exists(label_file):
                with open(label_file, 'r') as f:
                    label = f.read().strip()
                    print(f"[INA3221] Channel {in_num}: {label}")
                    # Use VDD_IN or first available channel
                    if "VDD_IN" in label or self.voltage_file is None:
                        self.voltage_file = os.path.join(hwmon_path, f"in{in_num}_input")
                        self.current_file = os.path.join(hwmon_path, f"curr{in_num}_input")
                        print(f"[INA3221] Using channel {in_num} ({label}) for monitoring")
        
        if not self.voltage_file:
            raise RuntimeError("No suitable INA3221 channel found")
            
    def read_voltage(self):
        """Return voltage in volts"""
        with open(self.voltage_file, 'r') as f:
            return float(f.read().strip()) / 1000.0  # Convert mV to V
    
    def read_current(self):
        """Return current in amps"""
        if not os.path.exists(self.current_file):
            return 0.0  # Current not available on this channel
        with open(self.current_file, 'r') as f:
            return float(f.read().strip()) / 1000.0  # Convert mA to A
            
    def read_power(self):
        """Return power in watts"""
        return self.read_voltage() * self.read_current()

def detect_power_monitor():
    """Detect available power monitoring hardware"""
    
    # Try INA219 first
    try:
        from ina219 import INA219
        print(f"[DETECT] Trying INA219 on I2C bus {INA219_BUS}, address 0x{INA219_ADDRESS:02X}...")
        monitor = INA219Monitor(INA219_BUS, INA219_ADDRESS, SHUNT_OHMS, MAX_EXPECTED_AMPS)
        # Test read
        monitor.read_voltage()
        print(f"[DETECT] ✓ Found {monitor.name}")
        return monitor
    except Exception as e:
        print(f"[DETECT] INA219 not available: {e}")
    
    # Try INA3221 via hwmon
    try:
        # Find INA3221 hwmon path
        import glob
        hwmon_paths = glob.glob("/sys/devices/platform/*/i2c-*/1-0040/hwmon/hwmon*")
        if not hwmon_paths:
            hwmon_paths = glob.glob("/sys/devices/**/1-0040/hwmon/hwmon*", recursive=True)
        
        for hwmon_path in hwmon_paths:
            print(f"[DETECT] Trying INA3221 at {hwmon_path}...")
            monitor = INA3221Monitor(hwmon_path)
            # Test read
            monitor.read_voltage()
            print(f"[DETECT] ✓ Found {monitor.name}")
            return monitor
    except Exception as e:
        print(f"[DETECT] INA3221 not available: {e}")
    
    raise RuntimeError("No power monitoring hardware detected!")

def main():
    global shutdown_requested, power_monitor
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    print("=" * 80)
    print("BATTERY POWER MONITOR TEST")
    print("=" * 80)
    print(f"Battery Type: {CELLS}S LiPo")
    print(f"Critical Voltage: {CRITICAL_VOLTAGE_TOTAL}V ({CRITICAL_VOLTAGE_PER_CELL}V/cell)")
    print(f"Low Voltage Warning: {LOW_VOLTAGE_TOTAL}V ({LOW_VOLTAGE_PER_CELL}V/cell)")
    print("=" * 80)
    print()
    
    try:
        # Detect and initialize power monitor
        print("[INIT] Detecting power monitoring hardware...")
        power_monitor = detect_power_monitor()
        print(f"[INIT] ✓ Using: {power_monitor.name}")
        print()
        
        # Print header
        print(f"{'Time':<12} {'Voltage':<20} {'Current':<12} {'Power':<12} {'Wh':<12} {'Status':<20}")
        print("-" * 90)
        
        # Tracking variables
        start_time = time.time()
        total_wh = 0.0  # Watt-hours consumed
        last_time = start_time
        sample_count = 0
        
        # Main monitoring loop
        while not shutdown_requested:
            try:
                current_time = time.time()
                elapsed = current_time - start_time
                delta_time = current_time - last_time
                
                # Read values from power monitor
                voltage = power_monitor.read_voltage()
                current = power_monitor.read_current()
                power = power_monitor.read_power()
                
                # Calculate energy consumption (Wh)
                if delta_time > 0:
                    total_wh += (power * delta_time) / 3600.0
                
                # Calculate voltage per cell
                voltage_per_cell = voltage / CELLS
                
                # Determine status
                status = "OK"
                if voltage < CRITICAL_VOLTAGE_TOTAL:
                    status = "⚠️ CRITICAL LOW!"
                elif voltage < LOW_VOLTAGE_TOTAL:
                    status = "⚠️ Low Battery"
                
                # Format output
                time_str = f"{elapsed:.1f}s"
                voltage_str = f"{voltage:.3f}V ({voltage_per_cell:.3f}V/cell)"
                current_str = f"{current:.3f}A"
                power_str = f"{power:.3f}W"
                wh_str = f"{total_wh:.4f}Wh"
                
                print(f"{time_str:<12} {voltage_str:<20} {current_str:<12} {power_str:<12} {wh_str:<12} {status:<20}")
                
                # Check for critical voltage
                if voltage < CRITICAL_VOLTAGE_TOTAL:
                    print()
                    print("=" * 90)
                    print(f"⚠️  CRITICAL VOLTAGE DETECTED: {voltage:.3f}V < {CRITICAL_VOLTAGE_TOTAL}V")
                    print(f"Battery voltage per cell: {voltage_per_cell:.3f}V (critical: {CRITICAL_VOLTAGE_PER_CELL}V)")
                    print("Initiating emergency shutdown to protect battery...")
                    print("=" * 90)
                    shutdown_requested = True
                    break
                
                sample_count += 1
                last_time = current_time
                
                # Wait 1 second before next reading
                time.sleep(1.0)
                
            except Exception as e:
                print(f"[ERROR] Reading failed: {e}")
                time.sleep(1.0)
                continue
                
    except KeyboardInterrupt:
        print("\n[POWER_MONITOR] Interrupted by user")
        
    except Exception as e:
        print()
        print(f"[POWER_MONITOR] ❌ Fatal error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    finally:
        # Print summary
        print()
        print("=" * 90)
        print("MONITORING SUMMARY")
        print("=" * 90)
        if 'sample_count' in locals() and sample_count > 0:
            elapsed_total = time.time() - start_time
            print(f"Total monitoring time: {elapsed_total:.1f}s")
            print(f"Total samples: {sample_count}")
            print(f"Total energy consumed: {total_wh:.4f}Wh")
            print(f"Average sample rate: {sample_count / elapsed_total:.2f} samples/sec")
            
            # Read final values
            try:
                if power_monitor:
                    final_voltage = power_monitor.read_voltage()
                    final_current = power_monitor.read_current()
                    final_power = power_monitor.read_power()
                    print(f"Final voltage: {final_voltage:.3f}V ({final_voltage / CELLS:.3f}V/cell)")
                    print(f"Final current: {final_current:.3f}A")
                    print(f"Final power: {final_power:.3f}W")
            except Exception:
                pass  # Monitor might be disconnected during shutdown
        
        print("=" * 90)
        print("[POWER_MONITOR] Test complete")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
