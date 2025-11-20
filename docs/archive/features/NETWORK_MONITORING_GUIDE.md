# Network Monitoring Guide - Drone Controller

**Date:** 2024-11-18  
**Purpose:** Monitor WiFi AP bandwidth usage during livestream and recording

---

## 🎯 Key Findings from Testing

### CPU Usage Breakdown
- **Idle (no recording, no livestream):** 5-10% CPU
- **Livestream only @ 5 FPS:** 5-10% CPU (minimal overhead!)
- **Recording SVO2 @ HD720/60fps:** 65-70% CPU ⚠️
- **Recording + Livestream @ 5 FPS:** 65-70% CPU (no significant increase)

**Conclusion:** 
- ✅ Recording is the bottleneck, NOT livestream
- ✅ Livestream can safely run at 5-10+ FPS without CPU impact
- ✅ Network bandwidth is likely the limiting factor at high FPS

---

## 📊 Estimated Bandwidth Usage

### JPEG Snapshot Size
- **Typical range:** 50-150 KB per frame (quality 85, HD720)
- **Average:** ~75 KB per frame

### Bandwidth Calculation
| FPS | KB/s | Mbps | Description |
|-----|------|------|-------------|
| 1 Hz | 75 | 0.6 | Very low bandwidth |
| 2 Hz | 150 | 1.2 | Default (balanced) |
| 3 Hz | 225 | 1.8 | Smooth |
| 5 Hz | 375 | 3.0 | Very smooth |
| 7 Hz | 525 | 4.2 | High quality |
| 10 Hz | 750 | 6.0 | Stress test ⚠️ |
| 15 Hz | 1125 | 9.0 | Network limit test ⚠️ |

**WiFi AP Capacity:**
- **Theoretical max:** ~54 Mbps (802.11g) or ~150 Mbps (802.11n)
- **Real-world:** ~20-30 Mbps (due to overhead, interference)
- **Safe operating range:** <10 Mbps for stable connection

---

## 🛠️ Network Monitoring Tools

### 1. Real-Time Bandwidth Monitor: `iftop`

**Install (if not present):**
```bash
sudo apt-get install iftop
```

**Usage - Monitor WiFi AP Interface:**
```bash
# Show bandwidth usage on WiFi interface
sudo iftop -i wlP1p1s0

# Or on shared connection interface
sudo iftop -i wlan0
```

**Interface:**
```
┌─────────────────────────────────────────────┐
│ jetson-orin -> 192.168.4.2   ████████ 6.2Mb │
│ 192.168.4.2 -> jetson-orin   ▌ 0.5Mb        │
│                                              │
│ TX (Upload):   6.2 Mbps                      │
│ RX (Download): 0.5 Mbps                      │
│ TOTAL:         6.7 Mbps                      │
└─────────────────────────────────────────────┘
```

**Keybindings:**
- `t` - Toggle TX/RX/Total display
- `p` - Toggle port display
- `n` - Toggle name resolution
- `q` - Quit

---

### 2. Per-Process Monitor: `nethogs`

**Install:**
```bash
sudo apt-get install nethogs
```

**Usage:**
```bash
# Monitor all network usage by process
sudo nethogs

# Monitor specific interface
sudo nethogs wlP1p1s0
```

**Output:**
```
NetHogs version 0.8.5

  PID USER     PROGRAM                    DEV    SENT    RECEIVED       
 1234 angelo   drone_web_controller       wlP1p1s0  6.2 MB/s  0.5 MB/s
 5678 angelo   /usr/bin/python3          wlP1p1s0  0.1 MB/s  0.2 MB/s
 
  TOTAL                                          6.3 MB/s  0.7 MB/s
```

---

### 3. Manual Check: `/proc/net/dev`

**Read raw statistics:**
```bash
cat /proc/net/dev | grep wlP1p1s0
```

**Output:**
```
wlP1p1s0: 1234567890  1000000    0    0    0     0          0         0  9876543210  800000    0    0    0     0       0          0
          └─ RX bytes  RX packets                                        └─ TX bytes  TX packets
```

**Calculate bandwidth over time:**
```bash
# Snapshot 1
cat /proc/net/dev | grep wlP1p1s0 | awk '{print $2, $10}'
# Wait 1 second
sleep 1
# Snapshot 2
cat /proc/net/dev | grep wlP1p1s0 | awk '{print $2, $10}'
# Calculate: (bytes2 - bytes1) * 8 / 1000000 = Mbps
```

---

### 4. Web UI Network Estimator (Built-in)

**Location:** System Tab → Network Status

**Formula:**
```javascript
FPS × Average Frame Size (75 KB) × 8 bits/byte ÷ 1,000,000 = Mbps
```

**Example:**
- 10 FPS × 75 KB × 8 = 6,000,000 bits/sec = **6.0 Mbps**

**Note:** This is an ESTIMATE. Actual usage may vary based on:
- Image complexity (more detail = larger JPEG)
- JPEG quality (currently 85)
- Network protocol overhead (HTTP headers, TCP/IP)
- Frame drops/retransmissions

---

## 🧪 Stress Testing Procedure

### Test 1: Baseline (No Livestream)
```bash
# Terminal 1: Monitor network
sudo iftop -i wlP1p1s0

# Terminal 2: Monitor CPU
htop

# Web UI: Start recording (SVO2, HD720@60fps)
# Observe: TX should be near 0 Mbps (no livestream)
# CPU: ~65-70%
```

### Test 2: Low FPS Livestream
```bash
# Keep monitoring tools running

# Web UI:
# 1. Enable livestream @ 2 FPS
# 2. Start recording
# Observe:
# - Network TX: ~1-2 Mbps
# - CPU: ~65-70% (no increase)
# - Web UI: Image updates smoothly
```

### Test 3: Medium FPS (5-7 Hz)
```bash
# Web UI: Change FPS to 5 Hz
# Observe:
# - Network TX: ~3-5 Mbps
# - CPU: ~65-70% (still no increase)
# - Latency: Check browser F12 console for snapshot request times
```

### Test 4: High FPS (10 Hz)
```bash
# Web UI: Change FPS to 10 Hz
# Observe:
# - Network TX: ~6-8 Mbps
# - Browser responsiveness: Should still be smooth
# - Check for frame drops: Console may show timeouts
```

### Test 5: Stress Test (15 Hz)
```bash
# Web UI: Change FPS to 15 Hz
# Expected behavior:
# - Network TX: ~9-12 Mbps
# - Possible frame drops if network saturated
# - Browser may lag on mobile devices
# - WiFi AP may throttle connection

# If unstable:
# - Reduce FPS to 10 Hz or lower
# - Check for other WiFi interference (2.4 GHz band)
```

---

## 🔍 Identifying Bottlenecks

### Symptom: Image Updates Slow/Frozen
**Possible Causes:**
1. Network saturation (WiFi can't keep up)
2. Browser rendering bottleneck (too fast for client)
3. Server overload (unlikely based on CPU tests)

**Debug:**
```bash
# Check network TX rate
sudo iftop -i wlP1p1s0
# If TX > 10 Mbps → network bottleneck

# Check browser console (F12)
# Look for:
# - "Request timeout" errors
# - Slow response times (>200ms per snapshot)
```

### Symptom: Recording Frame Drops
**Possible Causes:**
1. Recording is CPU bottleneck (65-70% baseline)
2. USB storage write speed (unlikely with NTFS/exFAT)
3. ZED SDK internal throttling (LOSSLESS mode limitation)

**Debug:**
```bash
# Check SVO file size growth
watch -n 1 'ls -lh /media/angelo/DRONE_DATA/flight_*/video.svo2'

# Expected: ~160-200 MB/min for HD720@60fps LOSSLESS
# If slower → check dmesg for USB errors
```

---

## 📝 Recommended Settings

### For Monitoring (Human Observation)
- **FPS:** 2-3 Hz
- **Bandwidth:** ~1-2 Mbps
- **CPU Impact:** Negligible
- **Use Case:** Check framing, lighting, composition

### For Inspection (Detail Analysis)
- **FPS:** 5-7 Hz
- **Bandwidth:** ~3-5 Mbps
- **CPU Impact:** Negligible
- **Use Case:** Object tracking, motion verification

### For Stress Testing
- **FPS:** 10-15 Hz
- **Bandwidth:** ~6-12 Mbps
- **CPU Impact:** Still ~5-10% (recording dominates)
- **Use Case:** Network capacity validation, future-proofing

---

## 🛡️ WiFi AP Stability Tips

### 1. Check WiFi Interface Status
```bash
# Show WiFi AP status
nmcli connection show DroneController

# Check for errors
sudo journalctl -u NetworkManager -n 50
```

### 2. Monitor Connected Clients
```bash
# List clients connected to AP
iw dev wlP1p1s0 station dump

# Or via NetworkManager
nmcli device show wlP1p1s0
```

### 3. Check for Interference (2.4 GHz)
```bash
# Scan for nearby networks
sudo iwlist wlP1p1s0 scan | grep -E '(ESSID|Channel|Quality)'

# If many networks on same channel:
# Consider changing AP channel in SafeHotspotManager
```

### 4. Increase WiFi Power (If Needed)
```bash
# Check current TX power
iw dev wlP1p1s0 info | grep txpower

# Set max power (if allowed)
sudo iw dev wlP1p1s0 set txpower fixed 3000  # 30 dBm = 1W
```

---

## 🎯 Expected Performance Matrix

| Scenario | CPU | Network TX | Stable? | Notes |
|----------|-----|------------|---------|-------|
| Recording only | 65-70% | 0 Mbps | ✅ | Baseline |
| Recording + 2 FPS | 65-70% | 1.2 Mbps | ✅ | Default |
| Recording + 5 FPS | 65-70% | 3.0 Mbps | ✅ | Smooth |
| Recording + 10 FPS | 65-75% | 6.0 Mbps | ✅ | High quality |
| Recording + 15 FPS | 70-80% | 9.0 Mbps | ⚠️ | May saturate |
| Livestream only 15 FPS | 10-15% | 9.0 Mbps | ✅ | No recording |

---

## 🔧 Advanced: Custom Bandwidth Monitor Script

**Create monitoring script:**
```bash
#!/bin/bash
# network_monitor.sh

INTERFACE="wlP1p1s0"
INTERVAL=1

echo "Monitoring $INTERFACE (Ctrl+C to stop)"
echo "Time      TX (Mbps)  RX (Mbps)"

PREV_TX=$(cat /proc/net/dev | grep $INTERFACE | awk '{print $10}')
PREV_RX=$(cat /proc/net/dev | grep $INTERFACE | awk '{print $2}')

while true; do
    sleep $INTERVAL
    
    CURR_TX=$(cat /proc/net/dev | grep $INTERFACE | awk '{print $10}')
    CURR_RX=$(cat /proc/net/dev | grep $INTERFACE | awk '{print $2}')
    
    TX_RATE=$(echo "scale=2; ($CURR_TX - $PREV_TX) * 8 / 1000000 / $INTERVAL" | bc)
    RX_RATE=$(echo "scale=2; ($CURR_RX - $PREV_RX) * 8 / 1000000 / $INTERVAL" | bc)
    
    echo "$(date +%H:%M:%S)  $TX_RATE       $RX_RATE"
    
    PREV_TX=$CURR_TX
    PREV_RX=$CURR_RX
done
```

**Usage:**
```bash
chmod +x network_monitor.sh
./network_monitor.sh
```

**Output:**
```
Monitoring wlP1p1s0 (Ctrl+C to stop)
Time      TX (Mbps)  RX (Mbps)
14:32:01  6.24       0.48
14:32:02  6.18       0.52
14:32:03  6.31       0.45
```

---

## 📊 Summary

**Key Insights:**
1. ✅ **CPU is NOT the bottleneck for livestream** (only 5-10% overhead)
2. ✅ **Recording dominates CPU** (65-70% for HD720@60fps LOSSLESS)
3. ✅ **Network bandwidth is the new limiting factor** (WiFi AP ~10-20 Mbps real-world)
4. ✅ **10 FPS livestream is feasible** (~6 Mbps, well within WiFi capacity)
5. ⚠️ **15+ FPS may cause network saturation** (>9 Mbps, approaching WiFi limits)

**Recommended Tools:**
- **Real-time monitoring:** `sudo iftop -i wlP1p1s0`
- **Per-process analysis:** `sudo nethogs wlP1p1s0`
- **Web UI estimate:** System Tab → Network Status

**Next Steps:**
- Test 10 FPS livestream during recording
- Monitor network TX with `iftop`
- Check for frame drops in browser console
- Consider adding backend API for real-time network stats
