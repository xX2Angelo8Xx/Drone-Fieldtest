#ifndef LCD_HANDLER_H
#define LCD_HANDLER_H

#include "lcd_i2c.h"
#include <string>
#include <chrono>
#include <vector>
#include <memory>
#include <mutex>

class LCDHandler {
private:
    std::unique_ptr<LCD_I2C> lcd_;
    std::chrono::steady_clock::time_point last_update_time_;
    int update_interval_ms_;
    std::string current_line1_;
    std::string current_line2_;
    bool is_initialized_;
    std::mutex lcd_mutex_;  // Thread-safe LCD operations
    
    // Hilfsfunktionen
    std::string truncateToWidth(const std::string& text, int max_width = 16);
    std::string centerText(const std::string& text, int width = 16);
    std::string formatTime(int seconds);
    
public:
    LCDHandler();
    ~LCDHandler();
    
    bool init();
    void cleanup();
    
    // Basis-Funktionen
    void displayMessage(const std::string& line1, const std::string& line2 = "");
    void clear();
    
    // Spezielle Display-Modi
    void showStartupMessage();
    void showFunnyMessage();
    void showInitializing(const std::string& component);
    void showUSBWaiting();
    void showRecording(const std::string& profile, int total_seconds, int remaining_seconds);
    void showRecordingComplete();
    void showError(const std::string& error);
    
    // Update-Kontrolle
    void setUpdateInterval(int milliseconds) { update_interval_ms_ = milliseconds; }
    bool shouldUpdate();
};

#endif
