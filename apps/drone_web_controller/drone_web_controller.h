#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include "zed_recorder.h"
#include "storage.h"
#include "lcd_handler.h"

enum class RecorderState {
    IDLE,
    RECORDING,
    STOPPING,
    ERROR
};

struct RecordingStatus {
    RecorderState state;
    int recording_time_remaining;
    int recording_duration_total;
    long bytes_written;
    double mb_per_second;
    std::string current_file_path;
    std::string error_message;
};

class DroneWebController {
public:
    DroneWebController();
    ~DroneWebController();
    
    // Control methods
    bool startRecording();
    bool stopRecording();
    bool shutdownSystem();
    
    // Status methods
    RecordingStatus getStatus() const;
    bool isRecording() const { return current_state_ == RecorderState::RECORDING; }
    
    // Network methods
    bool startHotspot();
    bool stopHotspot();
    bool isHotspotActive() const { return hotspot_active_; }
    
    // Initialize and start the web server
    bool initialize();
    void startWebServer(int port = 8080);
    void stopWebServer();
    
    // HTTP request handling
    void handleClientRequest(int client_socket);
    
    // LCD display updates
    void updateLCD(const std::string& line1, const std::string& line2 = "");
    
    // WiFi monitoring
    bool monitorWiFiStatus();
    void restartWiFiIfNeeded();
    
private:
    // Core components
    std::unique_ptr<ZEDRecorder> recorder_;
    std::unique_ptr<StorageHandler> storage_;
    std::unique_ptr<LCDHandler> lcd_;
    
    // State management
    std::atomic<RecorderState> current_state_{RecorderState::IDLE};
    std::atomic<bool> recording_active_{false};
    std::atomic<bool> hotspot_active_{false};
    std::atomic<bool> web_server_running_{false};
    
    // Recording state
    std::chrono::steady_clock::time_point recording_start_time_;
    int recording_duration_seconds_{240}; // 4 minutes default
    std::string current_recording_path_;
    
    // Background tasks
    std::unique_ptr<std::thread> recording_monitor_thread_;
    std::unique_ptr<std::thread> web_server_thread_;
    std::unique_ptr<std::thread> system_monitor_thread_;
    
    // Private methods
    void recordingMonitorLoop();
    void webServerLoop(int port);
    void systemMonitorLoop();
    bool setupWiFiHotspot();
    bool teardownWiFiHotspot();
    void updateRecordingStatus();
    
    // Web server helper methods
    std::string generateMainPage();
    std::string generateStatusAPI();
    std::string generateAPIResponse(const std::string& message);
    
    // Signal handlers
    static DroneWebController* instance_;
    static void signalHandler(int signal);
    void handleShutdown();
    
    std::atomic<bool> shutdown_requested_{false};
};