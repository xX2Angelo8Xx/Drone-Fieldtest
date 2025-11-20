#include <iostream>
#include <chrono>
#include <thread>
#include "drone_web_controller.h"

int main() {
    std::cout << "🚁 DRONE WEB CONTROLLER STARTING 🚁" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // Create controller instance
    DroneWebController controller;
    
    // Initialize all systems
    if (!controller.initialize()) {
        std::cout << "[MAIN] ERROR: Initialization failed" << std::endl;
        return 1;
    }
    
    // Start WiFi hotspot
    if (!controller.startHotspot()) {
        std::cout << "[MAIN] ERROR: Failed to start WiFi hotspot" << std::endl;
        return 1;
    }
    
    // Start web server
    controller.startWebServer(8080);
    
    std::cout << "[MAIN] ✅ Drone Web Controller is ready!" << std::endl;
    std::cout << "[MAIN] 📶 WiFi Network: DroneController" << std::endl;
    std::cout << "[MAIN] 🔐 Password: drone123" << std::endl;
    std::cout << "[MAIN] 🌐 Web Interface: http://10.42.0.1:8080" << std::endl;
    std::cout << "[MAIN] 📱 Connect your phone to the WiFi and open the web interface" << std::endl;
    std::cout << "[MAIN] Press Ctrl+C to stop application gracefully (Jetson stays on)" << std::endl;
    std::cout << "[MAIN] Use GUI shutdown button to power off Jetson" << std::endl;
    
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
    
    // Perform graceful cleanup (destructor will be called)
    // This includes: closing cameras, tearing down WiFi, stopping threads
    std::cout << "[MAIN] Cleanup complete" << std::endl;
    
    // Check if system shutdown was requested (GUI button) or just app stop (Ctrl+C)
    if (controller.isSystemShutdownRequested()) {
        std::cout << "[MAIN] System shutdown requested - powering off Jetson..." << std::endl;
        
        // CRITICAL: Update LCD right before system shutdown to ensure message persists
        // LCD is powered by external 5V, so message will remain visible after Jetson powers off
        // Must be done AFTER cleanup to avoid being overwritten
        if (controller.isBatteryShutdown()) {
            controller.updateLCD("Battery Shutdown", "System Off");
        } else {
            controller.updateLCD("User Shutdown", "System Off");
        }
        
        // Give LCD time to complete I2C write before power-off
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        system("sudo shutdown -h now");
    } else {
        std::cout << "[MAIN] Application stopped - Jetson remains running" << std::endl;
    }
    
    return 0;
}