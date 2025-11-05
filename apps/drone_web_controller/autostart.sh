#!/bin/bash
# Autostart script for Drone Web Controller with Desktop file control
# Replaces the old data_collector autostart with web-based control

echo "🚁 DRONE WEB CONTROLLER AUTOSTART 🚁"
echo "======================================"

PROJECT_ROOT="/home/angelo/Projects/Drone-Fieldtest"
WEB_CONTROLLER="$PROJECT_ROOT/build/apps/drone_web_controller/drone_web_controller"
LCD_TOOL="$PROJECT_ROOT/build/tools/lcd_display_tool"
DESKTOP_AUTOSTART_FILE="/home/angelo/Desktop/Autostart"

# Show early LCD message 
"$LCD_TOOL" "Autostart" "Started!" 2>/dev/null || true

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

# Check if executable exists
if [ ! -f "$WEB_CONTROLLER" ]; then
    echo "❌ ERROR: Web controller executable not found at $WEB_CONTROLLER"
    echo "Please build the project first with: ./scripts/build.sh"
    exit 1
fi

# Change to project directory
cd "$PROJECT_ROOT"

# Set environment variables for display (needed for ZED SDK)
export DISPLAY=:0
export XAUTHORITY=/home/angelo/.Xauthority

echo "📂 Working directory: $(pwd)"
echo "🔧 Executable: $WEB_CONTROLLER"
echo "📱 Starting Drone Web Controller..."
echo ""

# Update LCD before starting web controller
"$LCD_TOOL" "Starting" "Web Controller" 2>/dev/null || true

# Run the web controller
exec "$WEB_CONTROLLER"