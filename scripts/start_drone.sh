#!/bin/bash
# Drone Controller Startup Script
# Starts the drone web controller with proper logging and background execution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXECUTABLE="$PROJECT_ROOT/build/apps/drone_web_controller/drone_web_controller"
LOG_FILE="/tmp/drone_controller.log"
PID_FILE="/tmp/drone_controller.pid"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}🚁 DRONE CONTROLLER STARTUP${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if already running
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE")
    if ps -p "$OLD_PID" > /dev/null 2>&1; then
        echo -e "${YELLOW}⚠️  Drone Controller is already running (PID: $OLD_PID)${NC}"
        echo -e "${YELLOW}   Use 'drone stop' to stop it first${NC}"
        exit 1
    else
        echo -e "${YELLOW}⚠️  Found stale PID file, removing...${NC}"
        rm -f "$PID_FILE"
    fi
fi

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}✗ Executable not found: $EXECUTABLE${NC}"
    echo -e "${YELLOW}  Run './scripts/build.sh' first${NC}"
    exit 1
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}✗ This script must be run as root (sudo)${NC}"
    echo -e "${YELLOW}  Use: sudo $0${NC}"
    exit 1
fi

# Clear old log
> "$LOG_FILE"

echo -e "${GREEN}✓ Pre-flight checks passed${NC}"
echo ""
echo -e "${BLUE}Starting Drone Controller...${NC}"
echo -e "  Executable: $EXECUTABLE"
echo -e "  Log file:   $LOG_FILE"
echo -e "  Working dir: $PROJECT_ROOT"
echo ""

# Start in background with nohup
cd "$PROJECT_ROOT"
nohup "$EXECUTABLE" > "$LOG_FILE" 2>&1 &
DRONE_PID=$!

# Save PID
echo "$DRONE_PID" > "$PID_FILE"

# Wait a bit to see if it crashes immediately
sleep 3

if ! ps -p "$DRONE_PID" > /dev/null 2>&1; then
    echo -e "${RED}✗ Drone Controller failed to start!${NC}"
    echo -e "${YELLOW}  Check log: tail -50 $LOG_FILE${NC}"
    rm -f "$PID_FILE"
    exit 1
fi

echo -e "${GREEN}✓ Drone Controller started successfully!${NC}"
echo -e "  PID: $DRONE_PID"
echo ""

# Wait for web server to be ready
echo -e "${BLUE}Waiting for web server...${NC}"
MAX_WAIT=30
WAIT_COUNT=0
while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if curl -s http://10.42.0.1:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Web server is ready!${NC}"
        break
    fi
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
    echo -n "."
done
echo ""

if [ $WAIT_COUNT -ge $MAX_WAIT ]; then
    echo -e "${YELLOW}⚠️  Web server not responding after ${MAX_WAIT}s${NC}"
    echo -e "${YELLOW}  Check log: tail -50 $LOG_FILE${NC}"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}🎉 DRONE CONTROLLER READY${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Web Interface: http://10.42.0.1:8080${NC}"
echo -e "${GREEN}SSID:          DroneController${NC}"
echo -e "${GREEN}Password:      drone123${NC}"
echo ""
echo -e "${BLUE}Management Commands:${NC}"
echo -e "  View log:  ${YELLOW}tail -f $LOG_FILE${NC}"
echo -e "  Stop:      ${YELLOW}sudo $SCRIPT_DIR/stop_drone.sh${NC}"
echo -e "  Status:    ${YELLOW}ps aux | grep drone_web_controller${NC}"
echo ""
echo -e "${BLUE}Network Log:${NC}"
echo -e "  ${YELLOW}sudo tail -f /var/log/drone_controller_network.log${NC}"
echo -e "${BLUE}========================================${NC}"
