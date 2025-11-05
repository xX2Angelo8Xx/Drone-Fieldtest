#!/bin/bash

echo "🔧 Setting up WiFi Hotspot manually..."

# Stop any existing processes
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

# Reset WiFi interface
sudo ip link set wlP1p1s0 down
sudo iw dev wlP1p1s0 set type managed
sudo ip link set wlP1p1s0 up

sleep 2

# Set to AP mode and configure IP
sudo iw dev wlP1p1s0 set type __ap
sudo ip addr flush dev wlP1p1s0
sudo ip addr add 192.168.4.1/24 dev wlP1p1s0

# Create simple hostapd config
cat > /tmp/simple_hostapd.conf << EOF
interface=wlP1p1s0
driver=nl80211
ssid=DroneController
hw_mode=g
channel=6
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=drone123
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

# Create simple dnsmasq config
cat > /tmp/simple_dnsmasq.conf << EOF
interface=wlP1p1s0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
dhcp-option=3,192.168.4.1
dhcp-option=6,8.8.8.8
server=8.8.8.8
log-queries
log-dhcp
bind-interfaces
except-interface=lo
EOF

echo "📡 Starting hostapd..."
sudo hostapd /tmp/simple_hostapd.conf &
HOSTAPD_PID=$!

sleep 5

echo "🌐 Starting dnsmasq..."
sudo dnsmasq -C /tmp/simple_dnsmasq.conf --no-daemon --log-queries &
DNSMASQ_PID=$!

sleep 2

echo "✅ WiFi Hotspot should be active!"
echo "📱 Network: DroneController"
echo "🔐 Password: drone123"
echo "🌐 IP: 192.168.4.1"

# Show status
echo "📊 WiFi Status:"
iwconfig wlP1p1s0
echo ""
echo "📊 IP Configuration:"
ip addr show wlP1p1s0

echo ""
echo "Press any key to stop the hotspot..."
read -n 1

# Cleanup
echo "🛑 Stopping services..."
sudo kill $HOSTAPD_PID 2>/dev/null || true
sudo kill $DNSMASQ_PID 2>/dev/null || true
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

echo "✅ WiFi hotspot stopped."