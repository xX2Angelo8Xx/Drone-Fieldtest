#!/bin/bash

echo "🔧 Forcing 2.4GHz WiFi Hotspot..."

# Stop everything
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

# Reset interface completely
sudo ip link set wlP1p1s0 down
sudo ip addr flush dev wlP1p1s0
sudo iw dev wlP1p1s0 set type managed

# Wait
sleep 3

# Force 2.4GHz configuration
sudo ip link set wlP1p1s0 up
sudo iw dev wlP1p1s0 set type __ap

# Force specific frequency BEFORE IP assignment
echo "📡 Setting frequency to 2437 MHz (Channel 6)..."
sudo iw dev wlP1p1s0 set freq 2437

sleep 2

# Now set IP
sudo ip addr add 192.168.4.1/24 dev wlP1p1s0

# Create strict 2.4GHz hostapd config
cat > /tmp/force_2ghz_hostapd.conf << EOF
interface=wlP1p1s0
driver=nl80211
ssid=DroneController
hw_mode=g
channel=6
ieee80211n=0
ieee80211ac=0
ieee80211ax=0
max_num_sta=10
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=drone123
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

# Simple dnsmasq config
cat > /tmp/force_dnsmasq.conf << EOF
interface=wlP1p1s0
bind-interfaces
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
dhcp-option=3,192.168.4.1
dhcp-option=6,8.8.8.8
port=0
log-dhcp
EOF

echo "🚀 Starting hostapd with forced 2.4GHz..."
sudo hostapd /tmp/force_2ghz_hostapd.conf &
HOSTAPD_PID=$!

sleep 5

echo "🌐 Starting DHCP server..."
sudo dnsmasq -C /tmp/force_dnsmasq.conf &
DNSMASQ_PID=$!

sleep 3

echo "✅ Forced 2.4GHz Hotspot Active!"
echo "📱 Network: DroneController"  
echo "🔐 Password: drone123"
echo "📡 Should be on 2437 MHz (Channel 6)"

# Check current frequency
echo "📊 Current WiFi Status:"
iw dev wlP1p1s0 info
echo ""
iwconfig wlP1p1s0

echo ""
echo "🔍 Check your phone now for 'DroneController' network!"
echo "Press any key to stop..."
read -n 1

# Cleanup
echo "🛑 Stopping services..."
sudo kill $HOSTAPD_PID 2>/dev/null || true  
sudo kill $DNSMASQ_PID 2>/dev/null || true
sudo pkill hostapd 2>/dev/null || true
sudo pkill dnsmasq 2>/dev/null || true

echo "✅ Done."