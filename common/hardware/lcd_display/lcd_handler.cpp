#include "lcd_handler.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>  // For std::this_thread::sleep_for

LCDHandler::LCDHandler() 
    : lcd_(nullptr), update_interval_ms_(1000), current_line1_(""), current_line2_(""), is_initialized_(false) {
    // Use /dev/i2c-7 for Jetson Orin Nano (as per Copilot instructions)
    lcd_ = std::make_unique<LCD_I2C>("/dev/i2c-7", 0x27, true);
}

LCDHandler::~LCDHandler() {
    // LCD_I2C-Destruktor wird automatisch aufgerufen
}

bool LCDHandler::init() {
    bool success = lcd_->init();
    if (success) {
        is_initialized_ = true;
        // REMOVED: showStartupMessage() - Boot sequence controlled by autostart.sh
        // Message flow: System Booted → Autostart Enabled → Starting Script → (main app takes over)
    }
    return success;
}

void LCDHandler::cleanup() {
    if (lcd_) {
        clear();
        lcd_.reset();
    }
}

std::string LCDHandler::truncateToWidth(const std::string& text, int max_width) {
    if (text.length() <= max_width) {
        return text;
    }
    return text.substr(0, max_width);
}

std::string LCDHandler::centerText(const std::string& text, int width) {
    if (text.length() >= width) {
        return truncateToWidth(text, width);
    }
    
    int padding = (width - text.length()) / 2;
    return std::string(padding, ' ') + text + std::string(width - padding - text.length(), ' ');
}

std::string LCDHandler::formatTime(int seconds) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << minutes << ":" 
       << std::setfill('0') << std::setw(2) << secs;
    return ss.str();
}

void LCDHandler::displayMessage(const std::string& line1, const std::string& line2) {
    if (!is_initialized_) return;
    
    // CRITICAL: Thread-safe LCD operations with mutex
    std::lock_guard<std::mutex> lock(lcd_mutex_);
    
    std::string l1 = truncateToWidth(line1, 16);
    std::string l2 = truncateToWidth(line2, 16);
    
    // Nur updaten wenn sich was geändert hat
    if (l1 != current_line1_ || l2 != current_line2_) {
        current_line1_ = l1;
        current_line2_ = l2;
        
        // MAXIMUM stability: Rate limiting 100ms between LCD updates (was 50ms - still too fast)
        auto now = std::chrono::steady_clock::now();
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count();
        
        if (time_since_last < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 - time_since_last));
        }
        
        // Additional settling delay before I2C operation
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        // LCD_I2C verwendet printMessage mit \n als Trenner
        std::string full_message = l1 + "\n" + l2;
        lcd_->clear();
        
        // Longer delay between clear and print for I2C bus stability
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        lcd_->printMessage(full_message);
        
        // Extra delay after print to let LCD settle
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        last_update_time_ = std::chrono::steady_clock::now();
    }
}

void LCDHandler::clear() {
    if (!is_initialized_) return;
    lcd_->clear();
    current_line1_ = "";
    current_line2_ = "";
}

void LCDHandler::showStartupMessage() {
    displayMessage(
        centerText("DRONE CAM", 16),
        centerText("System Ready!", 16)
    );
}

void LCDHandler::showFunnyMessage() {
    // Zufällige lustige Sprüche
    std::vector<std::pair<std::string, std::string>> funny_messages = {
        {"Ready 2 Fly!", "Let's go hunt!"},
        {"Drone Activated", "Sky is calling!"},
        {"Camera Armed", "Target acquired"},
        {"Flight Mode ON", "Buckle up!"},
        {"ZED Vision", "Double trouble!"},
        {"Jetson Power", "AI engaged!"}
    };
    
    static int message_index = 0;
    auto& msg = funny_messages[message_index % funny_messages.size()];
    message_index++;
    
    displayMessage(
        centerText(msg.first, 16),
        centerText(msg.second, 16)
    );
}

void LCDHandler::showInitializing(const std::string& component) {
    displayMessage(
        "Initializing...",
        truncateToWidth(component, 16)
    );
}

void LCDHandler::showUSBWaiting() {
    displayMessage(
        "Waiting for USB",
        "Insert storage.."
    );
}

void LCDHandler::showRecording(const std::string& profile, int total_seconds, int remaining_seconds) {
    // Verkürze Profil-Namen für Display
    std::string short_profile = profile;
    if (profile == "realtime_30fps") short_profile = "RT-30FPS";
    else if (profile == "realtime_light") short_profile = "RT-LIGHT";
    else if (profile == "long_mission") short_profile = "LONGMISS";
    else if (profile == "training") short_profile = "TRAINING";
    else if (profile == "ultra_quality") short_profile = "ULTRA-Q";
    else if (profile == "development") short_profile = "DEVELOP";
    else if (profile == "realtime_heavy") short_profile = "RT-HEAVY";
    else if (profile == "quick_test") short_profile = "QUICKTEST";
    
    short_profile = truncateToWidth(short_profile, 16);
    
    std::string time_display = formatTime(remaining_seconds) + "/" + formatTime(total_seconds);
    
    displayMessage(
        short_profile,
        centerText(time_display, 16)
    );
}

void LCDHandler::showRecordingComplete() {
    displayMessage(
        centerText("Recording", 16),
        centerText("Complete!", 16)
    );
}

void LCDHandler::showError(const std::string& error) {
    displayMessage(
        "ERROR:",
        truncateToWidth(error, 16)
    );
}

bool LCDHandler::shouldUpdate() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count();
    return elapsed >= update_interval_ms_;
}
