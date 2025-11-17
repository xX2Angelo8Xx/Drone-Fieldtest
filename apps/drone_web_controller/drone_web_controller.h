#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include "zed_recorder.h"
#include "raw_frame_recorder.h"
#include "depth_data_writer.h"
#include "storage.h"
#include "lcd_handler.h"
#include "safe_hotspot_manager.h"

enum class RecorderState {
    IDLE,
    RECORDING,
    STOPPING,
    ERROR
};

enum class RecordingModeType {
    SVO2,              // Standard SVO2 compressed recording (no depth computation)
    SVO2_DEPTH_INFO,   // SVO2 + raw depth data (32-bit float .depth files, fast)
    SVO2_DEPTH_IMAGES, // SVO2 + depth visualization (PNG images, slower but visual)
    RAW_FRAMES         // RAW frame recording (separate left/right/depth images)
};

struct RecordingStatus {
    RecorderState state;
    int recording_time_remaining;
    int recording_duration_total;
    long bytes_written;
    double mb_per_second;
    std::string current_file_path;
    std::string error_message;
    
    // Raw frame mode specific
    RecordingModeType recording_mode;
    std::string depth_mode;
    long frame_count;
    float current_fps;
    float depth_fps;  // Depth computation FPS (for test modes)
    
    // System status
    bool camera_initializing;
    std::string status_message;
};

class DroneWebController {
public:
    DroneWebController();
    ~DroneWebController();
    
    // Control methods
    bool startRecording();
    bool stopRecording();
    bool shutdownSystem();
    
    // Recording mode configuration
    void setRecordingMode(RecordingModeType mode);
    void setDepthMode(DepthMode depth_mode);
    RecordingModeType getRecordingMode() const { return recording_mode_; }
    DepthMode getDepthMode() const { return depth_mode_; }
    
    // Camera settings
    void setCameraResolution(RecordingMode mode);
    void setCameraExposure(int exposure);  // -1 = auto, 0-100 = manual
    RecordingMode getCameraResolution();   // Removed const
    int getCameraExposure();               // Removed const
    
    // Status methods
    RecordingStatus getStatus() const;
    bool isRecording() const { return current_state_ == RecorderState::RECORDING; }
    bool isShutdownRequested() const { return shutdown_requested_.load(); }
    
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
    // Core components - dual recorder support
    std::unique_ptr<ZEDRecorder> svo_recorder_;
    std::unique_ptr<RawFrameRecorder> raw_recorder_;
    std::unique_ptr<DepthDataWriter> depth_data_writer_;  // For SVO2_DEPTH_INFO mode
    std::unique_ptr<StorageHandler> storage_;
    std::unique_ptr<LCDHandler> lcd_;
    
    // Network management - SAFE implementation (complies with NETWORK_SAFETY_POLICY.md)
    std::unique_ptr<SafeHotspotManager> hotspot_manager_;
    
    // Recording mode configuration
    RecordingModeType recording_mode_{RecordingModeType::SVO2};
    DepthMode depth_mode_{DepthMode::NEURAL_LITE};
    RecordingMode camera_resolution_{RecordingMode::HD720_60FPS};  // Default camera resolution/FPS
    std::atomic<int> depth_recording_fps_{10};  // FPS for depth visualization saving (0 = disabled)
    
    // State management
    std::atomic<RecorderState> current_state_{RecorderState::IDLE};
    std::atomic<bool> recording_active_{false};
    std::atomic<bool> hotspot_active_{false};
    std::atomic<bool> web_server_running_{false};
    std::atomic<bool> camera_initializing_{false};
    
    // Status message for UI feedback
    std::string status_message_;
    std::mutex status_mutex_;
    
    // Recording state
    std::chrono::steady_clock::time_point recording_start_time_;
    int recording_duration_seconds_{240}; // 4 minutes default
    std::string current_recording_path_;
    
    // LCD recording display state
    int lcd_display_cycle_{0};  // Alternates between different recording info displays
    std::chrono::steady_clock::time_point last_lcd_update_;
    
    // Background tasks
    std::unique_ptr<std::thread> recording_monitor_thread_;
    std::unique_ptr<std::thread> web_server_thread_;
    std::unique_ptr<std::thread> system_monitor_thread_;
    std::unique_ptr<std::thread> depth_viz_thread_;
    std::atomic<bool> depth_viz_running_{false};
    
    // Web server
    std::atomic<int> server_fd_{-1};  // Server socket file descriptor for clean shutdown
    
    // Private methods
    void recordingMonitorLoop();
    void webServerLoop(int port);
    void systemMonitorLoop();
    void depthVisualizationLoop();  // New: Depth visualization thread
    bool setupWiFiHotspot();
    bool teardownWiFiHotspot();
    bool verifyHotspotActive();     // Verify NetworkManager hotspot is running
    void displayWiFiStatus();       // Display WiFi connection info
    void updateRecordingStatus();
    std::string getDepthModeShortName(DepthMode mode) const;
    std::string getDepthModeName(DepthMode mode) const;
    sl::DEPTH_MODE convertDepthMode(DepthMode mode) const;
    
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