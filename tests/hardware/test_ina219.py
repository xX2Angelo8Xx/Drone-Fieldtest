#!/usr/bin/env python3
"""
INA219 Power Monitor Test Script
Tests voltage, current, and power monitoring for 4S LiPo battery (14.6V nominal)
Exits on Ctrl+C or when voltage drops below critical threshold (3.65V * 4 = 14.6V)
"""

import sys
import time
import signal
from ina219 import INA219, DeviceRangeError

# Configuration
I2C_BUS = 1
I2C_ADDRESS = 0x40
SHUNT_OHMS = 0.1  # 100mOhm shunt resistor (adjust if different)
MAX_EXPECTED_AMPS = 3.0  # Maximum expected current draw

# Battery parameters (4S LiPo)
CELLS = 4
CRITICAL_VOLTAGE_PER_CELL = 3.65  # Volts per cell
CRITICAL_VOLTAGE_TOTAL = CRITICAL_VOLTAGE_PER_CELL * CELLS  # 14.6V
LOW_VOLTAGE_PER_CELL = 3.7  # Warning threshold
LOW_VOLTAGE_TOTAL = LOW_VOLTAGE_PER_CELL * CELLS  # 14.8V

# Global flag for graceful shutdown
shutdown_requested = False

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global shutdown_requested
    print("\n[INA219] Ctrl+C detected - shutting down gracefully...")
    shutdown_requested = True

def main():
    global shutdown_requested
    
    # Register signal handler for Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)
    
    print("=" * 60)
    print("INA219 POWER MONITOR TEST")
    print("=" * 60)
    print(f"I2C Bus: {I2C_BUS}")
    print(f"I2C Address: 0x{I2C_ADDRESS:02X}")
    print(f"Shunt Resistor: {SHUNT_OHMS}Ω")
    print(f"Max Expected Current: {MAX_EXPECTED_AMPS}A")
    print(f"Battery Type: {CELLS}S LiPo")
    print(f"Critical Voltage: {CRITICAL_VOLTAGE_TOTAL}V ({CRITICAL_VOLTAGE_PER_CELL}V/cell)")
    print(f"Low Voltage Warning: {LOW_VOLTAGE_TOTAL}V ({LOW_VOLTAGE_PER_CELL}V/cell)")
    print("=" * 60)
    print()
    
    try:
        # Initialize INA219
        print("[INA219] Initializing power monitor...")
        ina = INA219(SHUNT_OHMS, MAX_EXPECTED_AMPS, busnum=I2C_BUS, address=I2C_ADDRESS)
        ina.configure(ina.RANGE_16V)  # 16V range for 4S LiPo
        print("[INA219] ✓ Initialized successfully")
        print()
        
        # Print header
        print(f"{'Time':<12} {'Voltage':<12} {'Current':<12} {'Power':<12} {'Wh':<12} {'Status':<20}")
        print("-" * 80)
        
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
                
                # Read values from INA219
                voltage = ina.voltage()  # Bus voltage in V
                current = ina.current() / 1000.0  # Current in A (library returns mA)
                power = ina.power() / 1000.0  # Power in W (library returns mW)
                
                # Calculate energy consumption (Wh)
                # Energy = Power * Time (in hours)
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
                
                print(f"{time_str:<12} {voltage_str:<12} {current_str:<12} {power_str:<12} {wh_str:<12} {status:<20}")
                
                # Check for critical voltage
                if voltage < CRITICAL_VOLTAGE_TOTAL:
                    print()
                    print("=" * 80)
                    print(f"⚠️  CRITICAL VOLTAGE DETECTED: {voltage:.3f}V < {CRITICAL_VOLTAGE_TOTAL}V")
                    print(f"Battery voltage per cell: {voltage_per_cell:.3f}V (critical: {CRITICAL_VOLTAGE_PER_CELL}V)")
                    print("Initiating emergency shutdown to protect battery...")
                    print("=" * 80)
                    shutdown_requested = True
                    break
                
                sample_count += 1
                last_time = current_time
                
                # Wait 1 second before next reading
                time.sleep(1.0)
                
            except DeviceRangeError as e:
                print(f"[INA219] ⚠️ Device range error: {e}")
                print("[INA219] Current or voltage exceeds configured range!")
                time.sleep(1.0)
                continue
                
    except FileNotFoundError:
        print()
        print("[INA219] ❌ ERROR: INA219 device not found!")
        print(f"[INA219] Make sure device is connected to I2C bus {I2C_BUS} at address 0x{I2C_ADDRESS:02X}")
        print("[INA219] Run 'i2cdetect -y -r 1' to verify connection")
        return 1
        
    except OSError as e:
        print()
        print(f"[INA219] ❌ ERROR: I2C communication failed: {e}")
        print("[INA219] Check wiring and I2C bus configuration")
        return 1
        
    except KeyboardInterrupt:
        # Shouldn't reach here due to signal handler, but just in case
        print("\n[INA219] Interrupted by user")
        
    except Exception as e:
        print()
        print(f"[INA219] ❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    finally:
        # Print summary
        print()
        print("=" * 80)
        print("MONITORING SUMMARY")
        print("=" * 80)
        if sample_count > 0:
            elapsed_total = time.time() - start_time
            print(f"Total monitoring time: {elapsed_total:.1f}s")
            print(f"Total samples: {sample_count}")
            print(f"Total energy consumed: {total_wh:.4f}Wh")
            print(f"Average sample rate: {sample_count / elapsed_total:.2f} samples/sec")
            
            # Read final values
            try:
                final_voltage = ina.voltage()
                final_current = ina.current() / 1000.0
                final_power = ina.power() / 1000.0
                print(f"Final voltage: {final_voltage:.3f}V ({final_voltage / CELLS:.3f}V/cell)")
                print(f"Final current: {final_current:.3f}A")
                print(f"Final power: {final_power:.3f}W")
            except Exception:
                pass  # INA219 might be disconnected during shutdown
        
        print("=" * 80)
        print("[INA219] Test complete")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
