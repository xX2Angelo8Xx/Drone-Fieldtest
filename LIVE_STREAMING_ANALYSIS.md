# Live Streaming Bandwidth Analysis for ZED 2i at 15fps

## Bandwidth Requirements

### Raw Data Calculations
```
ZED 2i HD720 (1280x720) stereo at 15fps:
- Single frame: 1280 × 720 × 3 bytes (RGB) = 2.76 MB per frame
- Stereo (left + right): 2.76 MB × 2 = 5.52 MB per frame
- At 15fps: 5.52 MB × 15 = 82.8 MB/s = 662.4 Mbps (RAW)
```

### Compressed Streaming (Realistic Requirements)

#### H.264 Compression (Recommended)
```
HD720@15fps H.264:
- High quality: 3-5 Mbps per camera
- Medium quality: 1-2 Mbps per camera  
- Low quality: 0.5-1 Mbps per camera

For stereo streaming:
- High quality: 6-10 Mbps total
- Medium quality: 2-4 Mbps total
- Low quality: 1-2 Mbps total
```

#### H.265/HEVC (Better Compression)
```
HD720@15fps H.265:
- High quality: 2-3 Mbps per camera
- Medium quality: 0.8-1.5 Mbps per camera
- Low quality: 0.3-0.8 Mbps per camera

For stereo streaming:
- High quality: 4-6 Mbps total
- Medium quality: 1.6-3 Mbps total  
- Low quality: 0.6-1.6 Mbps total
```

### Practical Upload Requirements

**For object detection overlay streaming:**
- **Minimum viable**: 2-3 Mbps upload (medium quality, single camera)
- **Good quality**: 4-6 Mbps upload (high quality, single camera)
- **Professional**: 8-12 Mbps upload (high quality stereo with overlays)

**Network considerations:**
- Add 20-30% overhead for TCP/UDP headers, retransmission
- Stable connection more important than peak bandwidth
- Low latency requirement: <200ms for real-time control

## Implementation Architecture

### Phase 1: Basic Live Streaming (No AI)
```cpp
class ZEDLiveStreamer {
private:
    sl::Camera zed_;
    cv::VideoWriter stream_encoder_;
    
public:
    bool initStreaming(StreamMode mode) {
        // Configure for streaming instead of recording
        sl::InitParameters init_params;
        init_params.camera_resolution = sl::RESOLUTION::HD720;
        init_params.camera_fps = 15;
        init_params.depth_mode = sl::DEPTH_MODE::PERFORMANCE; // Faster than NEURAL
        
        // Configure encoder for streaming
        int fourcc = cv::VideoWriter::fourcc('H','2','6','4');
        stream_encoder_.open("rtmp://stream-server/live", fourcc, 15, cv::Size(1280, 720));
    }
    
    void streamLoop() {
        sl::Mat zed_frame;
        cv::Mat cv_frame;
        
        while (streaming_) {
            if (zed_.grab() == sl::ERROR_CODE::SUCCESS) {
                zed_.retrieveImage(zed_frame, sl::VIEW::LEFT);
                
                // Convert ZED format to OpenCV
                cv_frame = slMat2cvMat(zed_frame);
                
                // Write to stream
                stream_encoder_.write(cv_frame);
            }
        }
    }
};
```

### Phase 2: AI Object Detection Integration
```cpp
class AILiveStreamer : public ZEDLiveStreamer {
private:
    std::unique_ptr<ObjectDetector> ai_model_;
    cv::Mat overlay_frame_;
    
public:
    void streamWithAI() {
        sl::Mat zed_frame;
        cv::Mat cv_frame;
        
        while (streaming_) {
            if (zed_.grab() == sl::ERROR_CODE::SUCCESS) {
                zed_.retrieveImage(zed_frame, sl::VIEW::LEFT);
                cv_frame = slMat2cvMat(zed_frame);
                
                // Run AI inference (optimized for real-time)
                auto detections = ai_model_->detect(cv_frame);
                
                // Draw bounding boxes and labels
                drawDetections(cv_frame, detections);
                
                // Add telemetry overlay
                addTelemetryOverlay(cv_frame);
                
                // Stream the annotated frame
                stream_encoder_.write(cv_frame);
            }
        }
    }
    
private:
    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections) {
        for (const auto& det : detections) {
            // Bounding box
            cv::rectangle(frame, det.bbox, cv::Scalar(0, 255, 0), 2);
            
            // Label with confidence
            std::string label = det.class_name + " " + std::to_string(det.confidence);
            cv::putText(frame, label, det.bbox.tl(), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        }
    }
};
```

### Phase 3: Advanced Streaming Architecture
```cpp
class ProductionStreamer {
private:
    // Multi-threaded architecture for performance
    std::thread capture_thread_;
    std::thread ai_thread_;
    std::thread stream_thread_;
    
    // Lock-free queues for frame passing
    boost::lockfree::queue<FrameData> capture_queue_;
    boost::lockfree::queue<FrameData> ai_queue_;
    boost::lockfree::queue<FrameData> stream_queue_;
    
public:
    void startProductionStream() {
        // Thread 1: High-frequency camera capture
        capture_thread_ = std::thread([this]() {
            while (running_) {
                captureFrame(); // 15fps, minimal processing
            }
        });
        
        // Thread 2: AI processing (can skip frames if behind)
        ai_thread_ = std::thread([this]() {
            while (running_) {
                processAI(); // Run inference, add overlays
            }
        });
        
        // Thread 3: Network streaming
        stream_thread_ = std::thread([this]() {
            while (running_) {
                streamFrame(); // Encode and transmit
            }
        });
    }
};
```

## Streaming Protocols & Infrastructure

### Protocol Options
1. **RTMP** (Real-Time Messaging Protocol)
   - Good for YouTube, Twitch, custom servers
   - ~2-5 second latency
   - Requires streaming server

2. **WebRTC** (Web Real-Time Communication)
   - Ultra-low latency (<100ms)
   - Direct browser connection
   - More complex implementation

3. **UDP Multicast**
   - Lowest latency (<50ms)
   - Best for local networks
   - No internet streaming

### Infrastructure Requirements
```yaml
# Streaming Server Setup (nginx-rtmp)
rtmp {
    server {
        listen 1935;
        application live {
            live on;
            record off;
            allow publish 192.168.1.0/24; # Drone IP range
            allow play all;
        }
    }
}

# Ground Station Requirements
- CPU: Multi-core for H.264 encoding
- GPU: Hardware encoding support (NVENC on Jetson)
- Network: Minimum 5 Mbps upload, stable connection
- Storage: Minimal (streaming only, no local recording)
```

## Integration with Current System

### Modify Existing Architecture
```cpp
// In zed_recorder.h - add streaming mode
enum class OperationMode {
    RECORDING_ONLY,     // Current system
    STREAMING_ONLY,     // New AI deployment mode
    RECORDING_AND_STREAMING  // Hybrid for development
};

class ZEDRecorder {
public:
    // New methods for streaming mode
    bool initStreaming(const std::string& stream_url, OperationMode mode);
    bool startStreaming();
    void stopStreaming();
    
    // AI integration
    bool loadAIModel(const std::string& model_path);
    bool enableObjectDetection(bool enable = true);
    
private:
    OperationMode current_mode_;
    std::unique_ptr<AIProcessor> ai_processor_;
    std::unique_ptr<StreamEncoder> stream_encoder_;
};
```

## Performance Optimizations

### GPU Acceleration (Jetson Orin Nano)
```cpp
// Use hardware encoding for streaming
cv::VideoWriter stream_encoder_;
// Enable NVENC hardware encoder
int fourcc = cv::VideoWriter::fourcc('H','2','6','4');
stream_encoder_.open("rtmp://server/live", fourcc, 15, cv::Size(1280, 720), {
    cv::VIDEOWRITER_PROP_HW_ACCELERATION, cv::VIDEO_ACCELERATION_ANY
});
```

### AI Model Optimization
- **TensorRT optimization** for Jetson inference
- **Model quantization** (FP16/INT8) for speed
- **Frame skipping** if AI processing falls behind
- **ROI processing** - only run AI on important image regions

## Deployment Strategy

### Development Phase
1. **Local streaming** - test on same network
2. **Basic AI overlay** - bounding boxes only
3. **Performance profiling** - ensure 15fps stability

### Production Phase  
1. **Cellular/LTE streaming** - field deployment
2. **Advanced AI features** - classification, tracking
3. **Telemetry integration** - GPS, altitude, battery status
4. **Failover systems** - backup streaming servers

## Bandwidth Summary

**Recommended minimum upload speeds:**
- **Development/Testing**: 5-8 Mbps
- **Production deployment**: 10-15 Mbps  
- **Professional operations**: 20+ Mbps with redundancy

The key is having **stable** bandwidth rather than peak speed - consistent 5 Mbps is better than variable 20 Mbps connection.