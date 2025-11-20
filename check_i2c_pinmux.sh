#!/bin/bash
# Check I2C Pin Configuration and Device Tree Status
# Verifies GPIO pin mapping and I2C controller status

echo "========================================================================="
echo "I2C PIN CONFIGURATION CHECK"
echo "Jetson Orin Nano - GPIO Pin Mapping Verification"
echo "========================================================================="
echo ""

if [ "$EUID" -ne 0 ]; then 
    echo "⚠ Warning: Some checks require sudo"
    echo "Run with: sudo ./check_i2c_pinmux.sh"
    echo ""
fi

# Expected configuration
echo "EXPECTED PIN MAPPING:"
echo "--------------------"
echo "I2C_IDA_0 (Bus 0, /dev/i2c-0):"
echo "  - GPIO Pin 27 = SDA_0"
echo "  - GPIO Pin 28 = SCL_0"
echo ""
echo "I2C_IDA_1 (Bus 7, /dev/i2c-7):"
echo "  - GPIO Pin 29 = SDA_1"
echo "  - GPIO Pin 30 = SCL_1"
echo ""

# Check if I2C devices exist
echo "========================================================================="
echo "I2C DEVICE NODES CHECK"
echo "========================================================================="
echo ""

for bus in 0 7; do
    if [ -e "/dev/i2c-$bus" ]; then
        echo "✓ /dev/i2c-$bus exists"
        ls -la "/dev/i2c-$bus"
    else
        echo "✗ /dev/i2c-$bus MISSING!"
    fi
done
echo ""

# Check sysfs I2C adapter info
echo "========================================================================="
echo "I2C ADAPTER INFORMATION"
echo "========================================================================="
echo ""

for bus in 0 7; do
    echo "--- Bus $bus ---"
    adapter_path="/sys/class/i2c-adapter/i2c-$bus"
    
    if [ -d "$adapter_path" ]; then
        echo "Adapter path: $adapter_path"
        
        if [ -f "$adapter_path/name" ]; then
            name=$(cat "$adapter_path/name")
            echo "Name: $name"
        fi
        
        if [ -L "$adapter_path/device" ]; then
            device_link=$(readlink "$adapter_path/device")
            echo "Device: $device_link"
        fi
    else
        echo "⚠ Adapter not found!"
    fi
    echo ""
done

# Check Device Tree status
echo "========================================================================="
echo "DEVICE TREE STATUS"
echo "========================================================================="
echo ""

# Bus 0 (I2C@3160000)
echo "--- Bus 0 (I2C@3160000) ---"
dt_path_bus0="/proc/device-tree/i2c@3160000"
if [ -d "$dt_path_bus0" ]; then
    echo "Device Tree node: EXISTS"
    
    if [ -f "$dt_path_bus0/status" ]; then
        status=$(cat "$dt_path_bus0/status" | tr -d '\0')
        echo "Status: $status"
        
        if [ "$status" = "okay" ]; then
            echo "✓ Bus 0 is ENABLED in Device Tree"
        else
            echo "✗ Bus 0 is DISABLED in Device Tree!"
        fi
    fi
else
    echo "⚠ Device Tree node not found at $dt_path_bus0"
fi
echo ""

# Bus 7 (I2C@c250000)
echo "--- Bus 7 (I2C@c250000) ---"
dt_path_bus7="/proc/device-tree/i2c@c250000"
if [ -d "$dt_path_bus7" ]; then
    echo "Device Tree node: EXISTS"
    
    if [ -f "$dt_path_bus7/status" ]; then
        status=$(cat "$dt_path_bus7/status" | tr -d '\0')
        echo "Status: $status"
        
        if [ "$status" = "okay" ]; then
            echo "✓ Bus 7 is ENABLED in Device Tree"
        else
            echo "✗ Bus 7 is DISABLED in Device Tree!"
        fi
    fi
else
    echo "⚠ Device Tree node not found at $dt_path_bus7"
fi
echo ""

# Check GPIO debug info (requires root)
echo "========================================================================="
echo "GPIO PIN STATUS (requires sudo)"
echo "========================================================================="
echo ""

if [ "$EUID" -eq 0 ]; then
    if [ -f "/sys/kernel/debug/gpio" ]; then
        echo "Searching for I2C-related GPIOs..."
        cat /sys/kernel/debug/gpio | grep -i "i2c" || echo "No I2C GPIO info found"
    else
        echo "⚠ /sys/kernel/debug/gpio not available"
        echo "   Try: sudo mount -t debugfs none /sys/kernel/debug"
    fi
else
    echo "⚠ Skipped (requires root)"
    echo "   Run with sudo to see GPIO status"
fi
echo ""

# Check pinmux configuration (requires root)
echo "========================================================================="
echo "PINMUX CONFIGURATION (requires sudo)"
echo "========================================================================="
echo ""

if [ "$EUID" -eq 0 ]; then
    pinmux_path="/sys/kernel/debug/pinctrl/2430000.pinmux/pinmux-pins"
    
    if [ -f "$pinmux_path" ]; then
        echo "Searching for I2C pin configuration..."
        cat "$pinmux_path" | grep -i "i2c" || echo "No I2C pinmux info found"
    else
        echo "⚠ Pinmux debug info not available at $pinmux_path"
    fi
else
    echo "⚠ Skipped (requires root)"
    echo "   Run with sudo to see pinmux configuration"
fi
echo ""

# Kernel module check
echo "========================================================================="
echo "I2C KERNEL MODULES"
echo "========================================================================="
echo ""

echo "Loaded I2C modules:"
lsmod | grep i2c || echo "No I2C modules loaded"
echo ""

# Device scan
echo "========================================================================="
echo "DEVICE SCAN (actual devices connected)"
echo "========================================================================="
echo ""

if command -v i2cdetect &> /dev/null; then
    echo "--- Bus 0 (GPIO 27/28) ---"
    if [ "$EUID" -eq 0 ]; then
        i2cdetect -y 0 2>&1 | grep -v "Warning"
    else
        echo "Run with sudo to scan bus"
    fi
    echo ""
    
    echo "--- Bus 7 (GPIO 29/30) ---"
    if [ "$EUID" -eq 0 ]; then
        i2cdetect -y 7 2>&1 | grep -v "Warning"
    else
        echo "Run with sudo to scan bus"
    fi
else
    echo "⚠ i2cdetect not found. Install with: sudo apt install i2c-tools"
fi
echo ""

# Summary
echo "========================================================================="
echo "SUMMARY"
echo "========================================================================="
echo ""
echo "Pin mapping is FIXED in Device Tree and controlled by pinmux."
echo "GPIO Pins 27/28 → Bus 0 (I2C_IDA_0)"
echo "GPIO Pins 29/30 → Bus 7 (I2C_IDA_1)"
echo ""
echo "To change pin functions, you would need to:"
echo "1. Modify Device Tree configuration (complex, risky)"
echo "2. Use jetson-io tool: sudo /opt/nvidia/jetson-io/jetson-io.py"
echo "3. Or use Software I2C on different GPIO pins (slow)"
echo ""
echo "RECOMMENDED: Use both devices on Bus 7 with different addresses!"
echo "  - LCD @ 0x27 + INA219 @ 0x40 = No conflict, works perfectly"
echo ""
echo "========================================================================="
