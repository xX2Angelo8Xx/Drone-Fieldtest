#!/bin/bash
# I2C Bus 0 Complete Diagnostic Suite
# Runs all available diagnostic tools systematically

echo "========================================================================="
echo "I2C BUS 0 COMPLETE DIAGNOSTIC SUITE"
echo "Jetson Orin Nano - I2C_IDA_0 Hardware Troubleshooting"
echo "========================================================================="
echo ""
echo "This script will run multiple diagnostic tools to identify the issue"
echo "with I2C_IDA_0 (Bus 0). Please ensure:"
echo "  1. You have a known working device (LCD or INA219)"
echo "  2. The device is currently connected to Bus 0"
echo "  3. You are running this with sudo"
echo ""

if [ "$EUID" -ne 0 ]; then 
    echo "⚠ ERROR: This script must be run with sudo"
    echo "Usage: sudo ./test_i2c_bus0_complete.sh"
    exit 1
fi

echo "Press Enter to start diagnostic..."
read

# Test 1: Basic system check
echo ""
echo "========================================================================="
echo "TEST 1: System & Kernel Module Check"
echo "========================================================================="
echo ""

echo "Checking loaded I2C kernel modules..."
lsmod | grep i2c
echo ""

echo "Checking I2C device nodes..."
ls -la /dev/i2c-* 2>/dev/null || echo "No I2C device nodes found!"
echo ""

# Test 2: Run C++ diagnostic tool
echo ""
echo "========================================================================="
echo "TEST 2: Hardware-Level Diagnostic (C++)"
echo "========================================================================="
echo ""

if [ -f "./diagnose_i2c_bus0" ]; then
    ./diagnose_i2c_bus0
else
    echo "⚠ C++ diagnostic tool not found. Compile with:"
    echo "   g++ -o diagnose_i2c_bus0 diagnose_i2c_bus0.cpp -std=c++17"
fi

# Test 3: Run Python detailed diagnostic
echo ""
echo "========================================================================="
echo "TEST 3: System-Level Diagnostic (Python)"
echo "========================================================================="
echo ""

if [ -f "./diagnose_i2c_bus0_detailed.py" ]; then
    python3 ./diagnose_i2c_bus0_detailed.py
else
    echo "⚠ Python diagnostic tool not found"
fi

# Test 4: Manual i2cdetect scan comparison
echo ""
echo "========================================================================="
echo "TEST 4: Side-by-Side Bus Scan (i2cdetect)"
echo "========================================================================="
echo ""

if command -v i2cdetect &> /dev/null; then
    echo "--- Bus 7 (I2C_IDA_1 - Should show device at 0x40 if INA219 connected) ---"
    i2cdetect -y 7
    echo ""
    
    echo "--- Bus 0 (I2C_IDA_0 - Testing if device appears here) ---"
    i2cdetect -y 0
    echo ""
else
    echo "⚠ i2cdetect not installed. Install with:"
    echo "   sudo apt install i2c-tools"
fi

# Test 5: Check for existing INA219 test script
echo ""
echo "========================================================================="
echo "TEST 5: INA219 Specific Test (if connected)"
echo "========================================================================="
echo ""

if [ -f "./diagnose_ina219.py" ]; then
    echo "Running INA219 diagnostic script..."
    python3 ./diagnose_ina219.py
else
    echo "INA219 diagnostic script not found (optional)"
fi

# Test 6: Try existing LCD test (user can modify bus number)
echo ""
echo "========================================================================="
echo "TEST 6: LCD Test Instructions"
echo "========================================================================="
echo ""

if [ -f "./simple_lcd_test" ]; then
    echo "LCD test binary found. To test LCD on Bus 0:"
    echo ""
    echo "1. Ensure LCD is connected to I2C_IDA_0"
    echo "2. Edit simple_lcd_test.cpp and change bus number to 0"
    echo "3. Recompile: g++ -o simple_lcd_test simple_lcd_test.cpp -std=c++17"
    echo "4. Run: sudo ./simple_lcd_test"
    echo ""
    echo "Current simple_lcd_test uses bus 7. Run it? (y/n)"
    read -r response
    if [[ "$response" == "y" ]]; then
        ./simple_lcd_test
    fi
else
    echo "LCD test binary not found"
fi

# Summary and recommendations
echo ""
echo "========================================================================="
echo "DIAGNOSTIC SUMMARY & RECOMMENDATIONS"
echo "========================================================================="
echo ""

echo "Based on the tests above, analyze the following:"
echo ""
echo "1. Device Detection:"
echo "   - Did Bus 7 show device(s)? (Expected: YES)"
echo "   - Did Bus 0 show device(s)? (Expected: NO if bus is broken)"
echo ""
echo "2. Hardware Access:"
echo "   - Could the diagnostic tools open /dev/i2c-0?"
echo "   - Were there any permission errors?"
echo ""
echo "3. Comparison:"
echo "   - If Bus 7 works perfectly but Bus 0 shows nothing,"
echo "     this confirms Bus 0 hardware issue"
echo ""
echo "========================================================================="
echo "RECOMMENDED ACTIONS:"
echo "========================================================================="
echo ""
echo "If Bus 0 is confirmed broken:"
echo ""
echo "✓ IMMEDIATE SOLUTION (Works Now):"
echo "  Use both LCD and INA219 on Bus 7 (I2C_IDA_1)"
echo "  - LCD: 0x27 (or 0x3F)"
echo "  - INA219: 0x40"
echo "  - No address conflict - both will work simultaneously!"
echo ""
echo "⚠ HARDWARE CHECKS (Before giving up on Bus 0):"
echo "  1. Measure voltage on Bus 0 pins with multimeter:"
echo "     - VCC pin: Should be 3.3V"
echo "     - SDA/SCL idle: Should be 3.3V (pull-up)"
echo "     - If 0V or wrong voltage: Hardware damaged"
echo ""
echo "  2. Visual inspection:"
echo "     - Check for bent/broken pins on connector"
echo "     - Check for solder bridges or damage on board"
echo ""
echo "  3. Try external pull-up resistors:"
echo "     - Add 2.2kΩ resistors from SDA/SCL to 3.3V"
echo "     - Sometimes internal pull-ups fail"
echo ""
echo "🔧 FUTURE EXPANSION:"
echo "  Consider I2C multiplexer (TCA9548A) if you need >2 devices"
echo "  - Adds 8 I2C channels to a single bus"
echo "  - Cost: ~$5-10"
echo ""
echo "========================================================================="
echo "DIAGNOSTIC COMPLETE"
echo "========================================================================="
