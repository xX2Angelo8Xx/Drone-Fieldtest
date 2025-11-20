#!/bin/bash
# Autostart script for Drone Web Controller with Desktop file control
# Replaces the old data_collector autostart with web-based control

echo "🚁 DRONE WEB CONTROLLER AUTOSTART 🚁"
echo "======================================"

PROJECT_ROOT="/home/angelo/Projects/Drone-Fieldtest"
START_SCRIPT="$PROJECT_ROOT/scripts/start_drone.sh"
LCD_TOOL="$PROJECT_ROOT/build/tools/lcd_display_tool"
DESKTOP_AUTOSTART_FILE="/home/angelo/Desktop/Autostart"

# Bug #1 Fix: Show boot sequence on LCD (messages persist until next update)
# First message: System booted
"$LCD_TOOL" "System" "Booted!" 2>/dev/null || true
sleep 2

# Check if Desktop autostart control file exists FIRST (no duplicate messages)
if [ ! -f "$DESKTOP_AUTOSTART_FILE" ]; then
    # Autostart disabled - show message and exit
    "$LCD_TOOL" "Autostart" "Disabled" 2>/dev/null || true
    
    echo "⏸️  AUTOSTART DISABLED: File 'Autostart' not found on Desktop"
    echo "📁 To enable autostart, create file: $DESKTOP_AUTOSTART_FILE"
    echo "💡 To disable autostart, rename or delete the Desktop file"
    echo ""
    echo "Autostart skipped. System ready for manual operation."
    
    exit 0
fi

# Autostart enabled - show progress
echo "✅ AUTOSTART ENABLED: Found Desktop/Autostart file"
echo "💡 To disable autostart: rename/delete Desktop/Autostart file"
echo ""

# Show autostart enabled message (persists until next update)
"$LCD_TOOL" "Autostart" "Enabled!" 2>/dev/null || true
sleep 2

# Show "Starting Script" message (persists until main app takes over)
"$LCD_TOOL" "Starting" "Script..." 2>/dev/null || true
sleep 1

# Check if start script exists
if [ ! -f "$START_SCRIPT" ]; then
    echo "❌ ERROR: Start script not found at $START_SCRIPT"
    "$LCD_TOOL" "Error!" "No start script" 2>/dev/null || true
    exit 1
fi

# Change to project directory
cd "$PROJECT_ROOT"

# Set environment variables for display (needed for ZED SDK)
export DISPLAY=:0
export XAUTHORITY=/home/angelo/.Xauthority

echo "📂 Working directory: $(pwd)"
echo "🔧 Start script: $START_SCRIPT"
echo "📱 Starting Drone Web Controller via start_drone.sh..."
echo ""

# Execute the start_drone.sh script (it handles LCD and everything else)
# The "Starting Script..." message will persist until start_drone.sh updates it
exec "$START_SCRIPT"