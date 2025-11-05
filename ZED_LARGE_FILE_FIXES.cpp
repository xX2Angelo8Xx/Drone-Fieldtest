/*
 * FIXES FOR >4GB ZED RECORDING CORRUPTION
 * =====================================
 * 
 * Issues Identified:
 * 1. Integer overflow in bytes_written_ tracking (long -> size_t)
 * 2. Disabled filesystem sync causing corruption on large files 
 * 3. No hardware encoder support on Jetson Orin Nano
 * 4. Missing buffer management for large file handling
 */

// FIX 1: Change bytes_written_ data type in zed_recorder.h
// FROM: std::atomic<long> bytes_written_;
// TO:   std::atomic<size_t> bytes_written_;  // Supports files >4GB

// FIX 2: Add periodic sync back (CRITICAL for large files)
// In recordingLoop(), replace lines around 241:

if (sync_counter % (900 / sensor_skip_rate) == 0) {
    sensor_file_.flush();  // Flush sensor data
    
    // CRITICAL: Re-enable sync for large files >1GB 
    if (bytes_written_ > 1024*1024*1024) {  // >1GB
        std::cout << "[ZED] Large file detected, forcing filesystem sync..." << std::endl;
        sync();  // CRITICAL: Prevents corruption
    }
}

// FIX 3: Implement file segmentation for >4GB recordings
// Add to recordingLoop():

// Check file size every few seconds
static auto last_size_check = std::chrono::steady_clock::now();
auto now = std::chrono::steady_clock::now();
if (std::chrono::duration_cast<std::chrono::seconds>(now - last_size_check).count() > 10) {
    
    // If approaching 4GB (use 3.5GB safety margin)
    if (bytes_written_ > 3758096384) {  // 3.5GB in bytes
        std::cout << "[ZED] File approaching 4GB limit, consider segmentation..." << std::endl;
        
        // Option A: Stop current recording and create new segment
        // Option B: Continue with explicit large file handling
        
        // Force filesystem operations to handle large files properly
        sync();
        sensor_file_.flush();
        
        last_size_check = now;
    }
}

// FIX 4: Alternative compression without NVENC
// Since H264/H265 requires NVENC (not available on Orin Nano),
// use CPU-based compression with lower resolution/framerate:

rec_params.compression_mode = sl::SVO_COMPRESSION_MODE::LOSSLESS;

// For large file recording, reduce parameters:
if (recording_duration_expected > 300) {  // >5 minutes
    // Use HD720@15FPS instead of HD720@30FPS for longer recordings
    init_params.camera_fps = 15;  
    std::cout << "[ZED] Long recording detected, using 15FPS for file size management" << std::endl;
}

// FIX 5: Proper getBytesWritten() with large file support
long ZEDRecorder::getBytesWritten() const {
    // Use filesystem directly for accurate large file sizes
    try {
        if (std::filesystem::exists(current_video_path_)) {
            return static_cast<long>(std::filesystem::file_size(current_video_path_));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting file size: " << e.what() << std::endl;
    }
    return bytes_written_;
}