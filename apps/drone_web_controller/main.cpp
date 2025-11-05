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
    std::cout << "[MAIN] 🌐 Web Interface: http://192.168.4.1:8080" << std::endl;
    std::cout << "[MAIN] 📱 Connect your phone to the WiFi and open the web interface" << std::endl;
    std::cout << "[MAIN] Press Ctrl+C to shutdown gracefully" << std::endl;
    
    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Check if shutdown was requested
        // (The signal handler in DroneWebController will handle shutdown)
    }
    
    return 0;
}