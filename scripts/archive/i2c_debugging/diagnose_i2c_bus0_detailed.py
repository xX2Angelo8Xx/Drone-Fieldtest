#!/usr/bin/env python3
"""
I2C Bus 0 Hardware Diagnostic Tool (Python Version)
Provides additional diagnostic capabilities using Linux sysfs interface

Usage: sudo python3 diagnose_i2c_bus0_detailed.py
"""

import os
import sys
import time
import subprocess

def print_header(text):
    """Print formatted header"""
    print("\n" + "=" * 70)
    print(text)
    print("=" * 70)

def print_section(text):
    """Print formatted section"""
    print(f"\n[{text}]")

def run_command(cmd, description):
    """Run shell command and return output"""
    print(f"  Running: {description}")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=5)
        return result.stdout.strip(), result.returncode
    except subprocess.TimeoutExpired:
        return "TIMEOUT", -1
    except Exception as e:
        return f"ERROR: {str(e)}", -1

def check_i2c_bus_exists(bus_num):
    """Check if I2C bus device node exists"""
    device_path = f"/dev/i2c-{bus_num}"
    exists = os.path.exists(device_path)
    
    if exists:
        print(f"  ✓ Device node {device_path} exists")
        
        # Check permissions
        stat_info = os.stat(device_path)
        print(f"    Permissions: {oct(stat_info.st_mode)[-3:]}")
        print(f"    Owner: UID {stat_info.st_uid}, GID {stat_info.st_gid}")
    else:
        print(f"  ✗ Device node {device_path} does NOT exist")
    
    return exists

def check_kernel_modules():
    """Check if required I2C kernel modules are loaded"""
    print_section("Checking Kernel Modules")
    
    required_modules = ['i2c_dev', 'i2c_tegra']
    
    output, _ = run_command("lsmod | grep i2c", "Listing I2C modules")
    
    for module in required_modules:
        if module in output:
            print(f"  ✓ Module '{module}' is loaded")
        else:
            print(f"  ✗ Module '{module}' is NOT loaded")
            print(f"    Try: sudo modprobe {module}")

def check_sysfs_bus_info(bus_num):
    """Check sysfs information for I2C bus"""
    print_section(f"Checking sysfs Info for Bus {bus_num}")
    
    sysfs_path = f"/sys/class/i2c-adapter/i2c-{bus_num}"
    
    if not os.path.exists(sysfs_path):
        print(f"  ✗ sysfs path {sysfs_path} does not exist")
        return
    
    print(f"  ✓ sysfs path exists: {sysfs_path}")
    
    # Read name
    name_file = os.path.join(sysfs_path, "name")
    if os.path.exists(name_file):
        with open(name_file, 'r') as f:
            name = f.read().strip()
            print(f"    Name: {name}")
    
    # List devices
    device_path = os.path.join(sysfs_path, "device")
    if os.path.exists(device_path):
        print(f"    Device link: {os.readlink(device_path) if os.path.islink(device_path) else 'N/A'}")

def i2c_detect_scan(bus_num):
    """Use i2cdetect to scan bus"""
    print_section(f"Scanning Bus {bus_num} with i2cdetect")
    
    # Check if i2cdetect is available
    which_result = subprocess.run("which i2cdetect", shell=True, capture_output=True)
    if which_result.returncode != 0:
        print("  ✗ i2cdetect not found. Install with: sudo apt install i2c-tools")
        return
    
    # Run i2cdetect
    cmd = f"i2cdetect -y {bus_num}"
    output, returncode = run_command(cmd, f"i2cdetect -y {bus_num}")
    
    if returncode == 0:
        print(output)
    else:
        print(f"  ✗ i2cdetect failed")

def gpio_pinmux_check():
    """Check GPIO pin multiplexing configuration"""
    print_section("Checking GPIO Pin Multiplexing")
    
    # Check for jetson-io (pin configuration tool)
    if os.path.exists("/opt/nvidia/jetson-io/jetson-io.py"):
        print("  ✓ jetson-io tool is available")
        print("    Run 'sudo /opt/nvidia/jetson-io/jetson-io.py' to check pin config")
    else:
        print("  ⚠ jetson-io tool not found")
    
    # Try to find device tree info
    dt_path = "/proc/device-tree/i2c@c240000"  # Common I2C0 path on Jetson
    if os.path.exists(dt_path):
        print(f"  ✓ Device tree node exists: {dt_path}")
        
        status_file = os.path.join(dt_path, "status")
        if os.path.exists(status_file):
            with open(status_file, 'r') as f:
                status = f.read().strip('\x00')
                print(f"    Status: {status}")
                if status == "okay":
                    print("    ✓ I2C controller is enabled in device tree")
                else:
                    print("    ✗ I2C controller may be disabled!")
    else:
        print(f"  ⚠ Device tree node not found at {dt_path}")

def hardware_measurements():
    """Provide instructions for hardware measurements"""
    print_section("Hardware Measurement Instructions")
    
    print("  To diagnose physical issues, measure with a multimeter:")
    print()
    print("  1. POWER (3.3V) on I2C_IDA_0:")
    print("     - Pin 1 (or marked VCC): Should read ~3.3V")
    print("     - Pin 2 (GND): Should be 0V")
    print()
    print("  2. PULL-UP VOLTAGE (idle state):")
    print("     - SDA pin: Should read ~3.3V when no device connected")
    print("     - SCL pin: Should read ~3.3V when no device connected")
    print("     - If reading 0V or ~1.5V, pull-up resistors may be missing/damaged")
    print()
    print("  3. CONTINUITY TEST:")
    print("     - With power OFF, test continuity between:")
    print("       * I2C_IDA_0 GND ↔ System GND (should be continuous)")
    print("       * I2C_IDA_0 SDA ↔ I2C_IDA_1 SDA (should be isolated/open)")
    print()
    print("  4. SIGNAL OBSERVATION (requires oscilloscope/logic analyzer):")
    print("     - Connect LCD to Bus 0")
    print("     - Run ./simple_lcd_test while monitoring SDA/SCL")
    print("     - Should see clock pulses (~100kHz) and data transitions")
    print("     - If flat line, bus controller may be inactive")

def compare_working_vs_broken():
    """Compare working Bus 7 with problematic Bus 0"""
    print_header("COMPARISON: Bus 7 (Working) vs Bus 0 (Problem)")
    
    for bus in [7, 0]:
        label = "WORKING" if bus == 7 else "PROBLEM"
        print(f"\n--- Bus {bus} ({label}) ---")
        
        check_i2c_bus_exists(bus)
        check_sysfs_bus_info(bus)
        i2c_detect_scan(bus)

def test_with_known_device():
    """Instructions for testing with LCD or INA219"""
    print_section("Testing with Known Working Device")
    
    print("  STEP 1: Connect your INA219 (verified working on Bus 7) to Bus 0")
    print("    - I2C_IDA_0 Pin layout:")
    print("      Pin 1: 3.3V  (usually marked or leftmost)")
    print("      Pin 2: SDA")
    print("      Pin 3: SCL")
    print("      Pin 4: GND")
    print()
    print("  STEP 2: Run scan:")
    print("    sudo i2cdetect -y 0")
    print()
    print("  STEP 3: Expected result:")
    print("    - If INA219 appears at 0x40: Bus 0 hardware is OK!")
    print("    - If nothing appears: Bus 0 hardware issue confirmed")
    print()
    print("  STEP 4: Try LCD on Bus 0:")
    print("    - Connect LCD to Bus 0 (address 0x27 or 0x3F)")
    print("    - Run: sudo ./simple_lcd_test")
    print("      (You may need to modify the bus number in the code)")

def suggest_workarounds():
    """Suggest workarounds if Bus 0 is broken"""
    print_section("Workarounds if Bus 0 is Broken")
    
    print("  Option 1: I2C Multiplexer/Expander")
    print("    - Use TCA9548A I2C multiplexer on Bus 7")
    print("    - Provides 8 additional I2C channels")
    print("    - Cost: ~$5-10")
    print()
    print("  Option 2: Use Both Devices on Bus 7")
    print("    - LCD at 0x27")
    print("    - INA219 at 0x40")
    print("    - Different addresses = no conflict")
    print("    - This is your current best option!")
    print()
    print("  Option 3: Software I2C (bit-banging)")
    print("    - Use GPIO pins to emulate I2C")
    print("    - Slower but functional")
    print("    - Requires kernel module: i2c-gpio")

def main():
    if os.geteuid() != 0:
        print("⚠ Warning: This script should be run with sudo for full access")
        print("Some tests may fail without root privileges\n")
    
    print_header("I2C BUS 0 DETAILED HARDWARE DIAGNOSTIC")
    print("Jetson Orin Nano - I2C_IDA_0 Troubleshooting")
    print("\nThis tool performs comprehensive I2C Bus 0 diagnostics")
    
    # Run all diagnostic checks
    print_section("System Information")
    run_command("uname -a", "Kernel version")
    run_command("cat /etc/nv_tegra_release", "Jetson release info")
    
    check_kernel_modules()
    
    print_header("INDIVIDUAL BUS TESTS")
    
    print("\n--- Testing Bus 0 (Problem) ---")
    check_i2c_bus_exists(0)
    check_sysfs_bus_info(0)
    i2c_detect_scan(0)
    
    print("\n--- Testing Bus 7 (Working) ---")
    check_i2c_bus_exists(7)
    check_sysfs_bus_info(7)
    i2c_detect_scan(7)
    
    gpio_pinmux_check()
    hardware_measurements()
    test_with_known_device()
    suggest_workarounds()
    
    print_header("DIAGNOSTIC COMPLETE")
    print("\nNext steps:")
    print("1. Review the output above for errors")
    print("2. Perform hardware measurements with multimeter")
    print("3. If Bus 0 is confirmed broken, use workaround (both devices on Bus 7)")
    print("4. Consider I2C multiplexer for future expansion")
    
if __name__ == "__main__":
    main()
