# Main Loop Recording Stop Fix v1.5.4

**Date:** 2025-11-18  
**Version:** v1.5.4  
**Type:** Critical Fix - System Shutdown Timing

## Problem Description

System shutdown was executing **before** recording stopped, causing:
- Black screen after ~1 second (system powered off immediately)
- Only 1 line of console output visible
- Recording files potentially corrupted (no time for ZED buffer flush)
- Expected 5+ second stop sequence never completed

### Root Cause

The shutdown command ran **before the destructor**, which meant `handleShutdown()` never executed:

```cpp
// OLD FLOW (BROKEN):
// 1. Web UI shutdown button clicked
if (request.find("POST /api/shutdown")) {
    system_shutdown_requested_ = true;  // Flag set
}

// 2. Main loop exits immediately
while (!controller.isShutdownRequested()) { }  // Exits instantly!

// 3. Main checks flag and powers off IMMEDIATELY
if (controller.isSystemShutdownRequested()) {
    system("sudo shutdown -h now");  // ❌ RUNS BEFORE DESTRUCTOR!
}
// System powers off here - recording still active!

// 4. Destructor NEVER RUNS (system already shutting down)
~DroneWebController() {
    handleShutdown();  // Never called - too late!
    stopRecording();   // Never called - recording file corrupted!
}
```

**Timeline:**
1. Shutdown flag set → main loop exits (0ms)
2. `shutdown -h now` executes (50ms)
3. System powers off (1000ms)
4. Recording never stopped, ZED buffers never flushed ❌

## Solution

**Move recording stop into main loop BEFORE system shutdown command:**

```cpp
// NEW FLOW (FIXED):
// 1. Main loop exits when shutdown requested
while (!controller.isShutdownRequested()) { }

std::cout << "[MAIN] Shutdown signal received, stopping services..." << std::endl;

// 2. CRITICAL: Stop recording FIRST (while still in main loop)
if (controller.isRecording()) {
    std::cout << "[MAIN] Active recording detected - waiting for complete stop..." << std::endl;
    
    // Stop recording and wait for complete cleanup
    controller.stopRecording();
    
    // Wait for recording_stop_complete flag with timeout
    int wait_count = 0;
    const int max_wait = 100;  // 10 seconds max
    while (!controller.isRecordingStopComplete() && wait_count < max_wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    
    if (controller.isRecordingStopComplete()) {
        std::cout << "[MAIN] ✓ Recording stopped completely in " 
                  << (wait_count * 100) << "ms" << std::endl;
    }
    
    // Final filesystem sync
    sync();
    std::cout << "[MAIN] ✓ Filesystem synced" << std::endl;
}

// 3. THEN check if system shutdown is needed
if (controller.isSystemShutdownRequested()) {
    std::cout << "[MAIN] System shutdown requested - powering off Jetson..." << std::endl;
    system("sudo shutdown -h now");  // ✓ Recording already stopped!
}
```

**New Timeline:**
1. Shutdown flag set → main loop exits (0ms)
2. Recording stop initiated (0ms)
3. Wait for ZED buffers flush (2000-5000ms depending on file size)
4. State transitions to IDLE (in stopRecording())
5. Completion flag set
6. Filesystem sync (200ms)
7. **THEN** `shutdown -h now` executes (50ms)
8. System powers off cleanly with saved recording ✅

## Implementation Details

### File: `apps/drone_web_controller/main.cpp`

**Added recording stop sequence before system shutdown check:**

```cpp
// Keep main thread alive until shutdown is requested
while (!controller.isShutdownRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::cout << "[MAIN] Shutdown signal received, stopping services..." << std::endl;

// CRITICAL: Stop recording FIRST before system shutdown
if (controller.isRecording()) {
    std::cout << "[MAIN] Active recording detected - waiting for complete stop..." << std::endl;
    
    // Stop recording and wait for complete cleanup
    controller.stopRecording();
    
    // Wait for recording_stop_complete flag with timeout
    int wait_count = 0;
    const int max_wait = 100;  // 10 seconds max
    while (!controller.isRecordingStopComplete() && wait_count < max_wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    
    if (controller.isRecordingStopComplete()) {
        std::cout << "[MAIN] ✓ Recording stopped completely in " 
                  << (wait_count * 100) << "ms" << std::endl;
    } else {
        std::cout << "[MAIN] ⚠ Warning: Recording stop timeout after 10s" << std::endl;
    }
    
    // Final filesystem sync
    sync();
    std::cout << "[MAIN] ✓ Filesystem synced" << std::endl;
}

std::cout << "[MAIN] Performing cleanup..." << std::endl;
// ... destructor cleanup ...

// Check if system shutdown was requested (GUI button) or just app stop (Ctrl+C)
if (controller.isSystemShutdownRequested()) {
    std::cout << "[MAIN] System shutdown requested - powering off Jetson..." << std::endl;
    system("sudo shutdown -h now");
} else {
    std::cout << "[MAIN] Application stopped - Jetson remains running" << std::endl;
}
```

### File: `apps/drone_web_controller/drone_web_controller.h`

**Added accessor method for completion flag:**

```cpp
bool isRecordingStopComplete() const { return recording_stop_complete_; }
```

### File: `apps/drone_web_controller/drone_web_controller.cpp`

**Updated `handleShutdown()` comment to reflect that main handles recording:**

```cpp
// NOTE: If recording was already stopped by main.cpp before system shutdown, this is a no-op
if (recording_active_) {
    std::cout << "[WEB_CONTROLLER] Active recording detected - performing graceful stop..." << std::endl;
    // ... existing logic ...
} else {
    std::cout << "[WEB_CONTROLLER] No active recording to stop (already stopped or never started)" << std::endl;
}
```

## Expected Console Output

### Before Fix (Broken)
```
[MAIN] System shutdown requested - powering off Jetson...
[BLACK SCREEN after ~1 second]
```

### After Fix (Working)
```
[MAIN] Shutdown signal received, stopping services...
[MAIN] Active recording detected - waiting for complete stop...
[WEB_CONTROLLER] Stopping recording...
[ZED] Stopping recording - setting flag...
[ZED] Waiting for recording thread to finish...
[ZED] Disabling ZED recording...
[ZED] Large file pre-shutdown sync...
[ZED] Waiting 3000ms for ZED buffer flush (file size: 3456MB)...
[ZED] Final filesystem sync...
[ZED] Recording disabled.
[WEB_CONTROLLER] Recording monitor thread joined
[WEB_CONTROLLER] State transitioned to IDLE
[WEB_CONTROLLER] Recording stopped
[MAIN] ✓ Recording stopped completely in 4200ms
[MAIN] ✓ Filesystem synced
[MAIN] Performing cleanup...
[MAIN] System shutdown requested - powering off Jetson...
[BLACK SCREEN - but recording is saved!]
```

**Key Difference:** 
- **Before:** 1 line, ~1 second
- **After:** 10+ lines, 4-6 seconds (proper ZED buffer flush)

## Testing Checklist

### Test 1: Shutdown During Active Recording
```bash
1. Start recording via web UI
2. Wait 2-3 minutes (large file)
3. Click "Shutdown System" button
4. Verify console shows:
   ✓ "[MAIN] Active recording detected..."
   ✓ "[ZED] Waiting Xms for ZED buffer flush..."
   ✓ "[MAIN] ✓ Recording stopped completely in Xms"
   ✓ "[MAIN] ✓ Filesystem synced"
   ✓ "[MAIN] System shutdown requested - powering off..."
5. Verify recording file is intact and playable
6. Verify process takes 4-6 seconds (not 1 second)
```

### Test 2: Shutdown While Idle
```bash
1. System idle (no recording)
2. Click "Shutdown System" button
3. Verify:
   ✓ "[MAIN] No active recording..."
   ✓ Shutdown proceeds immediately
   ✓ ~1 second shutdown time (no recording to stop)
```

### Test 3: Ctrl+C During Recording
```bash
1. Start recording
2. Press Ctrl+C
3. Verify:
   ✓ Recording stops (same output as Test 1)
   ✓ System STAYS ON (not powered off)
   ✓ Recording file intact
```

## Why This Works

1. **main.cpp has full control** - Handles recording stop before shutdown command
2. **Completion flag ensures timing** - Waits for actual stop, not guessing
3. **Destructor still works** - For Ctrl+C or other exit paths
4. **System shutdown delayed** - Only executes after recording completely saved

## Performance Impact

**Before Fix:**
- Shutdown during recording: ~1 second (file corrupted)
- ZED buffer flush: Never happened
- Recording files: Potentially unplayable

**After Fix:**
- Shutdown during recording: 4-6 seconds (file saved)
- ZED buffer flush: Complete (size-dependent wait)
- Recording files: Always intact ✅

**Trade-off:** Shutdown takes longer, but data integrity preserved

## Related Fixes

This completes the v1.5.4 shutdown trilogy:

1. **Shutdown Deadlock Fix** - Can't join thread from itself (flag-based signaling)
2. **Dual Shutdown Flags** - Ctrl+C vs GUI shutdown button
3. **State Transition Timing** - Synchronous IDLE transition in stopRecording()
4. **Main Loop Recording Stop (THIS FIX)** - Stop recording before system shutdown command

## Technical Notes

### Why Not Use atexit() Handler?

```cpp
// Considered but rejected:
atexit([]() {
    // Won't work - system shutdown kills process before this runs
    stopRecording();
});
```

`atexit()` handlers run during normal process exit, but `shutdown -h now` terminates the process forcefully before handlers execute. The **only** reliable place is in the main loop before the shutdown command.

### Why Not Block in Signal Handler?

```cpp
// Considered but rejected:
void signalHandler(int signal) {
    stopRecording();  // ❌ Signal handlers must be non-blocking!
    shutdown_requested_ = true;
}
```

Signal handlers have strict requirements (no I/O, no blocking calls). The main loop pattern is the correct approach.

## Documentation Updates

Updated files:
- `docs/MAIN_LOOP_RECORDING_FIX_v1.5.4.md` (this file)
- `docs/v1.5.4_COMPLETE_SUMMARY.md` (will be updated to include this fix)

---

**Status:** ✅ FIXED  
**Merged:** apps/drone_web_controller/main.cpp, drone_web_controller.h, drone_web_controller.cpp  
**Impact:** Critical - Prevents recording corruption on system shutdown  
**Next Steps:** Field test shutdown during large file recording (>4GB)
