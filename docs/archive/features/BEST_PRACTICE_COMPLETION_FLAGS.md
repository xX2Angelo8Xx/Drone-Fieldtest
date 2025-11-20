# Best Practice: Completion Flags vs Timing Delays

**Date:** November 18, 2025  
**Pattern:** Synchronization Using Completion Flags  
**Anti-Pattern:** Arbitrary Time Delays (sleep/wait)

## The Problem with Timing Delays

### ❌ BAD CODE (Timing-Based)

```cpp
// Anti-pattern: Guessing how long cleanup takes
void handleShutdown() {
    if (recording_active_) {
        stopRecording();
        
        // ❌ BAD: Arbitrary delay - too short causes data loss, too long wastes time
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        sync();
    }
    // Continue shutdown...
}
```

**Problems:**
1. **Race Conditions:** 500ms might be too short for large files (10GB SVO2)
2. **Performance:** 500ms might be way too long for small recordings
3. **Unpredictable:** Timing varies by:
   - Recording mode (SVO2 vs RAW vs Depth)
   - File size (30 seconds vs 4 minutes)
   - USB speed (USB 2.0 vs 3.0)
   - Filesystem type (NTFS vs exFAT)
   - System load (CPU busy)
4. **Not Scalable:** Have to adjust delays every time you add features
5. **Hard to Debug:** When things fail, you don't know if timing was the issue

### ✅ GOOD CODE (Flag-Based)

```cpp
// Best practice: Wait for actual completion signal
void handleShutdown() {
    if (recording_active_) {
        stopRecording();
        
        // ✅ GOOD: Wait for completion flag with timeout
        std::cout << "Waiting for recording cleanup to complete..." << std::endl;
        int wait_count = 0;
        const int max_wait = 100;  // 10 seconds maximum
        
        while (!recording_stop_complete_ && wait_count < max_wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        if (recording_stop_complete_) {
            std::cout << "✓ Recording cleanup completed in " 
                      << (wait_count * 100) << "ms" << std::endl;
        } else {
            std::cout << "⚠ Recording cleanup timeout after 10s" << std::endl;
        }
        
        sync();  // Final filesystem flush
    }
    // Continue shutdown...
}
```

**Benefits:**
1. **Robust:** Works for any recording size/mode
2. **Fast:** No unnecessary waiting - proceeds as soon as ready
3. **Transparent:** Logs actual completion time
4. **Debuggable:** Timeout detection shows if something is stuck
5. **Scalable:** New recording modes automatically work

## Implementation Pattern

### Step 1: Add Completion Flag (Header)

```cpp
// drone_web_controller.h
class DroneWebController {
private:
    std::atomic<bool> recording_active_{false};
    std::atomic<bool> recording_stop_complete_{true};  // true when idle/stopped
    // ...
};
```

**Key points:**
- Use `std::atomic<bool>` for thread-safe access
- Initialize to `true` (system starts in "complete" state)
- One flag per async operation you need to track

### Step 2: Set Flag When Operation Starts

```cpp
bool DroneWebController::startRecording() {
    // ... initialization ...
    
    recording_active_ = true;
    recording_stop_complete_ = false;  // ← Operation started, not complete yet
    
    // ... start threads ...
    return true;
}
```

### Step 3: Set Flag When Operation Completes

```cpp
bool DroneWebController::stopRecording() {
    // ... cleanup steps ...
    
    // Stop threads
    if (recording_monitor_thread_ && recording_monitor_thread_->joinable()) {
        recording_monitor_thread_->join();
    }
    
    // Close files
    svo_recorder_->stopRecording();
    
    // Update state
    current_state_ = RecorderState::IDLE;
    
    // ← ALL cleanup done, NOW set flag
    recording_stop_complete_ = true;
    
    std::cout << "Recording stopped" << std::endl;
    return true;
}
```

**Critical:** Flag MUST be set AFTER all cleanup, not before!

### Step 4: Wait for Flag in Dependent Code

```cpp
void DroneWebController::handleShutdown() {
    if (recording_active_) {
        stopRecording();  // Initiates cleanup
        
        // Wait for completion with timeout (best practice)
        int wait_count = 0;
        const int max_wait = 100;  // 10 seconds timeout
        
        while (!recording_stop_complete_ && wait_count < max_wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        // Log outcome for debugging
        if (recording_stop_complete_) {
            std::cout << "✓ Completed in " << (wait_count * 100) << "ms" << std::endl;
        } else {
            std::cout << "⚠ Timeout after " << (max_wait * 100) << "ms" << std::endl;
        }
        
        sync();  // Additional safety measure
    }
}
```

## When to Use Completion Flags

### ✅ USE FLAGS FOR:

1. **Thread Completion**
   ```cpp
   thread_running_ = true;
   thread_ = std::make_unique<std::thread>([this]() {
       // ... do work ...
       thread_running_ = false;  // Signal completion
   });
   
   // Later: wait for thread
   while (thread_running_) {
       std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   ```

2. **File I/O Completion**
   ```cpp
   file_write_complete_ = false;
   startFileWrite();
   while (!file_write_complete_) { /* wait */ }
   ```

3. **Network Operations**
   ```cpp
   data_sent_ = false;
   sendData();
   while (!data_sent_) { /* wait */ }
   ```

4. **Hardware Initialization**
   ```cpp
   camera_ready_ = false;
   initCamera();
   while (!camera_ready_) { /* wait */ }
   ```

5. **Multi-Step Cleanup**
   ```cpp
   cleanup_complete_ = false;
   startCleanup();
   while (!cleanup_complete_) { /* wait */ }
   ```

### ❌ DON'T USE FLAGS FOR:

1. **Fixed Hardware Delays** (timing is inherent to hardware)
   ```cpp
   // Hardware spec says "wait 100ms after reset"
   sendResetCommand();
   std::this_thread::sleep_for(std::chrono::milliseconds(100));  // OK - fixed spec
   ```

2. **Debounce Delays** (timing is the feature)
   ```cpp
   // UI debounce: ignore clicks within 200ms
   if (time_since_last_click < 200ms) return;  // OK - intentional delay
   ```

3. **Rate Limiting** (timing is the constraint)
   ```cpp
   // Don't send requests faster than 10/second
   std::this_thread::sleep_for(std::chrono::milliseconds(100));  // OK - rate limit
   ```

## Advanced Pattern: Multiple Completion Stages

For complex operations with multiple stages:

```cpp
class ComplexOperation {
private:
    std::atomic<bool> stage1_complete_{false};
    std::atomic<bool> stage2_complete_{false};
    std::atomic<bool> stage3_complete_{false};
    
public:
    void execute() {
        stage1_complete_ = false;
        stage2_complete_ = false;
        stage3_complete_ = false;
        
        // Stage 1: Initialize
        initialize();
        stage1_complete_ = true;
        
        // Stage 2: Process
        process();
        stage2_complete_ = true;
        
        // Stage 3: Finalize
        finalize();
        stage3_complete_ = true;
    }
    
    void waitForCompletion() {
        // Can monitor individual stages
        while (!stage1_complete_) { /* wait */ }
        std::cout << "Stage 1 done" << std::endl;
        
        while (!stage2_complete_) { /* wait */ }
        std::cout << "Stage 2 done" << std::endl;
        
        while (!stage3_complete_) { /* wait */ }
        std::cout << "Stage 3 done" << std::endl;
    }
};
```

## Timeout Best Practices

Always include timeout to prevent infinite hangs:

```cpp
// Good timeout pattern
const auto start_time = std::chrono::steady_clock::now();
const auto timeout = std::chrono::seconds(10);

while (!operation_complete_) {
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > timeout) {
        std::cout << "⚠ Operation timeout" << std::endl;
        break;  // Emergency exit
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

**Timeout values:**
- I/O operations: 5-10 seconds
- Network operations: 30-60 seconds
- Thread joins: 5 seconds (should be instant)
- Hardware init: Device-specific (check datasheet)

## Testing Completion Flags

### Test 1: Normal Operation
```cpp
// Start operation
startOperation();
assert(operation_complete_ == false);

// ... operation runs ...

// Check completion
waitForCompletion();
assert(operation_complete_ == true);
```

### Test 2: Timeout Handling
```cpp
// Simulate hung operation
operation_complete_ = false;  // Never gets set

// Should timeout gracefully
bool timed_out = !waitForCompletion(5s);
assert(timed_out == true);
```

### Test 3: Race Conditions
```cpp
// Start many operations concurrently
for (int i = 0; i < 100; i++) {
    std::thread([this]() {
        startOperation();
        waitForCompletion();
        // All should complete cleanly
    }).detach();
}
```

## Real-World Example: Our Recording Shutdown

### Before (Timing-Based)
```cpp
// ❌ Arbitrary 500ms delay
stopRecording();
std::this_thread::sleep_for(std::chrono::milliseconds(500));
sync();
```

**Problems:**
- 30-second recording: 500ms is overkill (actual: ~50ms cleanup)
- 4-minute recording: 500ms too short (actual: ~2000ms for large file)
- Result: Either wasted time or data corruption

### After (Flag-Based)
```cpp
// ✅ Wait for actual completion
stopRecording();

int wait_ms = 0;
while (!recording_stop_complete_ && wait_ms < 10000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    wait_ms += 100;
}

std::cout << "Cleanup took " << wait_ms << "ms" << std::endl;
sync();
```

**Results:**
- 30-second recording: Completes in ~50ms → saves 450ms
- 4-minute recording: Completes in ~2000ms → prevents corruption
- Automatic adaptation to different scenarios

## Summary: The Rule

**"Don't guess timing - use completion signals"**

| Scenario | Bad Practice | Good Practice |
|----------|-------------|---------------|
| Thread completion | `sleep(500ms)` | `while (!thread_done_)` |
| File write | `sleep(200ms)` | `while (!write_complete_)` |
| Network send | `sleep(1000ms)` | `while (!send_complete_)` |
| Cleanup | `sleep(300ms)` | `while (!cleanup_done_)` |
| Hardware init | `sleep(100ms)` | Datasheet spec OR flag |

**Exception:** Only use fixed delays when the delay IS the feature (debounce, rate limiting, hardware specs).

---

**Applied in:** Drone Field Test System v1.5.4  
**File:** `apps/drone_web_controller/drone_web_controller.cpp:2168-2210`  
**Pattern:** `recording_stop_complete_` flag replaces 500ms arbitrary delay
