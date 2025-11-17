#include "depth_data_writer.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

DepthDataWriter::DepthDataWriter() 
    : target_fps_(10)
    , running_(false)
    , frame_count_(0)
    , current_fps_(0.0f) {
}

DepthDataWriter::~DepthDataWriter() {
    stop();
}

bool DepthDataWriter::init(const std::string& output_dir, int target_fps) {
    output_dir_ = output_dir;
    target_fps_ = target_fps;
    
    // Create output directory if it doesn't exist
    try {
        fs::create_directories(output_dir);
        std::cout << "[DEPTH_DATA] Output directory: " << output_dir << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DEPTH_DATA] Failed to create output directory: " << e.what() << std::endl;
        return false;
    }
    
    // Initialize depth matrix for retrieval
    runtime_params_.enable_depth = true;
    runtime_params_.confidence_threshold = 50;
    runtime_params_.texture_confidence_threshold = 100;
    
    std::cout << "[DEPTH_DATA] Initialized (target " << target_fps << " FPS)" << std::endl;
    return true;
}

void DepthDataWriter::start(sl::Camera& zed) {
    if (running_) {
        std::cout << "[DEPTH_DATA] Already running" << std::endl;
        return;
    }
    
    running_ = true;
    frame_count_ = 0;
    
    capture_thread_ = std::make_unique<std::thread>(&DepthDataWriter::captureLoop, this, &zed);
    std::cout << "[DEPTH_DATA] Capture thread started (target " << target_fps_ << " FPS)" << std::endl;
}

void DepthDataWriter::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (capture_thread_ && capture_thread_->joinable()) {
        capture_thread_->join();
    }
    
    std::cout << "[DEPTH_DATA] Stopped. Total frames: " << frame_count_.load() << std::endl;
}

void DepthDataWriter::captureLoop(sl::Camera* zed) {
    if (!zed) {
        std::cerr << "[DEPTH_DATA] Invalid camera pointer!" << std::endl;
        return;
    }
    
    auto last_capture = std::chrono::steady_clock::now();
    int capture_interval_ms = (target_fps_ > 0) ? (1000 / target_fps_) : 100;  // Default 100ms if no FPS set
    
    auto fps_start = std::chrono::steady_clock::now();
    int fps_frame_count = 0;
    
    std::cout << "[DEPTH_DATA] Capture loop started (target FPS: " << target_fps_ 
              << ", interval: " << capture_interval_ms << "ms)" << std::endl;
    
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_capture).count();
        
        // Skip this iteration if not enough time has passed (non-blocking FPS control)
        if (elapsed < capture_interval_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Short sleep, don't block recording
            continue;
        }
        
        last_capture = now;
        
        // Retrieve depth map (non-blocking - uses already grabbed frame from main recording loop)
        sl::ERROR_CODE err = zed->retrieveMeasure(depth_mat_, sl::MEASURE::DEPTH, sl::MEM::CPU);
        if (err == sl::ERROR_CODE::SUCCESS) {
            int frame_num = frame_count_.fetch_add(1);
            
            // Save asynchronously if possible (current impl is sync, could be optimized later)
            if (saveDepthFrame(depth_mat_, frame_num)) {
                fps_frame_count++;
                
                // Update FPS every second
                auto fps_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fps_start).count();
                if (fps_elapsed >= 1000) {
                    current_fps_ = (fps_frame_count * 1000.0f) / fps_elapsed;
                    std::cout << "[DEPTH_DATA] Current FPS: " << current_fps_ << " (target: " << target_fps_ << ")" << std::endl;
                    fps_frame_count = 0;
                    fps_start = now;
                }
            } else {
                std::cerr << "[DEPTH_DATA] Failed to save frame " << frame_num << std::endl;
            }
        }
        // Note: retrieveMeasure may fail if no new frame available, which is normal
        // Don't spam error messages for that case
        
        // Very short sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "[DEPTH_DATA] Capture loop ended" << std::endl;
}

bool DepthDataWriter::saveDepthFrame(const sl::Mat& depth, int frame_number) {
    // Generate filename: depth_NNNNNN.depth
    char filename[256];
    snprintf(filename, sizeof(filename), "depth_%06d.depth", frame_number);
    std::string filepath = output_dir_ + "/" + filename;
    
    // Open file for binary write
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[DEPTH_DATA] Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    // Write header: width, height, frame_number (all 4 bytes each)
    int width = depth.getWidth();
    int height = depth.getHeight();
    
    file.write(reinterpret_cast<const char*>(&width), sizeof(int));
    file.write(reinterpret_cast<const char*>(&height), sizeof(int));
    file.write(reinterpret_cast<const char*>(&frame_number), sizeof(int));
    
    // Write depth data (32-bit floats)
    // ZED depth is in meters, stored as float32
    sl::float1* depth_ptr = depth.getPtr<sl::float1>();
    size_t data_size = width * height * sizeof(float);
    
    file.write(reinterpret_cast<const char*>(depth_ptr), data_size);
    
    file.close();
    
    return true;
}
