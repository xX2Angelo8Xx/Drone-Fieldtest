#pragma once
#include <sl/Camera.hpp>
#include <string>
#include <fstream>
#include <atomic>
#include <thread>
#include <memory>

// Forward declaration - RecordingMode is defined in zed_recorder.h
// Include zed_recorder.h to get the shared enum
#include "zed_recorder.h"

// Depth computation modes (matching ZED SDK options)
enum class DepthMode {
    // Neural Network-based depth modes (AI-powered)
    NEURAL_PLUS,    // Best quality, highest computation (slowest)
    NEURAL,         // Balanced quality and performance
    NEURAL_LITE,    // Lightweight neural depth (recommended for Jetson Orin Nano)
    
    // Traditional depth modes (non-AI)
    ULTRA,          // Ultra mode - highest quality traditional
    QUALITY,        // Quality mode - balanced traditional
    PERFORMANCE,    // Performance mode - fastest traditional
    
    NONE            // No depth computation (images only)
};

class RawFrameRecorder {
public:
    RawFrameRecorder();
    ~RawFrameRecorder();
    
    // Initialize camera with specific mode and depth mode
    bool init(RecordingMode mode = RecordingMode::HD720_30FPS, 
              DepthMode depth_mode = DepthMode::NEURAL_LITE);
    
    // Start raw frame recording
    // base_dir: e.g., /media/angelo/DRONE_DATA/flight_20251112_143022/
    // Creates subdirectories: left/, right/, depth/, and sensor_data.csv
    bool startRecording(const std::string& base_dir);
    
    // Stop recording
    void stopRecording();
    
    // Close camera explicitly (for clean restart)
    void close();
    
    // Change depth mode (can be called before init or between recordings)
    void setDepthMode(DepthMode depth_mode);
    
    // Status
    bool isRecording() const;
    long getFrameCount() const;
    long getBytesWritten() const;  // Total bytes written (all files combined)
    
    // Helper function for mode names
    std::string getDepthModeName(DepthMode mode) const;
    std::string getRecordingModeName(RecordingMode mode) const;
    
    // Performance info
    float getCurrentFPS() const;  // Actual achieved FPS
    
    // Camera settings
    bool setCameraExposure(int exposure_value);  // -1 = auto, 0-100 = manual
    int getCameraExposure();  // Removed const
    bool setCameraGain(int gain_value);  // -1 = auto, 0-100 = manual
    int getCameraGain();
    RecordingMode getCurrentMode() const { return current_mode_; }
    
    // Get camera reference (for snapshot/livestream)
    sl::Camera* getCamera() { return &zed_; }
    
private:
    sl::Camera zed_;
    std::atomic<bool> recording_;
    std::atomic<long> frame_count_;
    std::atomic<size_t> bytes_written_;
    std::ofstream sensor_file_;
    std::unique_ptr<std::thread> record_thread_;
    
    RecordingMode current_mode_;
    DepthMode depth_mode_;
    
    std::string base_dir_;
    std::string left_dir_;
    std::string right_dir_;
    std::string depth_dir_;
    std::string sensor_path_;
    
    // Performance tracking
    std::atomic<float> current_fps_;
    
    // Recording loop
    void recordingLoop();
    
    // Helper methods
    bool createDirectoryStructure(const std::string& base_dir);
    bool saveImageJPEG(const sl::Mat& image, const std::string& path, int quality = 90);
    bool saveDepthMap(const sl::Mat& depth, const std::string& path);
    std::string generateFramePath(const std::string& dir, long frame_num, const std::string& suffix);
    
    // Depth computation configuration
    void configureDepthMode();
};
