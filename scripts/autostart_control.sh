#!/bin/bash

echo "🚁 DRONE AUTOSTART CONTROL PANEL 🚁"
echo "====================================="

DESKTOP_FILE="/home/angelo/Desktop/Autostart"

if [ -f "$DESKTOP_FILE" ]; then
    echo "📄 Status: AUTOSTART ENABLED ✅"
    echo "📁 File: $DESKTOP_FILE exists"
    echo ""
    echo "Actions:"
    echo "1. To DISABLE autostart: mv '$DESKTOP_FILE' '/home/angelo/Desktop/Autostart_DISABLED'"
    echo "2. Current file content:"
    echo "   $(head -1 "$DESKTOP_FILE")"
else
    echo "📄 Status: AUTOSTART DISABLED ❌"
    echo "📁 File: $DESKTOP_FILE not found"
    echo ""
    echo "Actions:"
    echo "1. To ENABLE autostart: Create file 'Autostart' on Desktop"
    echo "2. Quick enable: touch '$DESKTOP_FILE'"
    
    # Check for renamed files
    DISABLED_FILES=$(find /home/angelo/Desktop -name "Autostart*" -type f 2>/dev/null)
    if [ ! -z "$DISABLED_FILES" ]; then
        echo ""
        echo "Found disabled autostart files:"
        echo "$DISABLED_FILES"
        echo "To re-enable, rename one to 'Autostart'"
    fi
fi

echo ""
echo "Service status:"
systemctl --user is-enabled drone-recorder 2>/dev/null || echo "Service not installed"

echo ""
echo "Quick commands:"
echo "• Enable:  touch /home/angelo/Desktop/Autostart"
echo "• Disable: mv /home/angelo/Desktop/Autostart /home/angelo/Desktop/Autostart_DISABLED"
echo "• Check:   ls -la /home/angelo/Desktop/Autostart*"