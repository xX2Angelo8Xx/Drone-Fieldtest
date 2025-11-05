#!/bin/bash

echo "🔧 WiFi Hotspot Setup - Complete Disconnect Method"
echo "=================================================="

# Kill any existing processes
echo "🛑 Stopping existing services..."
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

# CRITICAL: Disconnect from current WiFi network first!
echo "📡 Disconnecting from current WiFi network..."
sudo nmcli device disconnect wlP1p1s0
sudo systemctl stop NetworkManager
sleep 2

# Reset interface completely
echo "🔄 Resetting WiFi interface..."
sudo ip link set wlP1p1s0 down
sudo iw dev wlP1p1s0 set type managed
sudo ip addr flush dev wlP1p1s0
sudo ip link set wlP1p1s0 up
sleep 2

# Set to AP mode and configure IP
echo "📶 Setting up Access Point mode..."
sudo iw dev wlP1p1s0 set type __ap
sudo ip addr add 192.168.4.1/24 dev wlP1p1s0

# Create minimal hostapd config (no advanced features)
echo "📝 Creating hostapd configuration..."
cat > /tmp/minimal_hostapd.conf << EOF
interface=wlP1p1s0
driver=nl80211
ssid=DroneController
hw_mode=g
channel=6
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=drone123
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

# Create dnsmasq config
echo "📝 Creating DHCP configuration..."
cat > /tmp/minimal_dnsmasq.conf << EOF
interface=wlP1p1s0
bind-interfaces
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
dhcp-option=3,192.168.4.1
dhcp-option=6,8.8.8.8
port=0
log-dhcp
EOF

echo "🚀 Starting Access Point..."
sudo hostapd /tmp/minimal_hostapd.conf &
HOSTAPD_PID=$!
sleep 8

echo "🌐 Starting DHCP server..."
sudo dnsmasq -C /tmp/minimal_dnsmasq.conf &
DNSMASQ_PID=$!
sleep 3

echo ""
echo "✅ HOTSPOT SHOULD BE ACTIVE!"
echo "📱 Network Name: DroneController"
echo "🔐 Password: drone123"
echo "🌐 Gateway: 192.168.4.1"
echo ""

# Show actual status
echo "📊 Interface Status:"
ip addr show wlP1p1s0 | grep -E "(inet |state)"
echo ""
echo "📡 WiFi Mode:"
iwconfig wlP1p1s0 | grep -E "(Mode:|ESSID:|Frequency:)"
echo ""

echo "🔍 Check your phone for 'DroneController' network!"
echo "⏱️  Give it 30 seconds to appear in WiFi list"
echo ""
echo "Press any key to stop and restore NetworkManager..."
read -n 1

# Cleanup and restore
echo ""
echo "🛑 Stopping hotspot..."
sudo kill $HOSTAPD_PID 2>/dev/null || true
sudo kill $DNSMASQ_PID 2>/dev/null || true
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

echo "🔄 Restoring NetworkManager..."
sudo ip link set wlP1p1s0 down
sudo iw dev wlP1p1s0 set type managed
sudo ip addr flush dev wlP1p1s0
sudo ip link set wlP1p1s0 up
sudo systemctl start NetworkManager

echo "✅ WiFi restored to normal mode"