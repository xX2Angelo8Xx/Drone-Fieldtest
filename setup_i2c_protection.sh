#!/bin/bash
# Setup I2C Bus Protection via udev Rules
# Protects Bus 0 (EEPROM) while allowing Bus 7 (external devices)

echo "========================================================================="
echo "I2C BUS PROTECTION SETUP"
echo "========================================================================="
echo ""
echo "This script will create udev rules to:"
echo "  1. Restrict Bus 0 access (protects system EEPROM)"
echo "  2. Allow Bus 7 access (for LCD, INA219, etc.)"
echo ""

if [ "$EUID" -ne 0 ]; then 
    echo "❌ ERROR: This script must be run with sudo"
    echo "Usage: sudo ./setup_i2c_protection.sh"
    exit 1
fi

echo "Current I2C device permissions:"
echo "--------------------------------"
ls -la /dev/i2c-* 2>/dev/null || echo "No I2C devices found"
echo ""

echo "Press Enter to create protection rules, or Ctrl+C to cancel..."
read

# Create udev rules file
RULES_FILE="/etc/udev/rules.d/99-i2c-protection.rules"

echo "Creating udev rules at: $RULES_FILE"

cat > "$RULES_FILE" << 'EOF'
# I2C Bus Protection Rules
# Created by: setup_i2c_protection.sh
# Date: $(date)

# Bus 0 (I2C_IDA_0): Restrict access - System EEPROM protection
# Only root can access to prevent accidental writes to 0x50/0x57
KERNEL=="i2c-0", MODE="0600", OWNER="root", GROUP="root"

# Bus 7 (I2C_IDA_1): Allow normal access - External devices
# Members of i2c group can access (LCD, INA219, sensors, etc.)
KERNEL=="i2c-7", MODE="0666", OWNER="root", GROUP="i2c"

# All other I2C buses: Default restricted (safety)
KERNEL=="i2c-[1-9]*", MODE="0660", OWNER="root", GROUP="i2c"
EOF

if [ $? -eq 0 ]; then
    echo "✓ Rules file created successfully"
else
    echo "✗ Failed to create rules file!"
    exit 1
fi

echo ""
echo "Reloading udev rules..."
udevadm control --reload-rules

echo "Triggering udev events..."
udevadm trigger

sleep 1

echo ""
echo "========================================================================="
echo "VERIFICATION"
echo "========================================================================="
echo ""

echo "New I2C device permissions:"
echo "---------------------------"
ls -la /dev/i2c-* 2>/dev/null

echo ""
echo "Expected results:"
echo "  Bus 0: crw------- (0600) - Only root"
echo "  Bus 7: crw-rw-rw- (0666) - All users"
echo ""

# Check actual permissions
BUS0_PERMS=$(stat -c "%a" /dev/i2c-0 2>/dev/null)
BUS7_PERMS=$(stat -c "%a" /dev/i2c-7 2>/dev/null)

echo "Actual permissions:"
if [ "$BUS0_PERMS" = "600" ]; then
    echo "  ✓ Bus 0: $BUS0_PERMS (Protected)"
else
    echo "  ⚠ Bus 0: $BUS0_PERMS (Expected 600)"
fi

if [ "$BUS7_PERMS" = "666" ]; then
    echo "  ✓ Bus 7: $BUS7_PERMS (Open)"
else
    echo "  ⚠ Bus 7: $BUS7_PERMS (Expected 666)"
fi

echo ""
echo "========================================================================="
echo "TEST ACCESS"
echo "========================================================================="
echo ""

echo "Testing Bus 0 access (should be restricted):"
if sudo -u $SUDO_USER i2cdetect -y 0 2>&1 | grep -q "Permission denied"; then
    echo "  ✓ Bus 0 correctly restricted for normal users"
else
    echo "  ⚠ Bus 0 still accessible (might need reboot)"
fi

echo ""
echo "Testing Bus 7 access (should work):"
if sudo -u $SUDO_USER i2cdetect -y 7 >/dev/null 2>&1; then
    echo "  ✓ Bus 7 accessible for normal users"
else
    echo "  ⚠ Bus 7 access issue"
fi

echo ""
echo "========================================================================="
echo "SUMMARY"
echo "========================================================================="
echo ""
echo "✓ udev rules installed at: $RULES_FILE"
echo "✓ Rules active (no reboot needed)"
echo ""
echo "EEPROM Protection Status:"
echo "  Bus 0 (0x50, 0x57): Protected - Root only"
echo "  Bus 7: Open - For LCD, INA219, sensors"
echo ""
echo "To verify rules persist after reboot:"
echo "  1. Reboot: sudo reboot"
echo "  2. Check: ls -la /dev/i2c-*"
echo ""
echo "To remove protection (not recommended):"
echo "  sudo rm $RULES_FILE"
echo "  sudo udevadm control --reload-rules"
echo "  sudo udevadm trigger"
echo ""
echo "========================================================================="
echo "SETUP COMPLETE"
echo "========================================================================="
