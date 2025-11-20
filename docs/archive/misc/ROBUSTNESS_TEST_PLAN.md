# 🧪 DRONE SYSTEM ROBUSTNESS TEST PLAN
**Erstellt: November 17, 2024**
**Status: Ready to Execute**

---

## 🎯 TEST OBJECTIVES

1. **LCD Display Functionality** - Verify new LCD hardware works correctly
2. **Long-Duration Stability** - Ensure system stable over extended runtime
3. **Error Recovery** - Test graceful handling of failure scenarios
4. **Network Reliability** - Validate WiFi hotspot stability
5. **Resource Management** - Check for memory leaks and resource exhaustion

---

## 📋 TEST SUITE

### 1. LCD DISPLAY TESTS ✅ (Phase 1 - NOW)

#### Test 1.1: LCD Hardware Detection
```bash
# Verify I2C device present
ls -la /dev/i2c-7

# Scan for LCD address
sudo i2cdetect -y -r 7
# Expected: Device at 0x27
```
**Status**: ✅ PASSED (Address 0x27 detected)

#### Test 1.2: LCD Basic Communication
```bash
# Simple LCD test
./simple_lcd_test
# Expected: "Hello World" and test messages displayed
```
**Status**: ✅ PASSED (LCD responds correctly)

#### Test 1.3: LCD Integration with Drone Controller
```bash
# Start drone controller and watch LCD
drone

# Expected LCD Messages:
# 1. "Initialization" / "Complete!"
# 2. "Drone Ready" / "10.42.0.1:8080"
# 3. During recording: "Recording" / "Active"
# 4. On stop: "Recording" / "Stopped"
```
**Status**: ⏳ TO TEST

#### Test 1.4: LCD Status Updates
- [ ] Startup message appears
- [ ] IP address displayed correctly
- [ ] Recording status updates in real-time
- [ ] Error messages displayed (if triggered)
- [ ] Shutdown message appears

---

### 2. LONG-DURATION STABILITY TESTS (Phase 2)

#### Test 2.1: Extended Recording (30 Minutes)
```bash
# Modify auto-stop duration temporarily or use manual stop
# Start recording and let run for 30 minutes

# Metrics to Monitor:
# - File size growth (should be consistent)
# - Memory usage (check for leaks)
# - CPU temperature
# - Frame drops (should be 0)
# - Web UI responsiveness
```

**Acceptance Criteria**:
- ✅ Recording completes without crashes
- ✅ File size: ~50GB for 30min HD720@30fps
- ✅ Memory usage stable (no continuous growth)
- ✅ CPU temp < 85°C
- ✅ Zero frame drops
- ✅ Web UI remains responsive throughout

**Test Script**:
```bash
#!/bin/bash
# Test: Long Duration Recording

echo "Starting 30-minute recording test..."
START_TIME=$(date +%s)

# Start drone controller
drone &
sleep 30  # Wait for startup

# Start recording via curl
curl -X POST http://10.42.0.1:8080/api/start

# Monitor every 5 minutes
for i in {1..6}; do
    sleep 300  # 5 minutes
    
    ELAPSED=$((i * 5))
    echo "[$ELAPSED min] Checking system status..."
    
    # Memory usage
    MEM=$(free -m | awk 'NR==2{printf "%.2f%%", $3*100/$2}')
    echo "  Memory: $MEM"
    
    # Temperature
    TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
    TEMP_C=$((TEMP / 1000))
    echo "  CPU Temp: ${TEMP_C}°C"
    
    # File size
    LATEST=$(ls -t /media/angelo/DRONE_DATA*/flight_*/video.svo2 2>/dev/null | head -1)
    if [ -f "$LATEST" ]; then
        SIZE=$(du -h "$LATEST" | cut -f1)
        echo "  File Size: $SIZE"
    fi
done

echo "Test complete! Check results."
```

#### Test 2.2: Multiple Recording Cycles
```bash
# Test: Start/Stop Recording 20 times
# Verify no resource leaks or degradation

for i in {1..20}; do
    echo "Cycle $i/20"
    
    # Start recording
    curl -X POST http://10.42.0.1:8080/api/start
    sleep 60  # Record for 1 minute
    
    # Stop recording
    curl -X POST http://10.42.0.1:8080/api/stop
    sleep 10  # Wait between cycles
    
    # Check memory
    free -m
done
```

**Acceptance Criteria**:
- ✅ All 20 cycles complete successfully
- ✅ Memory returns to baseline after each cycle
- ✅ No "out of memory" errors
- ✅ Recording quality consistent across all cycles

#### Test 2.3: Overnight Stability (8 Hours)
```bash
# Start system and let run overnight
# No recording, just idle with web server running

# Monitor:
# - System remains responsive
# - Memory usage stable
# - No crashes or restarts
# - WiFi hotspot stays active
```

---

### 3. ERROR RECOVERY TESTS

#### Test 3.1: USB Disconnect/Reconnect During Recording
```bash
# Procedure:
# 1. Start recording
# 2. Wait 30 seconds
# 3. Physically disconnect USB storage
# 4. Observe system behavior
# 5. Reconnect USB storage
# 6. Verify recovery

# Expected Behavior:
# - Recording stops gracefully
# - Error message on web UI and LCD
# - No system crash
# - After reconnect: Can start new recording
```

**Acceptance Criteria**:
- ✅ No system crash on USB disconnect
- ✅ Error displayed to user (Web + LCD)
- ✅ Recording stops cleanly
- ✅ System recovers after reconnect

#### Test 3.2: Camera Disconnect During Recording
```bash
# Procedure:
# 1. Start recording
# 2. Wait 30 seconds
# 3. Disconnect ZED camera USB cable
# 4. Observe behavior

# Expected:
# - Recording stops
# - Error logged
# - No system crash
```

#### Test 3.3: Network Interruption
```bash
# Test WiFi hotspot resilience

# Procedure:
# 1. Start drone controller
# 2. Connect phone to DroneController WiFi
# 3. Start recording via web UI
# 4. Put phone in airplane mode for 30s
# 5. Disable airplane mode
# 6. Check if web UI reconnects

# Expected:
# - Recording continues despite client disconnect
# - Web UI reconnects automatically
# - Status updates resume
```

#### Test 3.4: Low Storage Space
```bash
# Test behavior when storage nearly full

# Procedure:
# 1. Use USB drive with ~5GB free space
# 2. Start recording (should create ~6.6GB file)
# 3. Monitor behavior

# Expected:
# - System detects low space
# - Warning message displayed
# - Recording stops before drive full
# - No corruption of filesystem
```

---

### 4. NETWORK RELIABILITY TESTS

#### Test 4.1: Hotspot Creation After Reboot
```bash
# Test: System reboot and auto-start

# Procedure:
# 1. Enable drone service: sudo systemctl enable drone-recorder
# 2. Reboot system: sudo reboot
# 3. Wait 2 minutes for boot + service start
# 4. Check WiFi hotspot: nmcli device status
# 5. Try connecting from phone

# Expected:
# - Hotspot active within 1 minute of boot
# - SSID "DroneController" visible
# - Connection successful
# - Web UI accessible at 10.42.0.1:8080
```

**Acceptance Criteria**:
- ✅ Hotspot active < 90 seconds after boot
- ✅ Phone can connect successfully
- ✅ Web UI loads correctly
- ✅ Original WiFi connection restored (Connecto Patronum)

#### Test 4.2: Multiple Client Connections
```bash
# Test: Multiple phones connected simultaneously

# Procedure:
# 1. Connect Phone 1 to DroneController
# 2. Connect Phone 2 to DroneController
# 3. Connect Laptop to DroneController
# 4. All access http://10.42.0.1:8080
# 5. Start recording from Phone 1
# 6. Monitor from other devices

# Expected:
# - All devices can connect
# - Status updates synchronized across clients
# - No connection drops
# - Recording controlled from any client
```

#### Test 4.3: Network State Preservation
```bash
# Test: SafeHotspotManager backup/restore

# Procedure:
# 1. Check autoconnect state: nmcli con show
# 2. Note which connections have autoconnect=yes
# 3. Start drone (creates hotspot)
# 4. Stop drone (teardown hotspot)
# 5. Check autoconnect state again

# Expected:
# - All original autoconnect states preserved
# - No permanent changes to WiFi config
# - Original WiFi connection restored
```

---

### 5. RESOURCE MANAGEMENT TESTS

#### Test 5.1: Memory Leak Detection
```bash
# Use valgrind to detect memory leaks

sudo valgrind --leak-check=full --show-leak-kinds=all \
    --track-origins=yes --verbose \
    --log-file=valgrind-drone.log \
    ./build/apps/drone_web_controller/drone_web_controller

# Run for 5 minutes, then stop
# Analyze valgrind-drone.log for leaks
```

**Acceptance Criteria**:
- ✅ Zero "definitely lost" bytes
- ✅ "Still reachable" bytes < 1MB
- ✅ No invalid memory access

#### Test 5.2: File Descriptor Leak Check
```bash
# Monitor open file descriptors

# Before starting:
PID=$(pgrep drone_web_controller)
BEFORE=$(ls /proc/$PID/fd | wc -l)

# Run for 30 minutes with multiple recording cycles

AFTER=$(ls /proc/$PID/fd | wc -l)
echo "FD count: $BEFORE -> $AFTER"
```

**Acceptance Criteria**:
- ✅ FD count stable or grows minimally (<10 FDs)
- ✅ No continuous FD growth

#### Test 5.3: CPU Usage Profiling
```bash
# Profile CPU usage during recording

# Install perf if needed
sudo apt install linux-tools-generic

# Profile for 60 seconds
sudo perf record -F 99 -p $(pgrep drone_web_controller) -g -- sleep 60
sudo perf report

# Check for hot spots
```

#### Test 5.4: Disk I/O Performance
```bash
# Monitor I/O during recording

sudo iotop -b -n 10 > iotop-recording.log

# Check:
# - Write speed to USB
# - I/O wait percentage
# - Identify bottlenecks
```

---

## 📊 TEST EXECUTION SCHEDULE

### Today (Nov 17):
- [x] **1.1**: LCD Hardware Detection ✅
- [x] **1.2**: LCD Basic Communication ✅
- [ ] **1.3**: LCD Integration Test
- [ ] **1.4**: LCD Status Updates
- [ ] **4.3**: Network State Preservation

### Tomorrow (Nov 18):
- [ ] **2.1**: Extended Recording (30 min)
- [ ] **2.2**: Multiple Recording Cycles (20x)
- [ ] **3.1**: USB Disconnect Recovery
- [ ] **3.2**: Camera Disconnect Recovery

### Day 3 (Nov 19):
- [ ] **2.3**: Overnight Stability Test
- [ ] **4.1**: Reboot Auto-Start Test
- [ ] **4.2**: Multiple Client Connections
- [ ] **3.3**: Network Interruption

### Day 4 (Nov 20):
- [ ] **5.1**: Memory Leak Detection
- [ ] **5.2**: File Descriptor Leak Check
- [ ] **5.3**: CPU Usage Profiling
- [ ] **5.4**: Disk I/O Performance
- [ ] **3.4**: Low Storage Space

---

## ✅ SUCCESS CRITERIA

System passes robustness testing if:

1. **Zero crashes** during all tests
2. **No resource leaks** (memory, FDs)
3. **Graceful error handling** for all failure scenarios
4. **Network stability** maintained across reboots
5. **LCD displays** correct status throughout
6. **Recording quality** consistent (no frame drops)
7. **Performance** acceptable (temp < 85°C, CPU < 80%)

---

## 📝 TEST RESULTS LOG

| Test ID | Date | Status | Notes |
|---------|------|--------|-------|
| 1.1 | Nov 17 | ✅ PASS | LCD at 0x27 detected |
| 1.2 | Nov 17 | ✅ PASS | Basic communication OK |
| 1.3 | Nov 17 | ⏳ PENDING | - |
| 1.4 | Nov 17 | ⏳ PENDING | - |

---

## 🚀 AFTER TESTING: STABLE RELEASE CHECKLIST

Once all tests pass:

- [ ] Update version to v1.4-stable
- [ ] Create comprehensive CHANGELOG.md
- [ ] Update README with test results
- [ ] Create GitHub Release with binaries
- [ ] Tag commit: `git tag v1.4-stable`
- [ ] Push tag: `git push origin v1.4-stable`
- [ ] Archive test results in `docs/testing/`
- [ ] Update PROJECT_OVERVIEW with "Production Tested" badge

---

**Next Steps**: Execute Test 1.3 (LCD Integration Test)
