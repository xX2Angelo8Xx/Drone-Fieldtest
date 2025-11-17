#pragma once

#include <string>
#include <fstream>
#include <sl/Camera.hpp>
#include <atomic>
#include <thread>
#include <memory>

/**
 * @brief Saves raw 32-bit depth data to binary files
 * 
 * Format: .depth files containing:
 * - Header: width (4 bytes), height (4 bytes), frame_number (4 bytes)
 * - Data: width * height * sizeof(float) bytes of depth values in meters
 * 
 * This format is MUCH faster than PNG encoding and preserves full 32-bit precision.
 */
class DepthDataWriter {
public:
    DepthDataWriter();
    ~DepthDataWriter();
    
    /**
     * @brief Initialize depth data writer
     * @param output_dir Directory where .depth files will be saved
     * @param target_fps Target FPS for depth capture (0 = every frame)
     * @return true if initialization successful
     */
    bool init(const std::string& output_dir, int target_fps = 10);
    
    /**
     * @brief Start depth data capture thread
     * @param zed Reference to ZED camera for depth retrieval
     */
    void start(sl::Camera& zed);
    
    /**
     * @brief Stop depth data capture
     */
    void stop();
    
    /**
     * @brief Get current frame number
     */
    int getFrameCount() const { return frame_count_.load(); }
    
    /**
     * @brief Get current FPS
     */
    float getCurrentFPS() const { return current_fps_.load(); }
    
private:
    void captureLoop(sl::Camera* zed);
    bool saveDepthFrame(const sl::Mat& depth, int frame_number);
    
    std::string output_dir_;
    int target_fps_;
    std::atomic<bool> running_;
    std::atomic<int> frame_count_;
    std::atomic<float> current_fps_;
    
    std::unique_ptr<std::thread> capture_thread_;
    
    // Depth retrieval configuration
    sl::Mat depth_mat_;
    sl::RuntimeParameters runtime_params_;
};
