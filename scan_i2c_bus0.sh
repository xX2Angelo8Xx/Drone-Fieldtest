#!/bin/bash
# Comprehensive I2C scanner - tests every address on bus 0

echo "=========================================="
echo "COMPREHENSIVE I2C BUS 0 SCANNER"
echo "=========================================="
echo "Testing all 7-bit addresses (0x03-0x77)..."
echo ""

found=0

for addr in $(seq 3 119); do
    hex_addr=$(printf "0x%02x" $addr)
    
    # Try to read from device (suppress errors)
    result=$(sudo i2cget -y 0 $addr 0x00 2>&1)
    
    if [[ ! "$result" =~ "Error" ]]; then
        echo "✓ DEVICE FOUND at $hex_addr - Response: $result"
        found=$((found + 1))
    fi
done

echo ""
echo "=========================================="
if [ $found -eq 0 ]; then
    echo "No devices found on bus 0!"
    echo ""
    echo "Possible issues:"
    echo "1. SDA/SCL wires swapped"
    echo "2. INA219 not powered (check 3.3V on VCC pin)"
    echo "3. Wrong I2C bus (try bus 1 or 7 instead)"
    echo "4. Faulty INA219 module"
    echo "5. Missing pull-up resistors (usually on module)"
else
    echo "Found $found device(s) on bus 0"
fi
echo "=========================================="
