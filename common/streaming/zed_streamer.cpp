#include "zed_streamer.h"
#include <iostream>
#include <chrono>

ZEDLiveStreamer::ZEDLiveStreamer() {
    // Initialize performance tracking
    current_fps_ = 0.0f;
    stream_bitrate_mbps_ = 0.0f;
    dropped_frames_ = 0;
}

ZEDLiveStreamer::~ZEDLiveStreamer() {
    stopStream();
}

bool ZEDLiveStreamer::init(StreamQuality quality) {
    quality_ = quality;
    
    // Configure camera for streaming (optimized for low latency)
    sl::InitParameters init_params;
    init_params.camera_resolution = sl::RESOLUTION::HD720;
    init_params.camera_fps = 15;
    init_params.depth_mode = sl::DEPTH_MODE::PERFORMANCE; // Faster than NEURAL
    init_params.coordinate_units = sl::UNIT::METER;
    init_params.depth_minimum_distance = 0.3f; // 30cm minimum
    init_params.depth_maximum_distance = 20.0f; // 20m maximum for drones
    
    sl::ERROR_CODE err = zed_.open(init_params);
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "[STREAM] Failed to initialize ZED camera: " << err << std::endl;
        return false;
    }
    
    // Configure streaming parameters based on quality
    switch (quality_) {
        case StreamQuality::LOW_BANDWIDTH:
            target_bitrate_kbps_ = 1500;  // 1.5 Mbps
            stream_resolution_ = cv::Size(640, 360); // Lower resolution
            break;
        case StreamQuality::MEDIUM_QUALITY:
            target_bitrate_kbps_ = 3000;  // 3 Mbps
            stream_resolution_ = cv::Size(1280, 720); // HD720
            break;
        case StreamQuality::HIGH_QUALITY:
            target_bitrate_kbps_ = 6000;  // 6 Mbps
            stream_resolution_ = cv::Size(1280, 720); // HD720 high bitrate
            break;
    }
    
    std::cout << "[STREAM] ZED camera initialized for streaming" << std::endl;
    std::cout << "[STREAM] Quality: " << (int)quality_ << ", Target bitrate: " 
              << target_bitrate_kbps_ << " kbps" << std::endl;
    
    return true;
}

bool ZEDLiveStreamer::startStream(const std::string& rtmp_url) {
    if (streaming_) {
        std::cout << "[STREAM] Already streaming" << std::endl;
        return true;
    }
    
    // Configure video encoder for streaming
    configureEncoder(rtmp_url);
    
    if (!stream_encoder_.isOpened()) {
        std::cerr << "[STREAM] Failed to open stream encoder" << std::endl;
        return false;
    }
    
    // Start streaming thread
    streaming_ = true;
    stream_thread_ = std::make_unique<std::thread>(&ZEDLiveStreamer::streamingLoop, this);
    
    std::cout << "[STREAM] Live streaming started to: " << rtmp_url << std::endl;
    return true;
}

void ZEDLiveStreamer::stopStream() {
    if (!streaming_) return;
    
    streaming_ = false;
    
    if (stream_thread_ && stream_thread_->joinable()) {
        stream_thread_->join();
    }
    
    if (stream_encoder_.isOpened()) {
        stream_encoder_.release();
    }
    
    std::cout << "[STREAM] Live streaming stopped" << std::endl;
}

void ZEDLiveStreamer::streamingLoop() {
    sl::Mat zed_image, depth_map;
    cv::Mat cv_frame, display_frame;
    
    auto last_fps_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    
    std::cout << "[STREAM] Streaming loop started" << std::endl;
    
    while (streaming_) {
        auto frame_start = std::chrono::steady_clock::now();
        
        // Capture frame from ZED
        sl::ERROR_CODE grab_result = zed_.grab();
        if (grab_result != sl::ERROR_CODE::SUCCESS) {
            dropped_frames_++;
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps fallback
            continue;
        }
        
        // Retrieve left camera image
        zed_.retrieveImage(zed_image, sl::VIEW::LEFT, sl::MEM::CPU, stream_resolution_);
        cv_frame = slMat2cvMat(zed_image);
        
        // Create display frame (copy for overlay processing)
        cv_frame.copyTo(display_frame);
        
        // AI Processing (if enabled)
        if (ai_enabled_ && ai_model_) {
            auto detections = processAI(cv_frame);
            drawDetections(display_frame, detections);
        }
        
        // Depth overlay (if enabled)
        if (depth_enabled_) {
            zed_.retrieveMeasure(depth_map, sl::MEASURE::DEPTH, sl::MEM::CPU, stream_resolution_);
            drawDepthOverlay(display_frame, depth_map);
        }
        
        // Telemetry overlay
        drawTelemetryOverlay(display_frame);
        
        // Write frame to stream
        if (stream_encoder_.isOpened()) {
            stream_encoder_.write(display_frame);
        }
        
        // Update performance metrics
        frame_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_fps_time).count();
        
        if (time_diff >= 1) {
            current_fps_ = frame_count / (float)time_diff;
            frame_count = 0;
            last_fps_time = current_time;
            
            // Calculate target frame time (15fps = 66ms per frame)
            auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - frame_start).count();
            
            if (frame_time < 66) {
                std::this_thread::sleep_for(std::chrono::milliseconds(66 - frame_time));
            }
        }
    }
    
    std::cout << "[STREAM] Streaming loop ended" << std::endl;
}

void ZEDLiveStreamer::drawTelemetryOverlay(cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    
    // Background for telemetry data
    cv::Rect overlay_rect(10, 10, 300, 120);
    cv::rectangle(frame, overlay_rect, cv::Scalar(0, 0, 0, 128), -1);
    
    // Draw telemetry text
    int y_offset = 30;
    cv::putText(frame, "DRONE TELEMETRY", cv::Point(20, y_offset), 
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
    
    y_offset += 25;
    cv::putText(frame, "Battery: " + std::to_string((int)telemetry_.battery_percent) + "%", 
                cv::Point(20, y_offset), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    
    y_offset += 20;
    cv::putText(frame, "Altitude: " + std::to_string((int)telemetry_.altitude_m) + "m", 
                cv::Point(20, y_offset), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
    
    y_offset += 20;
    cv::putText(frame, "Speed: " + std::to_string((int)(telemetry_.speed_ms * 3.6)) + "km/h", 
                cv::Point(20, y_offset), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 255), 1);
    
    y_offset += 20;
    cv::putText(frame, "GPS: " + telemetry_.gps_coords, 
                cv::Point(20, y_offset), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 0), 1);
    
    // Stream performance info
    cv::putText(frame, "FPS: " + std::to_string((int)current_fps_) + 
                " | Dropped: " + std::to_string(dropped_frames_), 
                cv::Point(frame.cols - 200, frame.rows - 20), 
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
}

void ZEDLiveStreamer::drawDetections(cv::Mat& frame, const std::vector<Detection>& detections) {
    for (const auto& detection : detections) {
        // Draw bounding box
        cv::rectangle(frame, detection.bbox, cv::Scalar(0, 255, 0), 3);
        
        // Draw label with confidence and distance
        std::string label = detection.class_name + " " + 
                           std::to_string((int)(detection.confidence * 100)) + "% " +
                           std::to_string((int)detection.world_position.z) + "m";
        
        cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, nullptr);
        cv::Rect label_rect(detection.bbox.x, detection.bbox.y - text_size.height - 10,
                           text_size.width + 10, text_size.height + 10);
        
        cv::rectangle(frame, label_rect, cv::Scalar(0, 255, 0), -1);
        cv::putText(frame, label, cv::Point(detection.bbox.x + 5, detection.bbox.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
    }
}

cv::Mat ZEDLiveStreamer::slMat2cvMat(const sl::Mat& input) {
    int cv_type = -1;
    switch (input.getDataType()) {
        case sl::MAT_TYPE::U8_C4: cv_type = CV_8UC4; break;
        case sl::MAT_TYPE::U8_C3: cv_type = CV_8UC3; break;
        case sl::MAT_TYPE::U8_C1: cv_type = CV_8UC1; break;
        default: break;
    }
    
    return cv::Mat(input.getHeight(), input.getWidth(), cv_type, input.getPtr<sl::uchar1>(sl::MEM::CPU));
}

void ZEDLiveStreamer::configureEncoder(const std::string& rtmp_url) {
    // Use H.264 codec optimized for streaming
    int fourcc = cv::VideoWriter::fourcc('H','2','6','4');
    
    // Configure encoder with streaming-optimized parameters
    std::vector<int> encoder_params = {
        cv::VIDEOWRITER_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY, // Use hardware encoding if available
        cv::VIDEOWRITER_PROP_BITRATE, target_bitrate_kbps_ * 1000, // Convert to bps
        cv::VIDEOWRITER_PROP_QUALITY, 80, // Good quality/compression balance
    };
    
    bool success = stream_encoder_.open(rtmp_url, fourcc, 15, stream_resolution_, encoder_params);
    
    if (!success) {
        std::cerr << "[STREAM] Failed to configure encoder for: " << rtmp_url << std::endl;
        std::cerr << "[STREAM] Falling back to basic configuration..." << std::endl;
        
        // Fallback configuration
        stream_encoder_.open(rtmp_url, fourcc, 15, stream_resolution_);
    }
    
    std::cout << "[STREAM] Encoder configured: " << stream_resolution_.width << "x" 
              << stream_resolution_.height << " @ 15fps, " << target_bitrate_kbps_ << " kbps" << std::endl;
}

void ZEDLiveStreamer::updateTelemetry(float battery_percent, float altitude_m, 
                                     float speed_ms, const std::string& gps_coords) {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    telemetry_.battery_percent = battery_percent;
    telemetry_.altitude_m = altitude_m;
    telemetry_.speed_ms = speed_ms;
    telemetry_.gps_coords = gps_coords;
    telemetry_.last_update = std::chrono::steady_clock::now();
}

bool ZEDLiveStreamer::loadAIModel(const std::string& model_path) {
    // [FUTURE] AI model loading (TensorRT, ONNX, etc.) - requires object detection architecture
    std::cout << "[STREAM] AI model loading not yet implemented: " << model_path << std::endl;
    return false;
}

bool ZEDLiveStreamer::enableObjectDetection(bool enable) {
    ai_enabled_ = enable && (ai_model_ != nullptr);
    std::cout << "[STREAM] Object detection " << (ai_enabled_ ? "enabled" : "disabled") << std::endl;
    return ai_enabled_;
}

bool ZEDLiveStreamer::enableDepthOverlay(bool enable) {
    depth_enabled_ = enable;
    std::cout << "[STREAM] Depth overlay " << (depth_enabled_ ? "enabled" : "disabled") << std::endl;
    return true;
}