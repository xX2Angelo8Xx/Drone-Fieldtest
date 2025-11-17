#!/bin/bash
# Drone Autostart Service Wrapper
# This script is called by systemd and handles proper startup with root privileges

set -e

SCRIPT_DIR="/home/angelo/Projects/Drone-Fieldtest"
LOG_FILE="/tmp/drone_autostart_service.log"
PID_FILE="/tmp/drone_controller.pid"

echo "======================================" > "$LOG_FILE"
echo "Drone Autostart Service Wrapper" >> "$LOG_FILE"
echo "Started: $(date)" >> "$LOG_FILE"
echo "======================================" >> "$LOG_FILE"

# Step 1: Early LCD Feedback (immediate user feedback)
echo "[$(date +%T)] Step 1: Early LCD Feedback" >> "$LOG_FILE"
"$SCRIPT_DIR/scripts/early_lcd_feedback.sh" >> "$LOG_FILE" 2>&1 || true

# Step 2: Wait for system to be ready
echo "[$(date +%T)] Step 2: Waiting for system ready..." >> "$LOG_FILE"
sleep 5

# Step 3: Start drone controller (this script already has root via systemd)
echo "[$(date +%T)] Step 3: Starting drone controller..." >> "$LOG_FILE"
cd "$SCRIPT_DIR"

# Run the drone web controller directly (no need for start_drone.sh wrapper)
# Set environment for the controller
export HOME=/home/angelo
export USER=angelo

# Start controller in background
nohup "$SCRIPT_DIR/build/apps/drone_web_controller/drone_web_controller" \
    > /tmp/drone_controller.log 2>&1 &

DRONE_PID=$!
echo "$DRONE_PID" > "$PID_FILE"

echo "[$(date +%T)] Controller started with PID: $DRONE_PID" >> "$LOG_FILE"

# Wait a bit to ensure it doesn't crash immediately
sleep 5

if ps -p "$DRONE_PID" > /dev/null 2>&1; then
    echo "[$(date +%T)] ✓ Controller running successfully" >> "$LOG_FILE"
    exit 0
else
    echo "[$(date +%T)] ✗ Controller failed to start" >> "$LOG_FILE"
    exit 1
fi
