# Timer-Based Stop Unification - v1.5.2

**Date:** November 18, 2025  
**Component:** drone_web_controller  
**Status:** ✅ Completed

## 📋 Problem Report

**User Report:** "Wenn man das Programm durchlaufen lässt (also nicht stoppt vor timerablauf) dann wird eine andere stop recording routine abgerufen als wenn man manuell stoppt. Die manuelle Stop Routine ist neuer und robuster, schau dass diese aufgerufen wird nachdem der Timer abgelaufen ist."

---

## 🔍 Root Cause Analysis

### Two Different Stop Paths

**Path 1: Manual Stop (Robust, Complete)**
- Triggered by: User clicks "Stop Recording" button → API call `/api/stop`
- Function: `stopRecording()` (Lines 308-374)
- Cleanup:
  - ✅ Stops DepthDataWriter (closes file, reports frame count)
  - ✅ Stops depth_viz_thread (joins thread safely)
  - ✅ Disables depth computation (for next recording)
  - ✅ Stops recorder (SVO2 or RAW)
  - ✅ Sets `recording_active_ = false`
  - ✅ Joins `recording_monitor_thread_`
  - ✅ Updates LCD: "Recording Stopped"
  - ✅ Sets `recording_stopped_time_` (for 3-second visible message)
  - ✅ State → IDLE (via systemMonitorLoop timer)

**Path 2: Timer-Based Stop (OLD, Incomplete)**
- Triggered by: `elapsed >= recording_duration_seconds_` in `recordingMonitorLoop()`
- Location: Lines 1002-1016 (OLD code)
- Cleanup:
  - ❌ NO DepthDataWriter stop (file left open!)
  - ❌ NO depth_viz_thread stop (thread keeps running!)
  - ❌ NO depth computation disable
  - ⚠️ Only stops recorder: `svo_recorder_->stopRecording()` or `raw_recorder_->stopRecording()`
  - ✅ Sets `recording_active_ = false`
  - ✅ Sets state to IDLE
  - ✅ Updates LCD: "Recording Completed"

**Critical Issues with Timer Path:**
1. **DepthDataWriter not closed** → File may be corrupted, frame count unknown
2. **depth_viz_thread keeps running** → Wasted CPU, potential access to released resources
3. **depth computation still enabled** → Next recording starts with wrong depth mode
4. **Inconsistent behavior** → User expects same cleanup regardless of stop method

---

## 💡 Solution Design

### Challenge: Deadlock Prevention

**Problem:**
- `recordingMonitorLoop()` runs in `recording_monitor_thread_`
- `stopRecording()` calls `recording_monitor_thread_->join()` to wait for thread exit
- **If we call `stopRecording()` from INSIDE `recordingMonitorLoop()` → DEADLOCK!**
  - Thread tries to join itself → blocks forever

**Solution: Flag-Based Handoff**

1. **Timer expires in `recordingMonitorLoop()`:**
   - Set flag: `timer_expired_ = true`
   - Exit loop (break)
   - Thread ends naturally

2. **Main web server loop checks flag:**
   - Every iteration: Check `if (timer_expired_ && recording_active_)`
   - If true: Call `stopRecording()` (from main thread, not monitor thread!)
   - Reset flag: `timer_expired_ = false`

3. **`stopRecording()` proceeds normally:**
   - Joins `recording_monitor_thread_` (already exited → instant join)
   - Performs all cleanup
   - No deadlock!

---

## 💻 Code Changes

### File: `apps/drone_web_controller/drone_web_controller.h`

**Change 1: Add Timer Expired Flag (Line ~119)**
```cpp
// State management
std::atomic<RecorderState> current_state_{RecorderState::IDLE};
std::atomic<bool> recording_active_{false};
std::atomic<bool> timer_expired_{false};  // NEW: Flag to signal timer reached duration
std::atomic<bool> hotspot_active_{false};
std::atomic<bool> web_server_running_{false};
std::atomic<bool> camera_initializing_{false};
```

**Why `atomic<bool>`:**
- Written by `recordingMonitorLoop()` thread
- Read by `webServerLoop()` thread
- Must be thread-safe (no mutex needed with atomic)

---

### File: `apps/drone_web_controller/drone_web_controller.cpp`

**Change 1: Timer Detection in recordingMonitorLoop (Lines ~1002-1010)**

**OLD (Incomplete Stop):**
```cpp
// Check if recording duration reached
if (elapsed >= recording_duration_seconds_) {
    std::cout << std::endl << "[WEB_CONTROLLER] Recording duration reached, stopping..." << std::endl;
    
    // Signal recording to stop without calling stopRecording() (avoids deadlock)
    recording_active_ = false;
    current_state_ = RecorderState::STOPPING;
    
    // Stop appropriate recorder
    if (recording_mode_ == RecordingModeType::SVO2 && svo_recorder_) {
        svo_recorder_->stopRecording();
    } else if (recording_mode_ == RecordingModeType::RAW_FRAMES && raw_recorder_) {
        raw_recorder_->stopRecording();
    }
    current_state_ = RecorderState::IDLE;
    updateLCD("Recording", "Completed");
    break;
}
```

**NEW (Flag-Based Handoff):**
```cpp
// Check if recording duration reached
if (elapsed >= recording_duration_seconds_) {
    std::cout << std::endl << "[WEB_CONTROLLER] Recording duration reached, setting timer_expired flag..." << std::endl;
    
    // Set flag to trigger stopRecording() from main thread (prevents deadlock)
    timer_expired_ = true;
    
    // Exit loop - main thread will call stopRecording() when it sees timer_expired_
    break;
}
```

**Key Differences:**
- **OLD:** Tried to stop recording from within monitor thread
- **NEW:** Just sets flag and exits cleanly
- **Result:** Monitor thread ends naturally, ready to be joined

---

**Change 2: Flag Check in webServerLoop (Lines ~1104-1113)**

**OLD:**
```cpp
while (web_server_running_) {
    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (activity <= 0) continue;
```

**NEW:**
```cpp
while (web_server_running_) {
    // Check if recording timer expired (must be checked in main thread to avoid deadlock)
    if (timer_expired_ && recording_active_) {
        std::cout << "[WEB_CONTROLLER] Timer expired detected, calling robust stopRecording()..." << std::endl;
        timer_expired_ = false;  // Reset flag
        stopRecording();  // Use robust stop routine with all cleanup
    }
    
    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (activity <= 0) continue;
```

**Why This Works:**
- `webServerLoop()` runs in `web_server_thread_` (separate from monitor thread)
- Can safely call `stopRecording()` which joins `recording_monitor_thread_`
- No self-join deadlock!

---

## 🔄 Execution Flow

### Scenario A: Manual Stop (No Change)

```
1. User clicks "Stop Recording" button
   ↓
2. Browser: POST /api/stop
   ↓
3. handleClientRequest() → stopRecording()
   ↓
4. stopRecording():
   - Sets current_state_ = STOPPING
   - Stops DepthDataWriter (if active)
   - Stops depth_viz_thread (if active)
   - Stops recorder (SVO2/RAW)
   - Sets recording_active_ = false
   - recordingMonitorLoop sees STOPPING state → breaks
   - Joins recording_monitor_thread_ ✅
   - Updates LCD: "Recording Stopped"
   - Sets recording_stopped_time_
   ↓
5. Recording fully stopped with complete cleanup
```

---

### Scenario B: Timer-Based Stop (NEW, Fixed)

```
1. recordingMonitorLoop() checks elapsed time
   ↓
2. elapsed >= recording_duration_seconds_
   ↓
3. recordingMonitorLoop():
   - Prints: "Recording duration reached, setting timer_expired flag..."
   - Sets timer_expired_ = true
   - Breaks from loop
   - Thread ends naturally
   ↓
4. webServerLoop() (next iteration, max 1 second later):
   - Checks: timer_expired_ && recording_active_
   - TRUE → Prints: "Timer expired detected, calling robust stopRecording()..."
   - Resets timer_expired_ = false
   - Calls stopRecording()
   ↓
5. stopRecording():
   - Sets current_state_ = STOPPING (monitor thread already exited)
   - Stops DepthDataWriter ✅ (NEW!)
   - Stops depth_viz_thread ✅ (NEW!)
   - Disables depth computation ✅ (NEW!)
   - Stops recorder
   - Sets recording_active_ = false
   - Joins recording_monitor_thread_ (instant, thread already exited)
   - Updates LCD: "Recording Stopped"
   - Sets recording_stopped_time_ ✅ (NEW!)
   ↓
6. Recording fully stopped with COMPLETE cleanup (matches manual stop!)
```

---

## 🧪 Testing Protocol

### Test 1: Manual Stop (Verify No Regression)

**Steps:**
1. Start recording: Set duration = 60 seconds
2. Wait 10 seconds
3. Click "Stop Recording"
4. Check console output

**Expected Console:**
```
[WEB_CONTROLLER] Stopping recording...
[WEB_CONTROLLER] Stopping DepthDataWriter... (if SVO2+Depth mode)
[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: XXX
[WEB_CONTROLLER] Stopping depth visualization thread... (if depth images mode)
[WEB_CONTROLLER] Depth visualization thread stopped
[WEB_CONTROLLER] Recording stopped
```

**Expected LCD:**
1. "Stopping / Recording..."
2. "Recording / Stopped" (visible 3 seconds)
3. "Web Controller / 10.42.0.1:8080"

**Expected State:**
- recording_active_ = false
- current_state_ = IDLE (after 3 seconds)
- All threads stopped
- Files closed properly

---

### Test 2: Timer-Based Stop (Main Fix)

**Steps:**
1. Start recording: Set duration = 10 seconds
2. Wait full 10 seconds (do NOT click stop)
3. Check console output

**Expected Console:**
```
[WEB_CONTROLLER] Recording duration reached, setting timer_expired flag...
[WEB_CONTROLLER] Timer expired detected, calling robust stopRecording()...
[WEB_CONTROLLER] Stopping recording...
[WEB_CONTROLLER] Stopping DepthDataWriter... (if SVO2+Depth mode)
[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: XXX
[WEB_CONTROLLER] Stopping depth visualization thread... (if depth images mode)
[WEB_CONTROLLER] Depth visualization thread stopped
[WEB_CONTROLLER] Recording stopped
```

**Expected LCD:**
1. "Rec: 10/10s" (last update during recording)
2. "Stopping / Recording..."
3. "Recording / Stopped" (visible 3 seconds)
4. "Web Controller / 10.42.0.1:8080"

**Critical Checks:**
- ✅ DepthDataWriter stopped (if used)
- ✅ depth_viz_thread stopped (if used)
- ✅ Depth computation disabled
- ✅ LCD shows "Recording Stopped" (not "Recording Completed")
- ✅ 3-second visible "Stopped" message (same as manual stop)
- ✅ No deadlock (program continues running)

---

### Test 3: SVO2 + Depth Info with Timer Stop

**Steps:**
1. Select "SVO2 + Depth Info (Fast, 32-bit raw)"
2. Set duration = 15 seconds
3. Start recording
4. Wait full 15 seconds
5. Check flight folder

**Expected Files:**
```
flight_YYYYMMDD_HHMMSS/
├── video.svo2          (SVO2 file)
├── depth_data.bin      (Depth data, properly closed)
├── sensor_data.csv     (Sensor log)
└── recording.log       (Recording metadata)
```

**Expected Console:**
```
[WEB_CONTROLLER] Stopping DepthDataWriter...
[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: ~450 (15s * 30fps)
```

**Critical:**
- `depth_data.bin` file size > 0 (not corrupted)
- Frame count reported in console
- File closed properly (no truncation)

---

### Test 4: SVO2 + Depth Images with Timer Stop

**Steps:**
1. Select "SVO2 + Depth Images (PNG)"
2. Set depth FPS = 10
3. Set duration = 20 seconds
4. Start recording
5. Wait full 20 seconds
6. Check flight folder

**Expected Files:**
```
flight_YYYYMMDD_HHMMSS/
├── video.svo2
├── depth_images/
│   ├── depth_0000.png
│   ├── depth_0001.png
│   ├── ...
│   └── depth_0199.png  (~200 images: 20s * 10fps)
├── sensor_data.csv
└── recording.log
```

**Expected Console:**
```
[WEB_CONTROLLER] Stopping depth visualization thread...
[WEB_CONTROLLER] Depth visualization thread stopped
```

**Critical:**
- All depth PNGs saved (no missing frames)
- depth_viz_thread properly stopped (no dangling thread)
- CPU usage drops to idle after stop (thread not spinning)

---

### Test 5: Sequential Recordings (Verify Clean State)

**Steps:**
1. Recording #1: SVO2 + Depth Info, 10 seconds, let timer expire
2. Wait 5 seconds
3. Recording #2: SVO2 only, 10 seconds, let timer expire
4. Check both recordings

**Expected:**
- Recording #1: Has depth data ✅
- Recording #2: NO depth data ✅ (depth auto-disabled for SVO2 only)
- Console shows depth mode auto-switch: NONE for recording #2
- No errors about "depth map not computed"

**Critical:**
- Depth computation properly disabled between recordings
- State fully reset after timer-based stop
- No leftover threads or file handles

---

## 📊 Comparison: Before vs After

| Aspect | Manual Stop (OLD) | Timer Stop (OLD) | Timer Stop (NEW) |
|--------|-------------------|------------------|------------------|
| **DepthDataWriter** | ✅ Stopped | ❌ Not stopped | ✅ Stopped |
| **depth_viz_thread** | ✅ Stopped | ❌ Not stopped | ✅ Stopped |
| **Depth computation** | ✅ Disabled | ❌ Still enabled | ✅ Disabled |
| **LCD Message** | "Recording Stopped" | "Recording Completed" | "Recording Stopped" |
| **recording_stopped_time_** | ✅ Set | ❌ Not set | ✅ Set |
| **3-second visible** | ✅ Yes | ❌ No | ✅ Yes |
| **State transition** | STOPPING → IDLE | Direct to IDLE | STOPPING → IDLE |
| **Thread join** | ✅ Safe | ✅ Safe (no join) | ✅ Safe |
| **Deadlock risk** | ❌ None | ❌ None | ❌ None |
| **Code path** | stopRecording() | Inline code | stopRecording() |

**Summary:**
- **OLD Timer:** 6 missing features, inconsistent behavior
- **NEW Timer:** 0 missing features, identical to manual stop

---

## 🎓 Lessons Learned

### 1. One Robust Code Path > Multiple Partial Paths

**Problem:** Maintaining two different stop routines
- Manual stop: Full cleanup
- Timer stop: Partial cleanup
- **Risk:** Features added to one path, forgotten in the other

**Solution:** Single source of truth
- One `stopRecording()` function with ALL cleanup
- All stop triggers use same function
- **Benefit:** New cleanup code automatically applies to all stop methods

---

### 2. Thread Self-Join is Always Deadlock

**Problem:** Calling `thread.join()` from inside the thread
```cpp
// INSIDE recordingMonitorLoop() thread:
stopRecording();  // Tries to join recordingMonitorLoop thread → DEADLOCK!
```

**Solution:** Flag-based handoff
```cpp
// INSIDE monitor thread:
timer_expired_ = true;  // Set flag
break;                   // Exit naturally

// OUTSIDE monitor thread (main loop):
if (timer_expired_) {
    stopRecording();  // Safe join (thread already exited)
}
```

---

### 3. Atomic Flags for Simple Thread Communication

**Why `std::atomic<bool>`:**
- Simple flag: true/false
- Written by one thread, read by another
- No complex data structure
- **Perfect for:** "Event happened, handle it in main thread"

**Don't Need:**
- `std::mutex` (atomic handles memory ordering)
- `std::condition_variable` (no waiting, just polling)
- Complex synchronization

---

### 4. Select() Timeout Determines Max Latency

**Current Implementation:**
```cpp
timeout.tv_sec = 1;  // 1-second timeout on select()
```

**Impact:**
- Timer expires in monitor thread
- Main loop checks flag next iteration
- **Max delay:** 1 second (when select() is blocking)

**Good Enough Because:**
- Recording just finished (user not waiting for immediate action)
- 1-second cleanup delay unnoticeable
- Simpler than event-based wakeup

**Alternative (if needed):**
```cpp
timeout.tv_sec = 0;
timeout.tv_usec = 100000;  // 100ms timeout → 100ms max latency
```

---

### 5. Identical Behavior Builds User Trust

**User Expectation:**
"Recording should stop the same way, regardless of how it stops"

**OLD Behavior:**
- Manual stop: All cleanup, "Recording Stopped", 3-second visible
- Timer stop: Partial cleanup, "Recording Completed", instant hide
- **User:** "Why does it behave differently?"

**NEW Behavior:**
- Both paths: Identical cleanup, same message, same timing
- **User:** "Predictable and reliable"

---

## 🚦 Success Criteria

- [x] Code compiles without errors
- [x] `timer_expired_` flag added to header
- [x] recordingMonitorLoop sets flag and exits cleanly
- [x] webServerLoop checks flag and calls stopRecording()
- [x] Manual stop behavior unchanged (no regression)
- [ ] Timer-based stop performs all cleanup (DepthDataWriter, threads, etc.)
- [ ] No deadlock when timer expires
- [ ] LCD shows "Recording Stopped" for both manual and timer stop
- [ ] 3-second visible message for both stop methods
- [ ] Sequential recordings work correctly (clean state between recordings)

---

## 🔍 Troubleshooting

### Issue: Deadlock when timer expires

**Check console for:**
```
[WEB_CONTROLLER] Recording duration reached, setting timer_expired flag...
```

**If missing:**
- recordingMonitorLoop never reached timer check
- Recording may have been manually stopped first

**If present but hangs:**
```cpp
// Check that flag is being read:
std::cout << "DEBUG: Checking timer_expired=" << timer_expired_ << std::endl;
```

**If flag never triggers stopRecording():**
- Verify `web_server_running_` is true (loop is running)
- Verify `select()` timeout allows frequent checks

---

### Issue: DepthDataWriter still not stopped

**Check console for:**
```
[WEB_CONTROLLER] Timer expired detected, calling robust stopRecording()...
[WEB_CONTROLLER] Stopping DepthDataWriter...
```

**If "Stopping DepthDataWriter" missing:**
- Verify `depth_data_writer_` is not null
- Check that recording mode is SVO2_DEPTH_INFO

**If frame count = 0:**
- DepthDataWriter may not have written any frames (separate issue)

---

### Issue: depth_viz_thread still running after stop

**Check CPU usage:**
```bash
top -p $(pgrep drone_web_controller)
# Should drop to ~5% after recording stops
```

**If high CPU (40%+) after stop:**
```cpp
// Add debug logging:
std::cout << "DEBUG: depth_viz_running_ = " << depth_viz_running_ << std::endl;
std::cout << "DEBUG: depth_viz_thread joinable = " << (depth_viz_thread_ && depth_viz_thread_->joinable()) << std::endl;
```

---

### Issue: "Recording Completed" still shows instead of "Recording Stopped"

**Cause:** Old code path still executing (build didn't pick up changes)

**Solution:**
```bash
# Clean build
rm -rf build/
./scripts/build.sh

# Or force rebuild of specific file
touch apps/drone_web_controller/drone_web_controller.cpp
./scripts/build.sh
```

---

## 📚 Related Documentation

- `docs/WEB_DISCONNECT_FIX_v1.3.4.md` - Thread interaction patterns (break vs continue)
- `docs/DEPTH_MODE_AUTO_MANAGEMENT_v1.5.2.md` - Depth computation management
- `docs/CRITICAL_LEARNINGS_v1.3.md` - Thread deadlock prevention patterns
- `RELEASE_v1.3_STABLE.md` - System architecture and thread model

---

**Version:** v1.5.2  
**Changelog:** Unified stop recording routine for timer and manual stop  
**Status:** Ready for Testing ✅
