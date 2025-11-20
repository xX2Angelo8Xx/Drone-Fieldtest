#!/bin/bash
# Test Script: Connect LCD and INA219 to Bus 7 and verify

echo "========================================================================="
echo "LCD + INA219 PARALLEL CONNECTION TEST"
echo "========================================================================="
echo ""
echo "INSTRUCTIONS:"
echo "============="
echo ""
echo "1. DISCONNECT all I2C devices first"
echo ""
echo "2. CONNECT LCD to Expansion Header:"
echo "   LCD GND  → Pin 9 (or 6, 14, 20, 25, 30, 34, 39)"
echo "   LCD VCC  → Pin 1 (3.3V)"
echo "   LCD SDA  → Pin 3 (I2C1_SDA)"
echo "   LCD SCL  → Pin 5 (I2C1_SCL)"
echo ""
echo "3. SCAN for LCD address:"
echo ""

if [ "$EUID" -eq 0 ]; then
    echo "Scanning Bus 7..."
    i2cdetect -y 7 2>&1 | grep -v "Warning"
    echo ""
    
    # Check for common LCD addresses
    if i2cget -y 7 0x27 0x00 2>&1 | grep -q "0x"; then
        echo "✓ LCD found at address 0x27"
        LCD_ADDR="0x27"
    elif i2cget -y 7 0x3F 0x00 2>&1 | grep -q "0x"; then
        echo "✓ LCD found at address 0x3F"
        LCD_ADDR="0x3F"
    else
        echo "✗ LCD not detected at 0x27 or 0x3F"
        echo "  Check wiring and power"
        exit 1
    fi
    
    echo ""
    echo "LCD Address confirmed: $LCD_ADDR"
    echo ""
    echo "========================================================================="
    echo "NOW ADD INA219"
    echo "========================================================================="
    echo ""
    echo "4. Keep LCD connected, ADD INA219 in PARALLEL:"
    echo ""
    echo "   PARALLEL WIRING (both share same pins!):"
    echo "   ----------------------------------------"
    echo "   Pin 1 (3.3V) ──┬── LCD VCC"
    echo "                  └── INA219 VCC"
    echo ""
    echo "   Pin 3 (SDA)  ──┬── LCD SDA"
    echo "                  └── INA219 SDA"
    echo ""
    echo "   Pin 5 (SCL)  ──┬── LCD SCL"
    echo "                  └── INA219 SCL"
    echo ""
    echo "   Pin 9 (GND)  ──┬── LCD GND"
    echo "                  └── INA219 GND"
    echo ""
    echo "   HINT: You can twist/solder wires together, or use breadboard"
    echo ""
    read -p "Press Enter after connecting INA219 in parallel..." 
    
    echo ""
    echo "Scanning Bus 7 again (should show BOTH devices)..."
    i2cdetect -y 7 2>&1 | grep -v "Warning"
    echo ""
    
    # Check both devices
    echo "Device Detection:"
    echo "-----------------"
    
    if i2cget -y 7 0x27 0x00 2>&1 | grep -q "0x" || i2cget -y 7 0x3F 0x00 2>&1 | grep -q "0x"; then
        echo "✓ LCD detected at $LCD_ADDR"
    else
        echo "✗ LCD not detected (check connection)"
    fi
    
    if i2cget -y 7 0x40 0x00 2>&1 | grep -q "0x"; then
        echo "✓ INA219 detected at 0x40"
    else
        echo "✗ INA219 not detected (check connection)"
    fi
    
    echo ""
    echo "========================================================================="
    echo "EXPECTED RESULT"
    echo "========================================================================="
    echo ""
    echo "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
    echo "20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- --  ← LCD here"
    echo "40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --  ← INA219 here"
    echo ""
    echo "If you see BOTH addresses → SUCCESS! Both work in parallel! 🎉"
    echo ""
    
else
    echo "⚠ This script needs sudo to scan I2C bus"
    echo "Run: sudo $0"
fi
