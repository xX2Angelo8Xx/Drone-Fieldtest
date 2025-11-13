#include "raw_frame_recorder.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <opencv2/opencv.hpp>

RawFrameRecorder::RawFrameRecorder() 
    : recording_(false), frame_count_(0), bytes_written_(0), current_fps_(0.0f) {
}

RawFrameRecorder::~RawFrameRecorder() {
    try {
        stopRecording();
        
        // Wait for thread to finish
        if (record_thread_ && record_thread_->joinable()) {
            record_thread_->join();
        }
        
        // Close sensor file
        if (sensor_file_.is_open()) {
            sensor_file_.close();
        }
        
        // Close ZED camera
        if (zed_.isOpened()) {
            zed_.close();
        }
        
        std::cout << "[RAW_RECORDER] Destructor completed cleanly" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[RAW_RECORDER] Exception in destructor: " << e.what() << std::endl;
    }
}

bool RawFrameRecorder::init(RecordingMode mode, DepthMode depth_mode) {
    std::cout << "[RAW_RECORDER] Initializing in mode: " << getRecordingModeName(mode) 
              << " with depth: " << getDepthModeName(depth_mode) << std::endl;
    
    current_mode_ = mode;
    depth_mode_ = depth_mode;
    
    // ZED Camera initialization parameters
    sl::InitParameters init_params;
    init_params.coordinate_units = sl::UNIT::METER;
    init_params.sdk_verbose = true;
    
    // Set depth mode at initialization
    switch (depth_mode_) {
        case DepthMode::NEURAL_PLUS:
            init_params.depth_mode = sl::DEPTH_MODE::NEURAL_PLUS;
            break;
        case DepthMode::NEURAL:
            init_params.depth_mode = sl::DEPTH_MODE::NEURAL;
            break;
        case DepthMode::NEURAL_LITE:
            init_params.depth_mode = sl::DEPTH_MODE::NEURAL;  // Use NEURAL with lighter settings
            break;
        case DepthMode::ULTRA:
            init_params.depth_mode = sl::DEPTH_MODE::ULTRA;
            break;
        case DepthMode::QUALITY:
            init_params.depth_mode = sl::DEPTH_MODE::QUALITY;
            break;
        case DepthMode::PERFORMANCE:
            init_params.depth_mode = sl::DEPTH_MODE::PERFORMANCE;
            break;
        case DepthMode::NONE:
            init_params.depth_mode = sl::DEPTH_MODE::NONE;
            break;
    }
    
    // Set resolution and FPS based on mode
    switch (mode) {
        case RecordingMode::HD720_60FPS:
            init_params.camera_resolution = sl::RESOLUTION::HD720;
            init_params.camera_fps = 60;
            break;
        case RecordingMode::HD720_30FPS:
            init_params.camera_resolution = sl::RESOLUTION::HD720;
            init_params.camera_fps = 30;
            break;
        case RecordingMode::HD720_15FPS:
            init_params.camera_resolution = sl::RESOLUTION::HD720;
            init_params.camera_fps = 15;
            break;
        case RecordingMode::HD1080_30FPS:
            init_params.camera_resolution = sl::RESOLUTION::HD1080;
            init_params.camera_fps = 30;
            break;
        case RecordingMode::HD2K_15FPS:
            init_params.camera_resolution = sl::RESOLUTION::HD2K;
            init_params.camera_fps = 15;
            break;
        case RecordingMode::VGA_100FPS:
            init_params.camera_resolution = sl::RESOLUTION::VGA;
            init_params.camera_fps = 100;
            break;
    }
    
    // Multiple retry attempts for USB issues
    sl::ERROR_CODE err;
    int retry_count = 0;
    const int max_retries = 3;
    
    do {
        if (retry_count > 0) {
            std::cout << "[RAW_RECORDER] ZED camera retry attempt " << retry_count 
                      << "/" << max_retries << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        err = zed_.open(init_params);
        retry_count++;
        
    } while (err != sl::ERROR_CODE::SUCCESS && retry_count <= max_retries);
    
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "[RAW_RECORDER] Error opening ZED camera after " << max_retries 
                  << " attempts: " << err << std::endl;
        return false;
    }
    
    std::cout << "[RAW_RECORDER] ZED camera initialized successfully" << std::endl;
    
    // Enable positional tracking for sensor data
    sl::PositionalTrackingParameters tracking_params;
    err = zed_.enablePositionalTracking(tracking_params);
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cout << "[RAW_RECORDER] Warning: Positional tracking failed: " << err << std::endl;
    }
    
    return true;
}

void RawFrameRecorder::configureDepthMode() {
    // Note: Depth mode will be set via RuntimeParameters during grab()
    // This method prepares any necessary configuration
    
    switch (depth_mode_) {
        case DepthMode::NEURAL_PLUS:
            std::cout << "[RAW_RECORDER] Using NEURAL_PLUS depth mode (high quality, high compute)" << std::endl;
            break;
        case DepthMode::NEURAL:
            std::cout << "[RAW_RECORDER] Using NEURAL depth mode (balanced)" << std::endl;
            break;
        case DepthMode::NEURAL_LITE:
            std::cout << "[RAW_RECORDER] Using NEURAL_LITE depth mode (recommended for Jetson)" << std::endl;
            break;
        case DepthMode::ULTRA:
            std::cout << "[RAW_RECORDER] Using ULTRA depth mode (traditional high quality)" << std::endl;
            break;
        case DepthMode::QUALITY:
            std::cout << "[RAW_RECORDER] Using QUALITY depth mode (traditional balanced)" << std::endl;
            break;
        case DepthMode::PERFORMANCE:
            std::cout << "[RAW_RECORDER] Using PERFORMANCE depth mode (traditional fast)" << std::endl;
            break;
        case DepthMode::NONE:
            std::cout << "[RAW_RECORDER] Depth computation DISABLED (images only)" << std::endl;
            break;
    }
}

bool RawFrameRecorder::startRecording(const std::string& base_dir) {
    if (recording_) {
        std::cerr << "[RAW_RECORDER] Already recording" << std::endl;
        return false;
    }
    
    if (!zed_.isOpened()) {
        std::cerr << "[RAW_RECORDER] Camera not initialized" << std::endl;
        return false;
    }
    
    base_dir_ = base_dir;
    
    // Create directory structure
    if (!createDirectoryStructure(base_dir)) {
        std::cerr << "[RAW_RECORDER] Failed to create directory structure" << std::endl;
        return false;
    }
    
    // Open sensor data file
    sensor_path_ = base_dir + "/sensor_data.csv";
    sensor_file_.open(sensor_path_);
    if (!sensor_file_) {
        std::cerr << "[RAW_RECORDER] Failed to open sensor file: " << sensor_path_ << std::endl;
        return false;
    }
    
    // Write CSV header
    sensor_file_ << "frame_number,timestamp_ms,rotation_x,rotation_y,rotation_z,"
                 << "accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,"
                 << "mag_x,mag_y,mag_z,pressure,temperature" << std::endl;
    
    recording_ = true;
    frame_count_ = 0;
    bytes_written_ = 0;
    current_fps_ = 0.0f;
    
    // Start recording thread
    record_thread_ = std::make_unique<std::thread>(&RawFrameRecorder::recordingLoop, this);
    
    std::cout << "[RAW_RECORDER] Recording started: " << base_dir << std::endl;
    std::cout << "[RAW_RECORDER]   Left images: " << left_dir_ << std::endl;
    std::cout << "[RAW_RECORDER]   Right images: " << right_dir_ << std::endl;
    std::cout << "[RAW_RECORDER]   Depth maps: " << depth_dir_ << std::endl;
    std::cout << "[RAW_RECORDER]   Sensor data: " << sensor_path_ << std::endl;
    
    return true;
}

void RawFrameRecorder::recordingLoop() {
    sl::Mat left_image, right_image, depth_map;
    sl::SensorsData sensor_data;
    
    // Runtime parameters
    sl::RuntimeParameters runtime_params;
    runtime_params.enable_depth = (depth_mode_ != DepthMode::NONE);
    runtime_params.confidence_threshold = 100;
    runtime_params.texture_confidence_threshold = 100;
    
    auto loop_start = std::chrono::steady_clock::now();
    int fps_frame_count = 0;
    
    std::cout << "[RAW_RECORDER] Recording loop started" << std::endl;
    
    while (recording_) {
        auto frame_start = std::chrono::steady_clock::now();
        
        // Grab new frame
        sl::ERROR_CODE grab_result = zed_.grab(runtime_params);
        
        if (grab_result == sl::ERROR_CODE::SUCCESS) {
            long current_frame = frame_count_.load();
            
            // Retrieve left image
            zed_.retrieveImage(left_image, sl::VIEW::LEFT);
            std::string left_path = generateFramePath(left_dir_, current_frame, "left.jpg");
            if (!saveImageJPEG(left_image, left_path)) {
                std::cerr << "[RAW_RECORDER] Failed to save left image: " << left_path << std::endl;
            }
            
            // Retrieve right image
            zed_.retrieveImage(right_image, sl::VIEW::RIGHT);
            std::string right_path = generateFramePath(right_dir_, current_frame, "right.jpg");
            if (!saveImageJPEG(right_image, right_path)) {
                std::cerr << "[RAW_RECORDER] Failed to save right image: " << right_path << std::endl;
            }
            
            // Retrieve and save depth map (if enabled)
            if (depth_mode_ != DepthMode::NONE) {
                zed_.retrieveMeasure(depth_map, sl::MEASURE::DEPTH);
                std::string depth_path = generateFramePath(depth_dir_, current_frame, "depth.dat");
                if (!saveDepthMap(depth_map, depth_path)) {
                    std::cerr << "[RAW_RECORDER] Failed to save depth map: " << depth_path << std::endl;
                }
            }
            
            // Get sensor data
            if (zed_.getSensorsData(sensor_data, sl::TIME_REFERENCE::CURRENT) == sl::ERROR_CODE::SUCCESS) {
                auto imu_data = sensor_data.imu;
                auto mag_data = sensor_data.magnetometer;
                auto baro_data = sensor_data.barometer;
                
                // Write sensor data to CSV
                sensor_file_ << current_frame << ","
                            << sensor_data.imu.timestamp.getMilliseconds() << ","
                            << imu_data.pose.getOrientation().ox << ","
                            << imu_data.pose.getOrientation().oy << ","
                            << imu_data.pose.getOrientation().oz << ","
                            << imu_data.linear_acceleration.x << ","
                            << imu_data.linear_acceleration.y << ","
                            << imu_data.linear_acceleration.z << ","
                            << imu_data.angular_velocity.x << ","
                            << imu_data.angular_velocity.y << ","
                            << imu_data.angular_velocity.z << ","
                            << mag_data.magnetic_field_calibrated.x << ","
                            << mag_data.magnetic_field_calibrated.y << ","
                            << mag_data.magnetic_field_calibrated.z << ","
                            << baro_data.pressure << ","
                            << sensor_data.temperature.temperature_map[sl::SensorsData::TemperatureData::SENSOR_LOCATION::IMU]
                            << std::endl;
            }
            
            // Increment frame count
            frame_count_++;
            fps_frame_count++;
            
            // Calculate FPS every second
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - loop_start).count();
            if (elapsed >= 1) {
                current_fps_ = fps_frame_count / static_cast<float>(elapsed);
                std::cout << "[RAW_RECORDER] Frame " << current_frame << " | FPS: " 
                          << std::fixed << std::setprecision(1) << current_fps_.load() << std::endl;
                fps_frame_count = 0;
                loop_start = now;
            }
            
        } else if (grab_result == sl::ERROR_CODE::END_OF_SVOFILE_REACHED) {
            std::cout << "[RAW_RECORDER] End of file reached" << std::endl;
            break;
        } else {
            std::cerr << "[RAW_RECORDER] Grab error: " << grab_result << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    std::cout << "[RAW_RECORDER] Recording loop finished. Total frames: " << frame_count_.load() << std::endl;
}

void RawFrameRecorder::stopRecording() {
    if (!recording_) {
        return;
    }
    
    std::cout << "[RAW_RECORDER] Stopping recording..." << std::endl;
    recording_ = false;
    
    // Wait for thread to finish
    if (record_thread_ && record_thread_->joinable()) {
        record_thread_->join();
        record_thread_.reset();
    }
    
    // Close sensor file
    if (sensor_file_.is_open()) {
        sensor_file_.flush();
        sensor_file_.close();
    }
    
    std::cout << "[RAW_RECORDER] Recording stopped. Frames captured: " << frame_count_.load() << std::endl;
}

void RawFrameRecorder::close() {
    stopRecording();
    
    if (zed_.isOpened()) {
        zed_.close();
        std::cout << "[RAW_RECORDER] Camera closed" << std::endl;
    }
}

void RawFrameRecorder::setDepthMode(DepthMode depth_mode) {
    if (recording_) {
        std::cerr << "[RAW_RECORDER] Cannot change depth mode while recording" << std::endl;
        return;
    }
    depth_mode_ = depth_mode;
    std::cout << "[RAW_RECORDER] Depth mode set to: " << getDepthModeName(depth_mode) << std::endl;
}

bool RawFrameRecorder::isRecording() const {
    return recording_;
}

long RawFrameRecorder::getFrameCount() const {
    return frame_count_;
}

long RawFrameRecorder::getBytesWritten() const {
    return bytes_written_;
}

float RawFrameRecorder::getCurrentFPS() const {
    return current_fps_;
}

std::string RawFrameRecorder::getDepthModeName(DepthMode mode) const {
    switch (mode) {
        case DepthMode::NEURAL_PLUS: return "NEURAL_PLUS";
        case DepthMode::NEURAL: return "NEURAL";
        case DepthMode::NEURAL_LITE: return "NEURAL_LITE";
        case DepthMode::ULTRA: return "ULTRA";
        case DepthMode::QUALITY: return "QUALITY";
        case DepthMode::PERFORMANCE: return "PERFORMANCE";
        case DepthMode::NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

std::string RawFrameRecorder::getRecordingModeName(RecordingMode mode) const {
    switch (mode) {
        case RecordingMode::HD720_60FPS: return "HD720_60FPS";
        case RecordingMode::HD720_30FPS: return "HD720_30FPS";
        case RecordingMode::HD720_15FPS: return "HD720_15FPS";
        case RecordingMode::HD1080_30FPS: return "HD1080_30FPS";
        case RecordingMode::HD2K_15FPS: return "HD2K_15FPS";
        case RecordingMode::VGA_100FPS: return "VGA_100FPS";
        default: return "UNKNOWN";
    }
}

bool RawFrameRecorder::createDirectoryStructure(const std::string& base_dir) {
    try {
        // Create base directory
        std::filesystem::create_directories(base_dir);
        
        // Create subdirectories
        left_dir_ = base_dir + "/left";
        right_dir_ = base_dir + "/right";
        depth_dir_ = base_dir + "/depth";
        
        std::filesystem::create_directories(left_dir_);
        std::filesystem::create_directories(right_dir_);
        
        if (depth_mode_ != DepthMode::NONE) {
            std::filesystem::create_directories(depth_dir_);
        }
        
        std::cout << "[RAW_RECORDER] Directory structure created: " << base_dir << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[RAW_RECORDER] Failed to create directory structure: " << e.what() << std::endl;
        return false;
    }
}

bool RawFrameRecorder::saveImageJPEG(const sl::Mat& image, const std::string& path, int quality) {
    try {
        // Convert sl::Mat to cv::Mat
        cv::Mat cv_image(image.getHeight(), image.getWidth(), CV_8UC4, image.getPtr<sl::uchar1>());
        
        // Convert BGRA to BGR (remove alpha channel)
        cv::Mat cv_image_bgr;
        cv::cvtColor(cv_image, cv_image_bgr, cv::COLOR_BGRA2BGR);
        
        // Save as JPEG with specified quality
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(quality);
        
        bool success = cv::imwrite(path, cv_image_bgr, compression_params);
        
        if (success) {
            // Update bytes written (approximate)
            size_t file_size = std::filesystem::file_size(path);
            bytes_written_ += file_size;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "[RAW_RECORDER] Error saving image: " << e.what() << std::endl;
        return false;
    }
}

bool RawFrameRecorder::saveDepthMap(const sl::Mat& depth, const std::string& path) {
    try {
        // Save depth map as binary file (32-bit float values)
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Write dimensions
        int width = depth.getWidth();
        int height = depth.getHeight();
        file.write(reinterpret_cast<const char*>(&width), sizeof(int));
        file.write(reinterpret_cast<const char*>(&height), sizeof(int));
        
        // Write depth data
        size_t data_size = width * height * sizeof(float);
        file.write(reinterpret_cast<const char*>(depth.getPtr<sl::uchar1>()), data_size);
        
        file.close();
        
        // Update bytes written
        bytes_written_ += data_size + 2 * sizeof(int);
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[RAW_RECORDER] Error saving depth map: " << e.what() << std::endl;
        return false;
    }
}

std::string RawFrameRecorder::generateFramePath(const std::string& dir, long frame_num, const std::string& suffix) {
    std::ostringstream oss;
    oss << dir << "/frame_" << std::setw(6) << std::setfill('0') << frame_num << "_" << suffix;
    return oss.str();
}

bool RawFrameRecorder::setCameraExposure(int exposure_value) {
    if (!zed_.isOpened()) {
        std::cerr << "[RAW] Camera not initialized - cannot set exposure" << std::endl;
        return false;
    }
    
    if (exposure_value == -1) {
        sl::ERROR_CODE err = zed_.setCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE, -1);
        if (err == sl::ERROR_CODE::SUCCESS) {
            std::cout << "[RAW] Auto exposure enabled" << std::endl;
            return true;
        }
    } else if (exposure_value >= 0 && exposure_value <= 100) {
        sl::ERROR_CODE err = zed_.setCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE, exposure_value);
        if (err == sl::ERROR_CODE::SUCCESS) {
            std::cout << "[RAW] Manual exposure set to: " << exposure_value << std::endl;
            return true;
        }
    }
    
    std::cerr << "[RAW] Failed to set exposure" << std::endl;
    return false;
}

int RawFrameRecorder::getCameraExposure() {
    if (!zed_.isOpened()) {
        return -1;
    }
    int value = 0;
    zed_.getCameraSettings(sl::VIDEO_SETTINGS::EXPOSURE, value);
    return value;
}
