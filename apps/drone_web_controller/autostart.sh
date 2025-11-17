#!/bin/bash
# Autostart script for Drone Web Controller with Desktop file control
# Replaces the old data_collector autostart with web-based control

echo "🚁 DRONE WEB CONTROLLER AUTOSTART 🚁"
echo "======================================"

PROJECT_ROOT="/home/angelo/Projects/Drone-Fieldtest"
START_SCRIPT="$PROJECT_ROOT/scripts/start_drone.sh"
LCD_TOOL="$PROJECT_ROOT/build/tools/lcd_display_tool"
DESKTOP_AUTOSTART_FILE="/home/angelo/Desktop/Autostart"

# Show early LCD message 
"$LCD_TOOL" "Autostart" "Starting..." 2>/dev/null || true
sleep 2

# Check if Desktop autostart control file exists
if [ ! -f "$DESKTOP_AUTOSTART_FILE" ]; then
    echo "⏸️  AUTOSTART DISABLED: File 'Autostart' not found on Desktop"
    echo "📁 To enable autostart, create file: $DESKTOP_AUTOSTART_FILE"
    echo "💡 To disable autostart, rename or delete the Desktop file"
    echo ""
    echo "Autostart skipped. System ready for manual operation."
    
    # Update LCD to show autostart disabled
    "$LCD_TOOL" "Autostart" "Disabled" 2>/dev/null || true
    
    exit 0
fi

echo "✅ AUTOSTART ENABLED: Found Desktop/Autostart file"
echo "💡 To disable autostart: rename/delete Desktop/Autostart file"
echo ""

# Update LCD with autostart enabled status
"$LCD_TOOL" "Autostart" "Enabled!" 2>/dev/null || true
sleep 2

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

# Update LCD before starting
"$LCD_TOOL" "Starting" "Drone System" 2>/dev/null || true
sleep 2

# Execute the start_drone.sh script (it handles everything)
exec "$START_SCRIPT"