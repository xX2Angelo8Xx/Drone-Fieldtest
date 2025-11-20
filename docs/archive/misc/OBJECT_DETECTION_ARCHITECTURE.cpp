/*
 * DUAL-MODE ZED SYSTEM ARCHITECTURE
 * =================================
 * 
 * MODE 1: RECORDING (Current - Gap-free at 15fps)
 * - HD720@15fps LOSSLESS recording
 * - Gap-free file storage for post-analysis
 * - 2.17GB per 2 minutes
 * 
 * MODE 2: REAL-TIME OBJECT DETECTION (Future - 30fps)
 * - HD720@30fps live processing (no recording)
 * - GPU-based object detection/tracking
 * - Results: JSON/CSV coordinates, classifications
 * - ~1MB/s data output vs 17MB/s for recording
 * 
 * MODE 3: HYBRID (Advanced - Both simultaneously)
 * - HD720@15fps recording + 30fps object detection
 * - Record gap-free footage + real-time AI analysis
 * - Best of both worlds
 */

// Example ZED Object Detection at 30fps
#include <sl/Camera.hpp>

class DroneObjectDetector {
private:
    sl::Camera zed_;
    sl::ObjectDetectionParameters obj_det_params_;
    sl::Objects objects_;
    
public:
    bool init() {
        // Initialize for 30fps object detection (no recording)
        sl::InitParameters init_params;
        init_params.camera_resolution = sl::RESOLUTION::HD720;
        init_params.camera_fps = 30;  // Full 30fps for object detection
        init_params.depth_mode = sl::DEPTH_MODE::ULTRA;  // Enable depth for 3D tracking
        
        if (zed_.open(init_params) != sl::ERROR_CODE::SUCCESS) {
            return false;
        }
        
        // Configure object detection
        obj_det_params_.detection_model = sl::OBJECT_DETECTION_MODEL::MULTI_CLASS_BOX_FAST;
        obj_det_params_.enable_tracking = true;
        obj_det_params_.enable_mask_output = false;  // Faster without masks
        
        return zed_.enableObjectDetection(obj_det_params_) == sl::ERROR_CODE::SUCCESS;
    }
    
    void processFrame() {
        if (zed_.grab() == sl::ERROR_CODE::SUCCESS) {
            // Object detection at 30fps - very fast operation
            if (zed_.retrieveObjects(objects_) == sl::ERROR_CODE::SUCCESS) {
                
                // Process detected objects (minimal CPU load)
                for (auto& obj : objects_.object_list) {
                    // Real-time processing: 
                    // - 3D position: obj.position
                    // - Classification: obj.label  
                    // - Confidence: obj.confidence
                    // - Tracking ID: obj.id
                    
                    logDetection(obj);  // ~1KB per detection vs 17MB/s recording
                }
            }
        }
    }
};

// Performance comparison:
// Recording:         17 MB/s → USB storage bottleneck
// Object Detection:  ~1 MB/s → GPU processing, minimal I/O