#!/usr/bin/env python3
"""
Jetson INA3221 Power Monitor Test Script  
Uses built-in INA3221 via hwmon sysfs (bus 1, addr 0x40)
Note: This monitors Jetson's input power, NOT external battery directly
For external battery monitoring, INA219 must be detected on I2C bus first

Tests voltage, current, and power monitoring
Exits on Ctrl+C or when voltage drops below critical threshold
"""

import sys
import time
import signal
import os

# Battery parameters (4S LiPo) - adjust if different
CELLS = 4
CRITICAL_VOLTAGE_PER_CELL = 3.65  # Volts per cell
CRITICAL_VOLTAGE_TOTAL = CRITICAL_VOLTAGE_PER_CELL * CELLS  # 14.6V
LOW_VOLTAGE_PER_CELL = 3.7  # Warning threshold
LOW_VOLTAGE_TOTAL = LOW_VOLTAGE_PER_CELL * CELLS  # 14.8V

# INA3221 hwmon path (Jetson Orin Nano)
HWMON_PATH = "/sys/devices/platform/bus@0/c240000.i2c/i2c-1/1-0040/hwmon/hwmon1"

# Global flag for graceful shutdown
shutdown_requested = False

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global shutdown_requested
    print("\n[POWER_MONITOR] Ctrl+C detected - shutting down gracefully...")
    shutdown_requested = True

def read_hwmon_value(filename):
    """Read value from hwmon sysfs file"""
    filepath = os.path.join(HWMON_PATH, filename)
    try:
        with open(filepath, 'r') as f:
            return float(f.read().strip())
    except (FileNotFoundError, ValueError, OSError) as e:
        print(f"[ERROR] Failed to read {filename}: {e}")
        return 0.0

def main():
    global shutdown_requested
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    print("=" * 90)
    print("JETSON INA3221 POWER MONITOR TEST")
    print("=" * 90)
    print(f"Sensor: INA3221 (built-in Jetson power monitor)")
    print(f"Location: {HWMON_PATH}")
    print(f"Battery Type: {CELLS}S LiPo")
    print(f"Critical Voltage: {CRITICAL_VOLTAGE_TOTAL}V ({CRITICAL_VOLTAGE_PER_CELL}V/cell)")
    print(f"Low Voltage Warning: {LOW_VOLTAGE_TOTAL}V ({LOW_VOLTAGE_PER_CELL}V/cell)")
    print()
    print("NOTE: INA3221 monitors Jetson input voltage (VDD_IN)")
    print("      For direct battery monitoring, connect external INA219 to I2C bus")
    print("=" * 90)
    print()
    
    # Check if hwmon path exists
    if not os.path.exists(HWMON_PATH):
        print(f"[ERROR] INA3221 hwmon path not found: {HWMON_PATH}")
        print("[ERROR] Make sure you're running on Jetson Orin Nano")
        return 1
    
    # Print available channels
    print("[INIT] Detecting INA3221 channels...")
    channels = []
    for in_num in [1, 2, 3]:
        label_file = os.path.join(HWMON_PATH, f"in{in_num}_label")
        if os.path.exists(label_file):
            with open(label_file, 'r') as f:
                label = f.read().strip()
                channels.append((in_num, label))
                print(f"  Channel {in_num}: {label}")
    
    if not channels:
        print("[ERROR] No INA3221 channels found!")
        return 1
    
    # Use channel 1 (VDD_IN) for monitoring
    channel = channels[0][0]
    channel_label = channels[0][1]
    print(f"\n[INIT] Using Channel {channel} ({channel_label}) for monitoring")
    print()
    
    # Print header
    print(f"{'Time':<12} {'Voltage':<20} {'Current':<12} {'Power':<12} {'Wh':<12} {'Status':<20}")
    print("-" * 90)
    
    # Tracking variables
    start_time = time.time()
    total_wh = 0.0  # Watt-hours consumed
    last_time = start_time
    sample_count = 0
    
    try:
        # Main monitoring loop
        while not shutdown_requested:
            try:
                current_time = time.time()
                elapsed = current_time - start_time
                delta_time = current_time - last_time
                
                # Read values from INA3221
                voltage = read_hwmon_value(f"in{channel}_input") / 1000.0  # mV to V
                current = read_hwmon_value(f"curr{channel}_input") / 1000.0  # mA to A
                power = voltage * current  # Calculate power in W
                
                # Calculate energy consumption (Wh)
                if delta_time > 0:
                    total_wh += (power * delta_time) / 3600.0
                
                # Calculate voltage per cell (assuming battery voltage = Jetson input)
                voltage_per_cell = voltage / CELLS
                
                # Determine status
                status = "OK"
                if voltage < CRITICAL_VOLTAGE_TOTAL:
                    status = "⚠️  CRITICAL LOW!"
                elif voltage < LOW_VOLTAGE_TOTAL:
                    status = "⚠️  Low Battery"
                
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
                
            except KeyboardInterrupt:
                break
                
            except Exception as e:
                print(f"[ERROR] Reading failed: {e}")
                time.sleep(1.0)
                continue
    
    finally:
        # Print summary
        print()
        print("=" * 90)
        print("MONITORING SUMMARY")
        print("=" * 90)
        if sample_count > 0:
            elapsed_total = time.time() - start_time
            print(f"Total monitoring time: {elapsed_total:.1f}s")
            print(f"Total samples: {sample_count}")
            print(f"Total energy consumed: {total_wh:.4f}Wh")
            print(f"Average sample rate: {sample_count / elapsed_total:.2f} samples/sec")
            
            # Read final values
            try:
                final_voltage = read_hwmon_value(f"in{channel}_input") / 1000.0
                final_current = read_hwmon_value(f"curr{channel}_input") / 1000.0
                final_power = final_voltage * final_current
                print(f"Final voltage: {final_voltage:.3f}V ({final_voltage / CELLS:.3f}V/cell)")
                print(f"Final current: {final_current:.3f}A")
                print(f"Final power: {final_power:.3f}W")
            except Exception:
                pass
        
        print("=" * 90)
        print("[POWER_MONITOR] Test complete")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
