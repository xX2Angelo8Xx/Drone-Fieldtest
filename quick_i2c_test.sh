#!/bin/bash
# Quick I2C Bus Test with LCD/INA219
# Tests if a specific device appears on Bus 0 vs Bus 7

echo "========================================================================="
echo "I2C DEVICE DETECTION TEST"
echo "========================================================================="
echo ""
echo "This script helps test if your LCD or INA219 appears on Bus 0"
echo ""
echo "Current Bus Status:"
echo "-------------------"
echo ""

echo "Bus 0 (I2C_IDA_0):"
sudo i2cdetect -y 0 2>&1 | grep -v "Warning"
echo ""

echo "Bus 7 (I2C_IDA_1):"
sudo i2cdetect -y 7 2>&1 | grep -v "Warning"
echo ""

echo "========================================================================="
echo "INTERPRETATION GUIDE"
echo "========================================================================="
echo ""
echo "UU = Device used by kernel driver (usually EEPROM)"
echo "27 = LCD Display at address 0x27"
echo "3F = LCD Display at address 0x3F (alternative)"
echo "40 = INA219 Power Monitor at default address"
echo "-- = No device / Address not responding"
echo ""
echo "========================================================================="
echo "TESTING INSTRUCTIONS"
echo "========================================================================="
echo ""
echo "To test Bus 0 with your INA219:"
echo "1. Unplug INA219 from current connection"
echo "2. Connect INA219 to I2C_IDA_0 (Bus 0):"
echo "   Pin 1: VCC  → INA219 VCC (3.3V)"
echo "   Pin 2: SDA  → INA219 SDA"
echo "   Pin 3: SCL  → INA219 SCL"
echo "   Pin 4: GND  → INA219 GND"
echo "3. Run this script again: sudo ./quick_i2c_test.sh"
echo "4. Look for '40' in Bus 0 scan results"
echo ""
echo "Expected Results:"
echo "- If '40' appears on Bus 0 → Bus 0 works! 🎉"
echo "- If '40' does NOT appear → Bus 0 hardware issue confirmed ❌"
echo ""
echo "To test Bus 0 with your LCD:"
echo "1. Connect LCD to I2C_IDA_0 (Bus 0)"
echo "2. Run this script again"
echo "3. Look for '27' or '3F' in Bus 0 scan results"
echo ""
echo "========================================================================="

# Check if specific devices are present
echo ""
echo "Quick Device Check:"
echo "-------------------"

# Check for INA219 (0x40)
if sudo i2cget -y 0 0x40 0x00 2>&1 | grep -q "0x"; then
    echo "✓ INA219 detected on Bus 0 at 0x40"
elif sudo i2cget -y 7 0x40 0x00 2>&1 | grep -q "0x"; then
    echo "✓ INA219 detected on Bus 7 at 0x40"
else
    echo "✗ INA219 not detected on Bus 0 or 7"
fi

# Check for LCD (0x27)
if sudo i2cget -y 0 0x27 0x00 2>&1 | grep -q "0x"; then
    echo "✓ LCD detected on Bus 0 at 0x27"
elif sudo i2cget -y 7 0x27 0x00 2>&1 | grep -q "0x"; then
    echo "✓ LCD detected on Bus 7 at 0x27"
else
    echo "✗ LCD not detected at 0x27"
fi

# Check for LCD (0x3F)
if sudo i2cget -y 0 0x3F 0x00 2>&1 | grep -q "0x"; then
    echo "✓ LCD detected on Bus 0 at 0x3F"
elif sudo i2cget -y 7 0x3F 0x00 2>&1 | grep -q "0x"; then
    echo "✓ LCD detected on Bus 7 at 0x3F"
else
    echo "✗ LCD not detected at 0x3F"
fi

echo ""
echo "========================================================================="
