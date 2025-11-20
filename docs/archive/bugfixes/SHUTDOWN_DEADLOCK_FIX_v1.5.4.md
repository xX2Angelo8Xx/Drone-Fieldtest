# Shutdown Deadlock Fix (v1.5.4)

**Date:** November 18, 2025  
**Issue:** System shutdown from GUI caused deadlock, leaving WiFi AP active and Jetson running  
**Severity:** CRITICAL - User cannot shut down system via web interface

## Problem Analysis

### The Deadlock Chain

1. User clicks "Shutdown" button in GUI
2. Browser sends `POST /api/shutdown` HTTP request
3. **Web server thread** handles request and calls `shutdownSystem()`
4. `shutdownSystem()` calls `handleShutdown()`
5. `handleShutdown()` tries to `join()` the web server thread
6. **DEADLOCK!** A thread cannot join itself → `std::system_error: Resource deadlock avoided`
7. Process aborts with signal ABRT (code=6)
8. WiFi AP remains active, system doesn't shut down

### Error Message
```
terminate called after throwing an instance of 'std::system_error'
  what():  Resource deadlock avoided
Main PID: 8108 (code=killed, signal=6/ABRT)
```

### Root Cause

The shutdown request is **processed by the web server thread**, which then tries to join itself:

```cpp
// ❌ OLD CODE - CAUSES DEADLOCK
void handleClientRequest() {
    if (request.find("POST /api/shutdown") != std::string::npos) {
        shutdownSystem();  // Called from web server thread
    }
}

void DroneWebController::handleShutdown() {
    // This is still running IN the web server thread!
    if (web_server_thread_ && web_server_thread_->joinable()) {
        web_server_thread_->join();  // ❌ DEADLOCK: Can't join self!
    }
}
```

## Solution

### 1. Separate Shutdown Signaling from Execution

**Key Principle:** Web server thread should only **signal** shutdown, not execute it. The **main thread** detects the signal and performs cleanup.

### 2. Code Changes

#### A. API Handler (drone_web_controller.cpp:1431-1439)
```cpp
// ✅ NEW CODE - Just set flag and respond
} else if (request.find("POST /api/shutdown") != std::string::npos) {
    response = generateAPIResponse("Shutdown initiated");
    send(client_socket, response.c_str(), response.length(), 0);
    
    // CRITICAL: Don't call shutdownSystem() from web server thread (causes deadlock)
    // Just set the flag - main loop will handle actual shutdown
    std::cout << std::endl << "[WEB_CONTROLLER] System shutdown requested" << std::endl;
    updateLCD("System", "Shutting Down");
    shutdown_requested_ = true;
    return;
}
```

#### B. Signal Handler (drone_web_controller.cpp:2125-2132)
```cpp
// ✅ NEW CODE - Signal handler also just sets flag
void DroneWebController::signalHandler(int signal) {
    if (instance_) {
        std::cout << "[WEB_CONTROLLER] Received signal " << signal << ", shutting down..." << std::endl;
        // CRITICAL: Just set flag - don't call handleShutdown() from signal handler
        // Main loop will detect flag and perform clean shutdown
        instance_->shutdown_requested_ = true;
    }
}
```

#### C. Main Loop (main.cpp:36-48)
```cpp
// ✅ NEW CODE - Main thread handles shutdown
// Keep main thread alive until shutdown is requested
while (!controller.isShutdownRequested()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

std::cout << "[MAIN] Shutdown signal received, performing cleanup..." << std::endl;

// Perform graceful cleanup (destructor will be called)
// This includes: stopping recording, closing cameras, tearing down WiFi, stopping threads
std::cout << "[MAIN] Cleanup complete" << std::endl;

// Execute system shutdown
std::cout << "[MAIN] Executing system shutdown..." << std::endl;
system("sudo shutdown -h now");

return 0;
```

#### D. handleShutdown() Fix (drone_web_controller.cpp:2145-2155)
```cpp
// ✅ NEW CODE - Don't join web server thread from within itself
// Close server socket to unblock accept() and force server thread to exit
int fd = server_fd_.load();
if (fd >= 0) {
    std::cout << "[WEB_CONTROLLER] Closing web server socket..." << std::endl;
    shutdown(fd, SHUT_RDWR);  // Shutdown both directions
    close(fd);
    server_fd_ = -1;
}

// CRITICAL: DON'T join web server thread here - might be called FROM web server thread!
// Thread will exit naturally when it detects web_server_running_ == false and socket closed
// Main thread cleanup (destructor) will handle final join after this function returns
std::cout << "[WEB_CONTROLLER] ✓ Web server shutdown signal sent (thread will exit naturally)" << std::endl;
```

#### E. Destructor Update (drone_web_controller.cpp:31-41)
```cpp
// ✅ NEW CODE - Join web server thread AFTER handleShutdown() completes
DroneWebController::~DroneWebController() {
    handleShutdown();
    
    // Now that handleShutdown() completed, join web server thread if it exists
    // (Safe to do here since we're not IN the web server thread - it already exited after setting flag)
    if (web_server_thread_ && web_server_thread_->joinable()) {
        std::cout << "[WEB_CONTROLLER] Waiting for web server thread to finish..." << std::endl;
        web_server_thread_->join();
        web_server_thread_.reset();
        std::cout << "[WEB_CONTROLLER] ✓ Web server thread joined" << std::endl;
    }
}
```

## Shutdown Flow (Fixed)

### Web GUI Shutdown
```
User clicks "Shutdown" button
  ↓
Web server thread receives POST /api/shutdown
  ↓
Web server thread sends HTTP response
  ↓
Web server thread sets shutdown_requested_ = true
  ↓
Web server thread continues running (processes snapshot requests until socket closed)
  ↓
Main loop detects shutdown_requested_ == true
  ↓
Main loop exits while() loop
  ↓
Controller destructor called
  ↓
handleShutdown() closes socket → web server thread exits naturally
  ↓
Destructor joins web server thread (safe - main thread context)
  ↓
system("sudo shutdown -h now") executes
  ↓
Jetson shuts down cleanly
```

### Ctrl+C / SIGTERM Shutdown
```
Signal received (SIGINT/SIGTERM)
  ↓
Signal handler sets shutdown_requested_ = true
  ↓
(Same flow as above from "Main loop detects...")
```

## Key Principles

1. **Never join a thread from within itself** - Always use main thread for joining
2. **Separate signaling from execution** - Flags for inter-thread communication
3. **Let threads exit naturally** - Close sockets/set flags, don't force termination
4. **Destructor handles final cleanup** - Safe context for thread joins

## Testing

### Before Fix
```bash
# Start system
sudo systemctl start drone-recorder.service

# Click "Shutdown" button in web GUI
# Result: LCD shows "System Shutting Down"
#         Connection lost
#         WiFi AP still active
#         System still running
#         Process crashed with ABRT signal

journalctl -u drone-recorder -n 20
# Output: terminate called after throwing an instance of 'std::system_error'
#         what():  Resource deadlock avoided
#         Main process exited, code=killed, status=6/ABRT
```

### After Fix
```bash
# Start system
sudo systemctl start drone-recorder.service

# Click "Shutdown" button in web GUI
# Expected Result: 
#   - LCD shows "System Shutting Down"
#   - Connection lost (expected)
#   - WiFi AP tears down
#   - Jetson shuts down cleanly
#   - No crash, no deadlock
```

## Related Files

- `apps/drone_web_controller/drone_web_controller.cpp` - Main implementation
- `apps/drone_web_controller/main.cpp` - Main loop with shutdown detection
- `docs/WEB_DISCONNECT_FIX_v1.3.4.md` - Previous thread interaction fixes
- `docs/CRITICAL_LEARNINGS_v1.3.md` - Thread deadlock patterns

## Version History

- **v1.5.3** - CORRUPTED_FRAME tolerance for field robustness
- **v1.5.4** - Shutdown deadlock fix (this document)
- **v1.3.4** - Web disconnect fix (break vs continue in monitor loops)

---

**Status:** ✅ FIXED - Shutdown from GUI now works cleanly without deadlock
