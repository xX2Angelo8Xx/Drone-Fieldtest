#pragma once
#include <sl/Camera.hpp>
#include <string>
#include <fstream>
#include <atomic>
#include <thread>
#include <memory>

enum class RecordingMode {
    HD720_60FPS,     // 720p @ 60fps
    HD720_30FPS,     // 720p @ 30fps (default)
    HD720_15FPS,     // 720p @ 15fps (no frame drops)
    HD1080_30FPS,    // 1080p @ 30fps
    HD2K_15FPS,      // 2K @ 15fps
    VGA_100FPS       // VGA @ 100fps
};

class ZEDRecorder {
public:
    ZEDRecorder();
    ~ZEDRecorder();
    
    // Initialisiere die Kamera mit spezifischem Modus
    bool init(RecordingMode mode = RecordingMode::HD720_30FPS);
    
    // Starte Aufnahme
    bool startRecording(const std::string& video_path, const std::string& sensor_path);
    
    // Stoppe Aufnahme
    void stopRecording();
    
    // Schließe Kamera explizit (für ordnungsgemäßen Neustart)
    void close();
    
    // === PRODUCTION READY FEATURES ===
    
    // Auto-segmentation DISABLED - no longer needed with NTFS/exFAT >4GB support
    // void enableAutoSegmentation(bool enable = true) { auto_segment_ = enable; }
    
    // === EXPERIMENTAL FEATURES (Under Development) ===
    // Note: These features are functional but may have gaps/instabilities
    
    // [EXPERIMENTAL] Seamless segmentation - switch files without stopping camera
    bool switchToNewSegment(const std::string& new_video_path, const std::string& new_sensor_path);
    
    // [EXPERIMENTAL] Fast segmentation with minimal recording gaps  
    bool fastSwitchToNewSegment(const std::string& new_video_path, const std::string& new_sensor_path);
    
    // [EXPERIMENTAL] Dual-instance recording for gap reduction
    bool dualInstanceSwitch(const std::string& new_video_path, const std::string& new_sensor_path);
    bool prepareNextRecording(const std::string& next_video_path);
    
    // [EXPERIMENTAL] Dual-camera mode for instant switching (requires 2 ZED cameras)
    bool initDualCamera();
    bool instantSwapRecording(const std::string& new_video_path, const std::string& new_sensor_path);
    
    // Status
    bool isRecording() const;
    long getBytesWritten() const;
    // int getCurrentSegment() const { return current_segment_; } // DISABLED: Segmentation removed
    
    // Hilfsfunktion für Modus-Namen
    std::string getModeName(RecordingMode mode) const;
    
private:
    sl::Camera zed_;
    std::atomic<bool> recording_;
    std::atomic<size_t> bytes_written_;  // FIX: Support files >4GB
    std::ofstream sensor_file_;
    std::unique_ptr<std::thread> record_thread_;
    RecordingMode current_mode_;
    std::string current_video_path_;  // Aktueller Videodateipfad (.svo oder .svo2)
    
    // Auto-segmentation support - DISABLED (no longer needed with NTFS/exFAT)
    // std::atomic<bool> auto_segment_{false};
    // std::atomic<int> current_segment_{1};
    // std::string base_video_path_;
    // std::string base_sensor_path_;
    
    // Dual recording instance support
    bool next_recording_prepared_{false};
    std::string prepared_video_path_;
    std::string prepared_sensor_path_;
    
    // [EXPERIMENTAL] Dual-camera support for instant switching (requires 2 ZED cameras)
    sl::Camera zed_secondary_;
    bool dual_camera_mode_{false};
    bool using_secondary_{false};
    
    // [EXPERIMENTAL] Memory buffer approach for gap-free switching
    bool enableMemoryBuffer(bool enable = true);
    bool memoryBufferedSwitch(const std::string& new_video_path, const std::string& new_sensor_path);
    std::vector<sl::Mat> frame_buffer_;
    std::atomic<bool> buffer_mode_{false};
    size_t max_buffer_frames_{300};
    
    // Aufnahme-Thread
    void recordingLoop(const std::string& video_path);
    
    // Helper methods for auto-segmentation
    std::string generateSegmentPath(const std::string& base_path, int segment_num, const std::string& extension);
};