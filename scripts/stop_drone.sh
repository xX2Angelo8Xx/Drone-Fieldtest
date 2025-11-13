#!/bin/bash
# Drone Controller Stop Script

PID_FILE="/tmp/drone_controller.pid"
LOG_FILE="/tmp/drone_controller.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}🛑 Stopping Drone Controller...${NC}"

if [ ! -f "$PID_FILE" ]; then
    echo -e "${YELLOW}⚠️  No PID file found${NC}"
    # Try to find process anyway
    PIDS=$(pgrep -f drone_web_controller)
    if [ -z "$PIDS" ]; then
        echo -e "${GREEN}✓ No drone controller process running${NC}"
        exit 0
    fi
    echo -e "${YELLOW}  Found running process(es): $PIDS${NC}"
    echo -e "${YELLOW}  Killing...${NC}"
    kill $PIDS
    sleep 2
    if pgrep -f drone_web_controller > /dev/null; then
        echo -e "${RED}  Process still alive, using SIGKILL${NC}"
        kill -9 $PIDS
    fi
    echo -e "${GREEN}✓ Stopped${NC}"
    exit 0
fi

PID=$(cat "$PID_FILE")
if ! ps -p "$PID" > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Process not running (stale PID file)${NC}"
    rm -f "$PID_FILE"
    exit 0
fi

echo -e "${BLUE}  Sending SIGTERM to PID $PID...${NC}"
kill "$PID"

# Wait for graceful shutdown
WAIT_COUNT=0
MAX_WAIT=10
while [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    if ! ps -p "$PID" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Stopped gracefully${NC}"
        rm -f "$PID_FILE"
        exit 0
    fi
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
    echo -n "."
done
echo ""

echo -e "${YELLOW}⚠️  Process did not stop gracefully, using SIGKILL${NC}"
kill -9 "$PID"
rm -f "$PID_FILE"
echo -e "${GREEN}✓ Stopped forcefully${NC}"
