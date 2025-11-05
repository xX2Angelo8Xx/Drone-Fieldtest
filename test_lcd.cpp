#include <iostream>
#include "lcd_handler.h"
#include <chrono>
#include <thread>

int main() {
    std::cout << "Testing LCD Display..." << std::endl;
    
    LCDHandler lcd;
    
    if (!lcd.init()) {
        std::cout << "LCD initialization failed! Checking I2C connection..." << std::endl;
        std::cout << "Make sure LCD is connected to /dev/i2c-7 with address 0x27" << std::endl;
        return 1;
    }
    
    std::cout << "LCD initialized successfully!" << std::endl;
    
    // Test basic display
    lcd.displayMessage("LCD Test", "Working!");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test startup messages
    lcd.showStartupMessage();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Test different states
    lcd.displayMessage("Autostart", "Started!");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    lcd.displayMessage("Starting WiFi", "Hotspot...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    lcd.displayMessage("Web Server", "Ready!");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    lcd.displayMessage("Status", "IDLE");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    lcd.clear();
    lcd.displayMessage("LCD Test", "Complete!");
    
    std::cout << "LCD test completed!" << std::endl;
    return 0;
}