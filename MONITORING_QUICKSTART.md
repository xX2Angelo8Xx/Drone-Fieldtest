# Quick Network & System Monitoring - Drone Controller

**Date:** 2024-11-18  
**Tools installed:** ✅ iftop, htop, nethogs

---

## 🚀 Quick Start

### 1. Monitor Network Bandwidth (iftop)

**Basic usage:**
```bash
sudo iftop -i wlP1p1s0
```

**What you'll see:**
```
┌────────────────────────────────────────────────────────┐
│ jetson.local => 192.168.4.2    ████████████ 6.5Mb     │
│                             <=                0.8Mb     │
│                                                         │
│ TX: 6.5 Mbps   RX: 0.8 Mbps   TOTAL: 7.3 Mbps        │
└────────────────────────────────────────────────────────┘
```

**Key columns:**
- **Left side:** Your Jetson sending TO client (TX = Upload)
- **Right side:** Client sending TO Jetson (RX = Download)
- **Bottom:** Total bandwidth usage

**Useful keybindings:**
- `t` - Toggle display mode (2s/10s/40s average)
- `h` - Toggle help screen
- `p` - Toggle port display
- `n` - Toggle DNS resolution (off = faster)
- `q` - Quit

**Pro tip:** Press `n` to disable DNS lookups for faster display!

---

### 2. Monitor CPU & Memory (htop)

**Basic usage:**
```bash
htop
```

**What you'll see:**
```
  1 [||||||||||||||||||  85%]   Tasks: 245, 612 thr; 2 running
  2 [|||||             45%]   Load average: 1.45 0.89 0.67
  3 [|||||||||||       65%]   Uptime: 02:34:12
  4 [||||              38%]
  Mem[||||||||||||||3.8G/7.5G]   CPU: 65%  (drone_web_controller)
  Swp[|        512M/8.0G]

  PID USER      RES  %CPU %MEM   TIME+ Command
 1234 angelo  256M  65.0  3.3  12:34.56 drone_web_controller
 5678 angelo   80M   8.2  1.0   1:23.45 /usr/local/zed/tools/ZED_Explorer
```

**Key info:**
- **Top bars:** Per-core CPU usage (Orin Nano has 6 cores)
- **Mem bar:** RAM usage (8GB total on Orin Nano)
- **Process list:** Sorted by CPU usage
- **%CPU:** Can exceed 100% (multi-threaded processes)

**Useful keybindings:**
- `F2` - Setup (change display options)
- `F3` - Search for process
- `F4` - Filter by name
- `F5` - Tree view (show process hierarchy)
- `F9` - Kill process (use with caution!)
- `q` - Quit

**Finding drone_web_controller:**
- Press `F4`, type `drone`, Enter
- Should show your running process

---

### 3. Monitor Per-Process Network Usage (nethogs)

**Basic usage:**
```bash
sudo nethogs wlP1p1s0
```

**What you'll see:**
```
NetHogs version 0.8.6

  PID USER     PROGRAM                    DEV      SENT    RECEIVED       
 1234 angelo   drone_web_controller       wlP1p1s0  6.2 MB/s   0.5 MB/s
 5678 angelo   firefox                    wlP1p1s0  0.1 MB/s   1.2 MB/s
 
  TOTAL                                            6.3 MB/s   1.7 MB/s
```

**Key info:**
- **SENT:** Upload speed (Jetson → Client)
- **RECEIVED:** Download speed (Client → Jetson)
- Shows which process is using bandwidth

**Useful keybindings:**
- `m` - Toggle units (KB/s, MB/s, GB/s)
- `r` - Sort by received traffic
- `s` - Sort by sent traffic
- `q` - Quit

---

## 🧪 Testing Workflow

### Terminal Setup (3 terminals recommended)

**Terminal 1: Network Monitor**
```bash
sudo iftop -i wlP1p1s0
```

**Terminal 2: CPU Monitor**
```bash
htop
```

**Terminal 3: Controller**
```bash
cd ~/Projects/Drone-Fieldtest
sudo ./build/apps/drone_web_controller/drone_web_controller
```

---

## 📊 Expected Values

### Baseline (No Recording, No Livestream)
```
htop:   CPU 5-10%
iftop:  TX 0 Mbps, RX 0.1 Mbps (status API polling)
```

### Recording Only (SVO2 @ HD720/60fps)
```
htop:   CPU 65-70%  ⚠️ BOTTLENECK
iftop:  TX 0 Mbps (no livestream)
```

### Livestream Only @ 5 FPS (No Recording)
```
htop:   CPU 5-10%   ✅ Very light
iftop:  TX 3.0 Mbps (JPEG snapshots to client)
```

### Recording + Livestream @ 5 FPS
```
htop:   CPU 65-70%  (recording dominates, +0-5% for livestream)
iftop:  TX 3.0 Mbps (livestream traffic)
```

### Recording + Livestream @ 10 FPS
```
htop:   CPU 70-75%  (+5% for higher FPS)
iftop:  TX 6.0 Mbps (doubled snapshot rate)
```

### Recording + Livestream @ 15 FPS (Stress Test)
```
htop:   CPU 75-80%
iftop:  TX 9.0 Mbps  ⚠️ Approaching WiFi AP limit (~10-20 Mbps)
```

---

## 🎯 What to Look For

### Signs of Network Saturation
```
iftop:  TX consistently >10 Mbps
Browser: Slow image updates, timeouts
Console: "Request timeout" errors in F12
```

### Signs of CPU Bottleneck
```
htop:   CPU >90% sustained
Recording: Frame drops reported
ZED SDK: Grab errors in console
```

### Healthy Operation
```
htop:   CPU 65-75% (recording + livestream)
iftop:  TX stable, matches expected FPS bandwidth
Browser: Smooth image updates at selected FPS
```

---

## 🔧 Troubleshooting

### iftop shows no traffic
**Problem:** Wrong interface name

**Solution:**
```bash
# List all network interfaces
ip link show

# Common Jetson Orin interfaces:
# - wlP1p1s0  (WiFi)
# - enpXXX    (Ethernet)
# - wlan0     (alternative WiFi name)

# Try alternative:
sudo iftop -i wlan0
```

### htop shows CPU >100%
**Not a problem!** Multi-threaded processes show combined CPU usage.

Example: `drone_web_controller` with 4 threads at 30% each = 120% total.

### nethogs shows no data
**Solution:** Make sure livestream is active!
```bash
# Network traffic only appears when:
# 1. Livestream checkbox is enabled
# 2. Client browser is connected
# 3. Recording is running (writes to USB, not network)
```

---

## 📝 Quick Test Checklist

### Before Starting Test
- [ ] 3 terminals open (iftop, htop, controller)
- [ ] WiFi client connected (laptop/phone)
- [ ] Browser at http://192.168.4.1:8080
- [ ] USB drive mounted (/media/angelo/DRONE_DATA)

### Test Sequence
1. [ ] **Baseline:** No recording, no livestream
   - `htop`: CPU ~5-10%
   - `iftop`: TX ~0 Mbps

2. [ ] **Recording only:** Start recording (SVO2)
   - `htop`: CPU ~65-70%
   - `iftop`: TX ~0 Mbps (no livestream yet)

3. [ ] **Add livestream @ 2 FPS:** Enable checkbox
   - `htop`: CPU ~65-70% (no increase!)
   - `iftop`: TX ~1.2 Mbps

4. [ ] **Increase to 5 FPS:** Change dropdown
   - `htop`: CPU ~65-70%
   - `iftop`: TX ~3.0 Mbps

5. [ ] **Stress test @ 10 FPS:**
   - `htop`: CPU ~70-75%
   - `iftop`: TX ~6.0 Mbps
   - Browser: Image updates smoothly?

6. [ ] **Network limit @ 15 FPS:**
   - `htop`: CPU ~75-80%
   - `iftop`: TX ~9.0 Mbps
   - Look for: Timeouts, frame drops

---

## 🚀 Advanced Tips

### Save iftop output to file
```bash
# Capture network stats for 60 seconds
sudo iftop -i wlP1p1s0 -t -s 60 > network_stats.txt
```

### Filter iftop by port
```bash
# Only show HTTP traffic (port 8080)
sudo iftop -i wlP1p1s0 -f "port 8080"
```

### Run htop in batch mode (logging)
```bash
# Log CPU usage every 2 seconds
htop -d 20 > cpu_log.txt
# (Press Ctrl+C after test)
```

### Monitor specific process with top
```bash
# Watch drone_web_controller CPU usage
top -p $(pidof drone_web_controller)
```

---

## 📊 Alternative: Built-in System Monitor

If you prefer a GUI, Jetson has a built-in system monitor:

```bash
gnome-system-monitor &
```

**Or find it in:** Applications → System → System Monitor

---

## 🎯 Quick Commands Reference

```bash
# Network bandwidth (WiFi AP)
sudo iftop -i wlP1p1s0

# CPU and memory
htop

# Per-process network usage
sudo nethogs wlP1p1s0

# Check WiFi interface name
ip link show | grep wl

# Find drone_web_controller process
ps aux | grep drone_web_controller

# Kill stuck process (if needed)
sudo killall drone_web_controller

# Check WiFi AP status
nmcli connection show DroneController

# List connected WiFi clients
iw dev wlP1p1s0 station dump
```

---

## ✅ You're Ready!

**Next steps:**
1. Open 3 terminals as shown above
2. Start monitoring tools
3. Launch drone_web_controller
4. Open browser to http://192.168.4.1:8080
5. Run through test sequence
6. Report findings! 🎉

**Happy monitoring!** 📊
