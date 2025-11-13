#!/bin/bash
# Emergency Network Recovery Script for Jetson
# Use this script if the Drone Controller breaks your network
# Version: 1.0
# Last Updated: 2025-11-12

set -e

echo "========================================="
echo "🛰️ JETSON NETWORK EMERGENCY RECOVERY"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Check if we're root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}ERROR: Please run as root (sudo)${NC}"
    exit 1
fi

echo -e "${YELLOW}Step 1/8: Checking NetworkManager...${NC}"
if systemctl is-active --quiet NetworkManager; then
    echo -e "${GREEN}✓ NetworkManager is running${NC}"
else
    echo -e "${RED}✗ NetworkManager is NOT running, starting...${NC}"
    systemctl start NetworkManager
    systemctl enable NetworkManager
    echo -e "${GREEN}✓ NetworkManager started${NC}"
fi

echo ""
echo -e "${YELLOW}Step 2/8: Restoring DNS configuration...${NC}"
if [ -L "/etc/resolv.conf" ]; then
    echo -e "${GREEN}✓ /etc/resolv.conf is a symlink (good)${NC}"
else
    echo -e "${RED}✗ /etc/resolv.conf is not a symlink, fixing...${NC}"
    rm -f /etc/resolv.conf
    ln -s /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
    echo -e "${GREEN}✓ DNS symlink restored${NC}"
fi

# Ensure systemd-resolved is running
if systemctl is-active --quiet systemd-resolved; then
    echo -e "${GREEN}✓ systemd-resolved is running${NC}"
else
    echo -e "${RED}✗ systemd-resolved is NOT running, starting...${NC}"
    systemctl start systemd-resolved
    systemctl enable systemd-resolved
    echo -e "${GREEN}✓ systemd-resolved started${NC}"
fi

# Check for dnsmasq blocking port 53
echo ""
echo -e "${YELLOW}Checking for dnsmasq (may block port 53)...${NC}"
if systemctl is-active --quiet dnsmasq 2>/dev/null || pgrep -x dnsmasq > /dev/null; then
    echo -e "${RED}✗ dnsmasq is running and may block port 53, stopping...${NC}"
    systemctl stop dnsmasq 2>/dev/null || true
    systemctl disable dnsmasq 2>/dev/null || true
    # Kill any remaining dnsmasq processes
    pkill dnsmasq 2>/dev/null || true
    echo -e "${GREEN}✓ dnsmasq stopped and disabled${NC}"
    # Restart systemd-resolved to reclaim port 53
    systemctl restart systemd-resolved
    echo -e "${GREEN}✓ systemd-resolved restarted${NC}"
else
    echo -e "${GREEN}✓ dnsmasq is not running${NC}"
fi

echo ""
echo -e "${YELLOW}Step 3/8: Unblocking RF-Kill...${NC}"
rfkill unblock all
echo -e "${GREEN}✓ RF-Kill unblocked${NC}"

echo ""
echo -e "${YELLOW}Step 4/8: Checking WiFi interface...${NC}"
WIFI_INTERFACE="wlP1p1s0"
if ip link show "$WIFI_INTERFACE" &>/dev/null; then
    echo -e "${GREEN}✓ WiFi interface $WIFI_INTERFACE exists${NC}"
    
    # Reset interface to managed mode
    nmcli dev set "$WIFI_INTERFACE" managed yes 2>/dev/null || true
    ip link set "$WIFI_INTERFACE" down 2>/dev/null || true
    sleep 1
    ip link set "$WIFI_INTERFACE" up 2>/dev/null || true
    echo -e "${GREEN}✓ WiFi interface reset to managed mode${NC}"
else
    echo -e "${RED}✗ WiFi interface $WIFI_INTERFACE not found${NC}"
fi

echo ""
echo -e "${YELLOW}Step 5/8: Deactivating DroneController hotspot (if active)...${NC}"
if nmcli con show DroneController &>/dev/null; then
    nmcli con down DroneController 2>/dev/null || true
    echo -e "${GREEN}✓ DroneController hotspot deactivated${NC}"
else
    echo -e "${GREEN}✓ DroneController hotspot not found (already cleaned)${NC}"
fi

echo ""
echo -e "${YELLOW}Step 6/8: Re-enabling autoconnect for all WiFi networks...${NC}"
WIFI_CONS=$(nmcli -t -f NAME,TYPE con show | grep ':802-11-wireless$' | cut -d: -f1)
if [ -n "$WIFI_CONS" ]; then
    while IFS= read -r con; do
        if [ "$con" != "DroneController" ]; then
            nmcli con modify "$con" connection.autoconnect yes 2>/dev/null || true
            echo -e "${GREEN}  ✓ Enabled autoconnect for: $con${NC}"
        fi
    done <<< "$WIFI_CONS"
else
    echo -e "${YELLOW}  ! No WiFi connections found${NC}"
fi

echo ""
echo -e "${YELLOW}Step 7/8: Restarting NetworkManager...${NC}"
systemctl restart NetworkManager
sleep 3
echo -e "${GREEN}✓ NetworkManager restarted${NC}"

echo ""
echo -e "${YELLOW}Step 8/8: Verifying connectivity...${NC}"

# Check LAN
if nmcli dev status | grep -q "ethernet.*connected"; then
    echo -e "${GREEN}✓ LAN (Ethernet) is connected${NC}"
    LAN_OK=true
else
    echo -e "${RED}✗ LAN (Ethernet) is NOT connected${NC}"
    LAN_OK=false
fi

# Check WiFi
if nmcli dev status | grep -q "wifi.*connected"; then
    WIFI_CON=$(nmcli -t -f NAME,DEVICE con show --active | grep ":$WIFI_INTERFACE$" | cut -d: -f1)
    echo -e "${GREEN}✓ WiFi is connected to: $WIFI_CON${NC}"
    WIFI_OK=true
else
    echo -e "${YELLOW}! WiFi is not connected (will try to auto-reconnect)${NC}"
    WIFI_OK=false
    
    # Try to reconnect to first available WiFi
    FIRST_WIFI=$(nmcli -t -f NAME,TYPE con show | grep ':802-11-wireless$' | grep -v 'DroneController' | head -1 | cut -d: -f1)
    if [ -n "$FIRST_WIFI" ]; then
        echo -e "${YELLOW}  Attempting to connect to: $FIRST_WIFI${NC}"
        nmcli con up "$FIRST_WIFI" 2>/dev/null && WIFI_OK=true || WIFI_OK=false
        if [ "$WIFI_OK" = true ]; then
            echo -e "${GREEN}  ✓ Connected to: $FIRST_WIFI${NC}"
        else
            echo -e "${RED}  ✗ Failed to connect to: $FIRST_WIFI${NC}"
        fi
    fi
fi

# Test DNS
echo ""
echo -e "${YELLOW}Testing DNS resolution...${NC}"
if nslookup google.com &>/dev/null; then
    echo -e "${GREEN}✓ DNS resolution works${NC}"
    DNS_OK=true
else
    echo -e "${RED}✗ DNS resolution FAILED${NC}"
    DNS_OK=false
    echo -e "${YELLOW}  Current DNS servers:${NC}"
    cat /etc/resolv.conf | grep nameserver
fi

# Test internet
echo ""
echo -e "${YELLOW}Testing internet connectivity...${NC}"
if ping -c 3 8.8.8.8 &>/dev/null; then
    echo -e "${GREEN}✓ Internet (IP) is reachable${NC}"
    INET_IP_OK=true
else
    echo -e "${RED}✗ Internet (IP) is NOT reachable${NC}"
    INET_IP_OK=false
fi

if ping -c 3 google.com &>/dev/null; then
    echo -e "${GREEN}✓ Internet (DNS) is reachable${NC}"
    INET_DNS_OK=true
else
    echo -e "${RED}✗ Internet (DNS) is NOT reachable${NC}"
    INET_DNS_OK=false
fi

# Final summary
echo ""
echo "========================================="
echo "RECOVERY SUMMARY"
echo "========================================="
echo -e "NetworkManager:    ${GREEN}✓ Running${NC}"
echo -e "systemd-resolved:  ${GREEN}✓ Running${NC}"
echo -e "RF-Kill:           ${GREEN}✓ Unblocked${NC}"

if [ "$LAN_OK" = true ]; then
    echo -e "LAN (Ethernet):    ${GREEN}✓ Connected${NC}"
else
    echo -e "LAN (Ethernet):    ${RED}✗ Not Connected${NC}"
fi

if [ "$WIFI_OK" = true ]; then
    echo -e "WiFi:              ${GREEN}✓ Connected${NC}"
else
    echo -e "WiFi:              ${YELLOW}! Not Connected${NC}"
fi

if [ "$DNS_OK" = true ]; then
    echo -e "DNS Resolution:    ${GREEN}✓ Working${NC}"
else
    echo -e "DNS Resolution:    ${RED}✗ FAILED${NC}"
fi

if [ "$INET_IP_OK" = true ]; then
    echo -e "Internet (IP):     ${GREEN}✓ Reachable${NC}"
else
    echo -e "Internet (IP):     ${RED}✗ NOT Reachable${NC}"
fi

if [ "$INET_DNS_OK" = true ]; then
    echo -e "Internet (DNS):    ${GREEN}✓ Reachable${NC}"
else
    echo -e "Internet (DNS):    ${RED}✗ NOT Reachable${NC}"
fi

echo "========================================="

# Overall status
if [ "$INET_DNS_OK" = true ] || [ "$INET_IP_OK" = true ]; then
    echo -e "${GREEN}✓ RECOVERY SUCCESSFUL - Network is working!${NC}"
    exit 0
elif [ "$LAN_OK" = true ] || [ "$WIFI_OK" = true ]; then
    echo -e "${YELLOW}⚠ PARTIAL RECOVERY - Connected but internet may not work${NC}"
    echo ""
    echo "Additional troubleshooting:"
    echo "  1. Check router connection"
    echo "  2. Check firewall settings"
    echo "  3. Try: sudo systemctl restart NetworkManager"
    exit 1
else
    echo -e "${RED}✗ RECOVERY INCOMPLETE - Manual intervention required${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Check physical network cables"
    echo "  2. Try connecting to WiFi manually: nmcli dev wifi connect <SSID> password <password>"
    echo "  3. Check system logs: journalctl -u NetworkManager -n 50"
    echo "  4. Consider rebooting: sudo reboot"
    exit 2
fi
