#!/usr/bin/env python3
"""
Battery Monitor for 4S LiPo Battery
Monitors voltage, current, and power via INA219 on I2C bus 7
Terminates automatically when cell voltage drops below 3.65V (critical voltage)
"""

import time
import signal
import sys
from ina219 import INA219
from datetime import datetime

# Battery configuration
NUM_CELLS = 4  # 4S LiPo battery
CRITICAL_VOLTAGE_PER_CELL = 3.65  # Minimum safe voltage per cell (V)
CRITICAL_TOTAL_VOLTAGE = CRITICAL_VOLTAGE_PER_CELL * NUM_CELLS  # 14.6V for 4S

# INA219 configuration
I2C_BUS = 7
I2C_ADDRESS = 0x40
SHUNT_OHMS = 0.1  # Default shunt resistor value
MAX_EXPECTED_AMPS = 3.0  # Maximum expected current

# Global flag for graceful shutdown
running = True

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n\n⚠️  Ctrl+C detected - shutting down gracefully...")
    running = False

def format_duration(seconds):
    """Format duration in human-readable format"""
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = int(seconds % 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"

def main():
    global running
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    print("=" * 70)
    print("  4S LiPo Battery Monitor")
    print("=" * 70)
    print(f"  Critical voltage per cell: {CRITICAL_VOLTAGE_PER_CELL}V")
    print(f"  Critical total voltage: {CRITICAL_TOTAL_VOLTAGE}V")
    print(f"  I2C Bus: {I2C_BUS}, Address: 0x{I2C_ADDRESS:02X}")
    print("=" * 70)
    print()
    
    try:
        # Initialize INA219
        print("Initializing INA219...")
        ina = INA219(shunt_ohms=SHUNT_OHMS, 
                     max_expected_amps=MAX_EXPECTED_AMPS,
                     busnum=I2C_BUS, 
                     address=I2C_ADDRESS)
        ina.configure(ina.RANGE_16V)
        print("✓ INA219 initialized successfully")
        print()
        
        # Print header
        print(f"{'Time':<12} {'Voltage':<10} {'Cell V':<10} {'Current':<12} {'Power':<12} {'Status':<20}")
        print("-" * 80)
        
        start_time = time.time()
        sample_count = 0
        
        while running:
            try:
                # Read sensor values
                voltage = ina.voltage()  # Total battery voltage (V)
                current = ina.current()  # Current in mA
                power = ina.power()  # Power in mW
                
                # Calculate per-cell voltage
                cell_voltage = voltage / NUM_CELLS
                
                # Format current time
                elapsed = time.time() - start_time
                time_str = format_duration(elapsed)
                
                # Determine status
                if cell_voltage < CRITICAL_VOLTAGE_PER_CELL:
                    status = "🔴 CRITICAL - SHUTDOWN"
                    status_color = "\033[91m"  # Red
                elif cell_voltage < 3.70:
                    status = "⚠️  LOW VOLTAGE"
                    status_color = "\033[93m"  # Yellow
                else:
                    status = "✓ OK"
                    status_color = "\033[92m"  # Green
                
                # Print readings
                print(f"{time_str:<12} "
                      f"{voltage:>6.3f}V   "
                      f"{cell_voltage:>6.3f}V   "
                      f"{current/1000:>7.3f}A    "
                      f"{power/1000:>7.3f}W    "
                      f"{status_color}{status}\033[0m")
                
                sample_count += 1
                
                # Check for critical voltage
                if cell_voltage < CRITICAL_VOLTAGE_PER_CELL:
                    print()
                    print("=" * 80)
                    print(f"🚨 CRITICAL BATTERY VOLTAGE DETECTED!")
                    print(f"   Total voltage: {voltage:.3f}V")
                    print(f"   Cell voltage: {cell_voltage:.3f}V (below {CRITICAL_VOLTAGE_PER_CELL}V threshold)")
                    print(f"   Terminating to protect battery...")
                    print("=" * 80)
                    break
                
                # Wait 1 second before next reading
                time.sleep(1.0)
                
            except OSError as e:
                print(f"\n⚠️  I2C communication error: {e}")
                print("   Retrying in 1 second...")
                time.sleep(1.0)
                continue
        
        # Print summary
        print()
        print("=" * 80)
        print("  Monitoring Session Summary")
        print("=" * 80)
        total_duration = time.time() - start_time
        print(f"  Duration: {format_duration(total_duration)}")
        print(f"  Samples collected: {sample_count}")
        print(f"  Final voltage: {voltage:.3f}V ({cell_voltage:.3f}V per cell)")
        print(f"  Final current: {current/1000:.3f}A")
        print(f"  Final power: {power/1000:.3f}W")
        print("=" * 80)
        
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
