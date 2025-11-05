#pragma once
#include <sl/Camera.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>
#include <thread>
#include <memory>

// Forward declarations for AI integration
class ObjectDetector;
struct Detection {
    cv::Rect bbox;
    std::string class_name;
    float confidence;
    cv::Point3f world_position; // 3D position from ZED depth
};

enum class StreamQuality {
    LOW_BANDWIDTH,    // 1-2 Mbps - for cellular connections
    MEDIUM_QUALITY,   // 3-5 Mbps - for stable WiFi
    HIGH_QUALITY      // 6-10 Mbps - for high-speed connections
};

class ZEDLiveStreamer {
public:
    ZEDLiveStreamer();
    ~ZEDLiveStreamer();
    
    // Initialize streaming system
    bool init(StreamQuality quality = StreamQuality::MEDIUM_QUALITY);
    
    // Streaming control
    bool startStream(const std::string& rtmp_url);
    void stopStream();
    bool isStreaming() const { return streaming_; }
    
    // AI Integration
    bool loadAIModel(const std::string& model_path);
    bool enableObjectDetection(bool enable = true);
    bool enableDepthOverlay(bool enable = true);
    
    // Telemetry overlay
    void updateTelemetry(float battery_percent, float altitude_m, 
                        float speed_ms, const std::string& gps_coords);
    
    // Performance monitoring
    float getCurrentFPS() const { return current_fps_; }
    float getStreamBitrate() const { return stream_bitrate_mbps_; }
    int getDroppedFrames() const { return dropped_frames_; }
    
private:
    // Core components
    sl::Camera zed_;
    cv::VideoWriter stream_encoder_;
    std::unique_ptr<ObjectDetector> ai_model_;
    
    // Threading
    std::unique_ptr<std::thread> stream_thread_;
    std::atomic<bool> streaming_{false};
    std::atomic<bool> ai_enabled_{false};
    std::atomic<bool> depth_enabled_{false};
    
    // Performance tracking
    std::atomic<float> current_fps_{0.0f};
    std::atomic<float> stream_bitrate_mbps_{0.0f};
    std::atomic<int> dropped_frames_{0};
    
    // Telemetry data
    struct TelemetryData {
        float battery_percent = 0.0f;
        float altitude_m = 0.0f;
        float speed_ms = 0.0f;
        std::string gps_coords = "No GPS";
        std::chrono::steady_clock::time_point last_update;
    } telemetry_;
    std::mutex telemetry_mutex_;
    
    // Streaming configuration
    StreamQuality quality_;
    int target_bitrate_kbps_;
    cv::Size stream_resolution_;
    
    // Core streaming loop
    void streamingLoop();
    
    // AI processing
    std::vector<Detection> processAI(const cv::Mat& frame);
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections);
    
    // Overlay rendering
    void drawTelemetryOverlay(cv::Mat& frame);
    void drawDepthOverlay(cv::Mat& frame, const sl::Mat& depth_map);
    
    // Utility functions
    cv::Mat slMat2cvMat(const sl::Mat& input);
    void configureEncoder(const std::string& rtmp_url);
    void updatePerformanceMetrics();
};