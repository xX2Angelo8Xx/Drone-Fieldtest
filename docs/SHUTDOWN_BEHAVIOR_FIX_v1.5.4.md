# Shutdown Behavior Fix (v1.5.4 continued)

**Date:** November 18, 2025  
**Issues Fixed:**
1. Ctrl+C should stop application without shutting down Jetson
2. Shutdown flag checked too early, interrupting recording cleanup

## Issue 1: Ctrl+C Shuts Down System

### Problem
```bash
# User runs application
$ drone

# User presses Ctrl+C to stop
^C
[MAIN] Shutdown signal received, performing cleanup...
[MAIN] Executing system shutdown...
# ❌ Jetson powers off!  (User just wanted to close app)
```

**Root Cause:** Signal handler (SIGINT/SIGTERM) sets `shutdown_requested_ = true`, which triggers `system("sudo shutdown -h now")` in main loop.

### Solution
Add separate flags for different shutdown intents:

```cpp
// drone_web_controller.h
std::atomic<bool> shutdown_requested_{false};       // Stop application (Ctrl+C)
std::atomic<bool> system_shutdown_requested_{false}; // Power off Jetson (GUI button)
```

**Implementation:**

```cpp
// Signal handler (Ctrl+C, SIGTERM) - Stop app only
void DroneWebController::signalHandler(int signal) {
    if (instance_) {
        std::cout << "Received signal " << signal << ", stopping application..." << std::endl;
        instance_->shutdown_requested_ = true;  // App stop, Jetson stays on
    }
}

// GUI shutdown button - Power off system
if (request.find("POST /api/shutdown") != std::string::npos) {
    std::cout << "System shutdown requested via GUI" << std::endl;
    system_shutdown_requested_ = true;  // Power off system
    shutdown_requested_ = true;          // Also stop app
    return;
}

// Main loop - Check which type of shutdown
while (!controller.isShutdownRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::cout << "Shutdown signal received, performing cleanup..." << std::endl;

if (controller.isSystemShutdownRequested()) {
    std::cout << "System shutdown requested - powering off Jetson..." << std::endl;
    system("sudo shutdown -h now");
} else {
    std::cout << "Application stopped - Jetson remains running" << std::endl;
}
```

### Results

**Before:**
| Action | Result |
|--------|--------|
| Ctrl+C | ❌ Powers off Jetson |
| GUI shutdown button | ✅ Powers off Jetson |
| `systemctl stop` | ❌ Powers off Jetson |

**After:**
| Action | Result |
|--------|--------|
| Ctrl+C | ✅ Stops app, Jetson stays on |
| GUI shutdown button | ✅ Powers off Jetson |
| `systemctl stop` | ✅ Stops app, Jetson stays on |

## Issue 2: Shutdown Interrupts Recording Cleanup

### Problem
```bash
# User clicks GUI shutdown while recording
[WEB_CONTROLLER] System shutdown requested
[WEB_CONTROLLER] Stopping recording...
[WEB_CONTROLLER] Stopping DepthDataWriter...  # ← Interrupted here!
# ❌ Recording monitor loop exits because shutdown_requested_ == true
# ❌ stopRecording() doesn't complete
# ❌ Files corrupted or incomplete
```

**Timeline:**
1. GUI shutdown button clicked → `shutdown_requested_ = true`
2. `handleShutdown()` calls `stopRecording()`
3. `stopRecording()` starts cleanup...
4. **Recording monitor loop checks `shutdown_requested_`** → exits early!
5. Cleanup incomplete → corrupted recording

**Root Cause:** Recording monitor loop checks `shutdown_requested_` in condition:
```cpp
// ❌ BAD - exits loop when shutdown requested, even during cleanup
while (recording_active_ && !shutdown_requested_) {
    // ... recording monitoring ...
}
```

### Solution
Remove `shutdown_requested_` check from recording monitor loop. Let recording complete before shutdown proceeds. Shutdown already waits for `recording_stop_complete_` flag anyway!

```cpp
// ✅ GOOD - only checks recording_active_ flag
// Shutdown will wait for recording_stop_complete_ flag
while (recording_active_) {
    if (current_state_ == RecorderState::STOPPING) {
        break;  // Exit when stopping
    }
    // ... recording monitoring ...
}
```

### Flow Comparison

**Before (Broken):**
```
1. User clicks shutdown
   ↓
2. shutdown_requested_ = true
   ↓
3. handleShutdown() calls stopRecording()
   ↓
4. stopRecording() sets recording_active_ = false
   ↓
5. Recording monitor loop checks:
   while (recording_active_ && !shutdown_requested_)
   ↓
6. ❌ Loop exits immediately (shutdown_requested_ is true)
   ↓
7. ❌ DepthDataWriter cleanup not finished
   ↓
8. ❌ Thread joins not complete
   ↓
9. ❌ Corrupted recording
```

**After (Fixed):**
```
1. User clicks shutdown
   ↓
2. shutdown_requested_ = true
   ↓
3. handleShutdown() calls stopRecording()
   ↓
4. stopRecording() sets recording_active_ = false
   ↓
5. Recording monitor loop checks:
   while (recording_active_)  ← Only checks this!
   ↓
6. ✓ Loop continues until recording_active_ becomes false
   ↓
7. ✓ DepthDataWriter cleanup completes
   ↓
8. ✓ All threads joined
   ↓
9. ✓ recording_stop_complete_ = true
   ↓
10. ✓ handleShutdown() proceeds after waiting for flag
    ↓
11. ✓ Recording saved successfully
```

## Testing

### Test 1: Ctrl+C Stops App (Not System)
```bash
# Start application
$ sudo /home/angelo/Projects/Drone-Fieldtest/scripts/start_drone.sh

# Press Ctrl+C
^C

# Expected output:
[WEB_CONTROLLER] Received signal 2, stopping application...
[MAIN] Shutdown signal received, performing cleanup...
[WEB_CONTROLLER] Initiating shutdown sequence...
[WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓
[MAIN] Application stopped - Jetson remains running  ← Jetson stays on!

# Verify Jetson is still running
$ uname -a
Linux AeroLock ...  ← Still responsive
```

### Test 2: GUI Shutdown Powers Off
```bash
# Start application
$ sudo /home/angelo/Projects/Drone-Fieldtest/scripts/start_drone.sh

# Connect to GUI: http://192.168.4.1:8080
# Click "System Herunterfahren" button

# Expected output:
[WEB_CONTROLLER] System shutdown requested via GUI
[MAIN] Shutdown signal received, performing cleanup...
[WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓
[MAIN] System shutdown requested - powering off Jetson...  ← Powers off!
# System powers down
```

### Test 3: Shutdown During Recording (Complete Cleanup)
```bash
# Start application and recording
# Wait 30 seconds
# Click GUI shutdown button

# Expected log:
[WEB_CONTROLLER] System shutdown requested via GUI
[WEB_CONTROLLER] Initiating shutdown sequence...
[WEB_CONTROLLER] Active recording detected - performing graceful stop...
[LCD] Update: Saving / Recording...
[WEB_CONTROLLER] Stopping recording...
[WEB_CONTROLLER] Stopping DepthDataWriter...  ← NOT interrupted!
[DEPTH_DATA] Stopped. Total frames: 123
[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: 123
[ZED] Recording stopped
[WEB_CONTROLLER] Recording stopped
[WEB_CONTROLLER] State transitioned to IDLE  ← Completes fully!
[WEB_CONTROLLER] Waiting for recording cleanup to complete...
[WEB_CONTROLLER] ✓ Recording cleanup completed in 850ms
[WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓
[MAIN] System shutdown requested - powering off Jetson...

# Verify recording is complete:
$ ls -lh /media/angelo/DRONE_DATA/flight_*/
-rw-r--r-- 1 angelo angelo 1.5G ... video.svo2  ← Complete file!
```

## Files Modified

1. **apps/drone_web_controller/drone_web_controller.h**
   - Line 173-174: Added `system_shutdown_requested_` flag
   - Line 78: Added `isSystemShutdownRequested()` accessor

2. **apps/drone_web_controller/drone_web_controller.cpp**
   - Line 1005: Removed `!shutdown_requested_` from recording monitor loop
   - Line 1451: GUI shutdown sets both flags
   - Line 2144: Signal handler only sets app shutdown flag

3. **apps/drone_web_controller/main.cpp**
   - Lines 37-52: Check both flags, only power off if system shutdown requested

## Summary

### Flag Meanings

| Flag | Set By | Purpose | Main Loop Action |
|------|--------|---------|------------------|
| `shutdown_requested_` | Ctrl+C, systemctl stop, GUI button | Stop application | Exit loop, cleanup, DON'T power off |
| `system_shutdown_requested_` | GUI shutdown button only | Power off Jetson | Exit loop, cleanup, power off |

### Shutdown Behavior Matrix

| Trigger | App Stops | Jetson Powers Off | Recording Saved |
|---------|-----------|-------------------|-----------------|
| **Ctrl+C** | ✅ Yes | ❌ No | ✅ Yes |
| **systemctl stop** | ✅ Yes | ❌ No | ✅ Yes |
| **GUI shutdown button** | ✅ Yes | ✅ Yes | ✅ Yes |
| **SIGTERM** | ✅ Yes | ❌ No | ✅ Yes |

### Key Principles

1. **Separate intent:** App stop vs system power off are different actions
2. **Don't interrupt recording:** Never check `shutdown_requested_` during cleanup
3. **Wait for completion:** Use `recording_stop_complete_` flag, not loop conditions
4. **User expectation:** Ctrl+C should stop app, not power off system

---

**Status:** ✅ FIXED  
**Version:** v1.5.4  
**Testing:** Both issues verified and working correctly
