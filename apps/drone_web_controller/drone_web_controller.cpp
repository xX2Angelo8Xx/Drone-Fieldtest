#include "drone_web_controller.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <sl/Camera.hpp>

namespace fs = std::filesystem;

// Static instance for signal handling
DroneWebController* DroneWebController::instance_ = nullptr;

DroneWebController::DroneWebController() {
    instance_ = this;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

DroneWebController::~DroneWebController() {
    handleShutdown();
}

bool DroneWebController::initialize() {
    std::cout << "[WEB_CONTROLLER] Initializing..." << std::endl;
    
    try {
        // Initialize LCD display FIRST for user feedback
        lcd_ = std::make_unique<LCDHandler>();
        if (!lcd_->init()) {
            std::cout << "[WEB_CONTROLLER] LCD initialization failed" << std::endl;
            return false;
        }
        
        // Minimal bootup message: Just "Starting..."
        lcd_->displayMessage("Starting...", "");
        
        // CRITICAL: Set depth mode to NONE for SVO2 only startup (save Jetson resources)
        // Will auto-switch to NEURAL_PLUS when user selects depth recording modes
        depth_mode_ = DepthMode::NONE;
        std::cout << "[WEB_CONTROLLER] Default recording mode: SVO2 only (depth: NONE, compute later on PC)" << std::endl;
        
        // Initialize ZED recorders based on mode
        // For now, initialize the default SVO2 recorder
        // Raw recorder will be initialized on-demand when mode is switched
        svo_recorder_ = std::make_unique<ZEDRecorder>();
        if (!svo_recorder_->init(camera_resolution_)) {  // Use member variable instead of default
            std::cout << "[WEB_CONTROLLER] ZED camera initialization failed" << std::endl;
            lcd_->displayMessage("ERROR", "Camera Failed");
            return false;
        }
        std::cout << "[WEB_CONTROLLER] ZED camera initialized with resolution: " 
                  << svo_recorder_->getModeName(camera_resolution_) << std::endl;
        
        // Set smart default exposure based on FPS
        // For 60 FPS: Use 1/120 shutter (50% exposure) for good motion capture
        if (camera_resolution_ == RecordingMode::HD720_60FPS || 
            camera_resolution_ == RecordingMode::VGA_100FPS) {
            svo_recorder_->setCameraExposure(50);  // 1/120 at 60fps, 1/200 at 100fps
            std::cout << "[WEB_CONTROLLER] Set default exposure: 50% (1/120 shutter @ 60fps)" << std::endl;
        }
        
        // Initialize storage
        storage_ = std::make_unique<StorageHandler>();
        if (!storage_->findAndMountUSB("DRONE_DATA")) {
            std::cout << "[WEB_CONTROLLER] USB storage not detected" << std::endl;
            lcd_->displayMessage("ERROR", "No USB Storage");
            return false;
        }
        
        // Start system monitor thread
        system_monitor_thread_ = std::make_unique<std::thread>(&DroneWebController::systemMonitorLoop, this);
        
        // Final bootup message: Ready with web address (keep visible before status display)
        updateLCD("Ready!", "10.42.0.1:8080");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        std::cout << "[WEB_CONTROLLER] Initialization complete" << std::endl;
        std::cout << "[WEB_CONTROLLER] Camera: " << svo_recorder_->getModeName(camera_resolution_) << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "[WEB_CONTROLLER] Initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool DroneWebController::startRecording() {
    if (recording_active_) {
        return false;
    }
    
    std::cout << std::endl << "[WEB_CONTROLLER] Starting recording..." << std::endl;
    std::cout << "[WEB_CONTROLLER] Mode: ";
    switch (recording_mode_) {
        case RecordingModeType::SVO2: std::cout << "SVO2"; break;
        case RecordingModeType::SVO2_DEPTH_INFO:
            std::cout << "SVO2 + Depth Info (" << depth_recording_fps_.load() << " FPS raw data)";
            break;
        case RecordingModeType::SVO2_DEPTH_IMAGES: 
            std::cout << "SVO2 + Depth Images (" << depth_recording_fps_.load() << " FPS viz)"; 
            break;
        case RecordingModeType::RAW_FRAMES: std::cout << "RAW_FRAMES"; break;
    }
    std::cout << std::endl;
    
    // Single consolidated LCD message - avoid rapid updates
    updateLCD("Recording", "Starting...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Let message be visible
    
    // Branch based on recording mode
    if (recording_mode_ == RecordingModeType::SVO2 || 
        recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
        recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
        
        // Check if we need to reinitialize camera with depth mode
        bool needs_reinit = false;
        if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
            recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
            if (!svo_recorder_->isDepthComputationEnabled()) {
                needs_reinit = true;
                std::cout << "[WEB_CONTROLLER] Camera needs reinitialization with depth mode" << std::endl;
            }
        }
        
        // Reinitialize camera if depth mode changed
        if (needs_reinit) {
            updateLCD("Reinitializing", "Camera...");  // Consolidated message
            
            // Close and destroy the old recorder
            svo_recorder_->close();
            svo_recorder_.reset();
            
            // CRITICAL: Give ZED SDK time to release USB resources
            // ZED 2i needs minimum 3 seconds to fully release hardware
            std::cout << "[WEB_CONTROLLER] Waiting 3s for camera hardware to release..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Don't update LCD during init - keep "Reinitializing Camera..." visible
            
            // Create new recorder
            svo_recorder_ = std::make_unique<ZEDRecorder>();
            
            // Enable depth BEFORE init
            sl::DEPTH_MODE zed_depth_mode = convertDepthMode(depth_mode_);
            svo_recorder_->enableDepthComputation(true, zed_depth_mode);
            
            // Try initialization with retry (corrupted frames can happen on first attempt)
            bool init_success = false;
            for (int attempt = 1; attempt <= 3; attempt++) {
                std::cout << "[WEB_CONTROLLER] Camera init attempt " << attempt << "/3..." << std::endl;
                
                if (svo_recorder_->init(camera_resolution_)) {  // Use saved resolution!
                    init_success = true;
                    std::cout << "[WEB_CONTROLLER] Camera initialized successfully" << std::endl;
                    break;
                }
                
                if (attempt < 3) {
                    std::cout << "[WEB_CONTROLLER] Init failed, waiting 2s before retry..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            
            if (!init_success) {
                std::cerr << "[WEB_CONTROLLER] Failed to reinitialize camera with depth after 3 attempts" << std::endl;
                updateLCD("Init Error", "Camera failed");
                std::this_thread::sleep_for(std::chrono::seconds(2));  // Keep error visible
                return false;
            }
            
            std::cout << "[WEB_CONTROLLER] Camera reinitialized: " << svo_recorder_->getModeName(camera_resolution_)
                      << " with depth mode: " << getDepthModeName(depth_mode_) << std::endl;
        }
        
        // SVO2 Recording Modes
        if (!storage_->createRecordingDir()) {
            std::cout << "[WEB_CONTROLLER] Failed to create recording directory" << std::endl;
            updateLCD("Recording Error", "Dir Failed");
            return false;
        }
        
        std::string video_path = storage_->getVideoPath();
        std::string sensor_path = storage_->getSensorDataPath();
        
        // Depth data recording setup (SVO2_DEPTH_INFO mode)
        if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO) {
            std::cout << "[WEB_CONTROLLER] SVO2_DEPTH_INFO mode: Raw 32-bit depth data recording" << std::endl;
            
            // Create depth_data directory
            std::string depth_data_dir = storage_->getRecordingDir() + "/depth_data";
            if (!fs::create_directories(depth_data_dir)) {
                std::cout << "[WEB_CONTROLLER] Warning: depth_data directory already exists" << std::endl;
            }
            std::cout << "[WEB_CONTROLLER] Depth data directory: " << depth_data_dir << std::endl;
            
            // Initialize DepthDataWriter
            depth_data_writer_ = std::make_unique<DepthDataWriter>();
            if (!depth_data_writer_->init(depth_data_dir, depth_recording_fps_.load())) {
                std::cout << "[WEB_CONTROLLER] Failed to initialize DepthDataWriter" << std::endl;
                updateLCD("Recording Error", "Depth Init Fail");
                depth_data_writer_.reset();
                return false;
            }
            std::cout << "[WEB_CONTROLLER] DepthDataWriter initialized (target: " 
                      << depth_recording_fps_.load() << " FPS)" << std::endl;
        }
        
        // Depth visualization setup (SVO2_DEPTH_IMAGES mode)
        if (recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
            int fps = depth_recording_fps_.load();
            if (fps > 0) {
                std::cout << "[WEB_CONTROLLER] Depth visualization enabled (" << fps << " FPS)" << std::endl;
                
                // Create depth_viz directory
                std::string depth_dir = storage_->getRecordingDir() + "/depth_viz";
                fs::create_directories(depth_dir);
                std::cout << "[WEB_CONTROLLER] Depth visualization directory: " << depth_dir << std::endl;
            } else {
                std::cout << "[WEB_CONTROLLER] Depth computation enabled (visualization disabled - 0 FPS)" << std::endl;
            }
            
            // Thread will be started after recording_active_ is set to true
        }
        
        if (!svo_recorder_->startRecording(video_path, sensor_path)) {
            std::cout << "[WEB_CONTROLLER] Failed to start SVO2 recording" << std::endl;
            updateLCD("Recording Error", "ZED Failed");
            return false;
        }
        
        // Start DepthDataWriter after SVO recording is running (SVO2_DEPTH_INFO mode)
        if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO && depth_data_writer_) {
            depth_data_writer_->start(*svo_recorder_->getCamera());
            std::cout << "[WEB_CONTROLLER] DepthDataWriter started successfully" << std::endl;
        }
        
        current_recording_path_ = video_path;
        std::cout << "[WEB_CONTROLLER] SVO2 Recording started: " << video_path << std::endl;
        if (recording_mode_ != RecordingModeType::SVO2) {
            std::cout << "[WEB_CONTROLLER] Depth mode: " << getDepthModeName(depth_mode_) << std::endl;
        }
        
    } else {
        // RAW FRAMES Recording Mode
        
        // Initialize raw recorder if not already done
        if (!raw_recorder_) {
            raw_recorder_ = std::make_unique<RawFrameRecorder>();
            if (!raw_recorder_->init(RecordingMode::HD720_30FPS, depth_mode_)) {
                std::cout << "[WEB_CONTROLLER] Failed to initialize raw recorder" << std::endl;
                updateLCD("Recording Error", "Init Failed");
                return false;
            }
        }
        
        // Create directory structure for raw frames
        if (!storage_->createRawRecordingStructure()) {
            std::cout << "[WEB_CONTROLLER] Failed to create raw recording structure" << std::endl;
            updateLCD("Recording Error", "Dir Failed");
            return false;
        }
        
        std::string base_dir = storage_->getRecordingDir();
        
        if (!raw_recorder_->startRecording(base_dir)) {
            std::cout << "[WEB_CONTROLLER] Failed to start raw frame recording" << std::endl;
            updateLCD("Recording Error", "Raw Failed");
            return false;
        }
        
        current_recording_path_ = base_dir;
        std::cout << "[WEB_CONTROLLER] RAW Recording started: " << base_dir << std::endl;
        std::cout << "[WEB_CONTROLLER] Depth mode: " 
                  << raw_recorder_->getDepthModeName(depth_mode_) << std::endl;
    }
    
    recording_active_ = true;
    current_state_ = RecorderState::RECORDING;
    recording_start_time_ = std::chrono::steady_clock::now();
    
    // Start depth visualization thread if in SVO2 + Depth Viz mode
    if (recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
        depth_viz_running_ = true;
        depth_viz_thread_ = std::make_unique<std::thread>(&DroneWebController::depthVisualizationLoop, this);
        std::cout << "[WEB_CONTROLLER] Depth visualization thread started" << std::endl;
    }
    
    // Start recording monitor thread
    recording_monitor_thread_ = std::make_unique<std::thread>(&DroneWebController::recordingMonitorLoop, this);
    
    updateLCD("Recording", "Active");
    
    return true;
}

bool DroneWebController::stopRecording() {
    if (!recording_active_) {
        return false;
    }
    
    std::cout << std::endl << "[WEB_CONTROLLER] Stopping recording..." << std::endl;
    updateLCD("Stopping", "Recording...");
    
    current_state_ = RecorderState::STOPPING;
    
    // Stop appropriate recorder based on mode
    if (recording_mode_ == RecordingModeType::SVO2 ||
        recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
        recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
        
        // Stop depth data writer if running (SVO2_DEPTH_INFO mode)
        if (depth_data_writer_) {
            std::cout << "[WEB_CONTROLLER] Stopping DepthDataWriter..." << std::endl;
            depth_data_writer_->stop();
            int frame_count = depth_data_writer_->getFrameCount();
            std::cout << "[WEB_CONTROLLER] DepthDataWriter stopped. Total frames: " << frame_count << std::endl;
            depth_data_writer_.reset();
        }
        
        // Stop depth visualization thread if running (SVO2_DEPTH_IMAGES mode)
        if (depth_viz_running_) {
            std::cout << "[WEB_CONTROLLER] Stopping depth visualization thread..." << std::endl;
            depth_viz_running_ = false;
            if (depth_viz_thread_ && depth_viz_thread_->joinable()) {
                depth_viz_thread_->join();
                depth_viz_thread_.reset();
            }
            std::cout << "[WEB_CONTROLLER] Depth visualization thread stopped" << std::endl;
        }
        
        if (svo_recorder_) {
            svo_recorder_->stopRecording();
            // Disable depth computation for next recording
            svo_recorder_->enableDepthComputation(false);
        }
    } else {
        if (raw_recorder_) {
            raw_recorder_->stopRecording();
        }
    }
    
    // Signal recording thread to stop first
    recording_active_ = false;
    
    // Wait for recording monitor thread to finish (after setting flags)
    if (recording_monitor_thread_ && recording_monitor_thread_->joinable()) {
        recording_monitor_thread_->join();
        recording_monitor_thread_.reset();  // Clean up thread pointer
    }
    
    // Show "Recording Stopped" message
    updateLCD("Recording", "Stopped");
    
    // Set state to IDLE only after a delay (let "Stopped" message stay visible)
    // Use a timer that systemMonitorLoop can check
    recording_stopped_time_ = std::chrono::steady_clock::now();
    
    std::cout << "[WEB_CONTROLLER] Recording stopped" << std::endl;
    
    // State will be set to IDLE by systemMonitorLoop after message is visible
    
    return true;
}

bool DroneWebController::shutdownSystem() {
    std::cout << std::endl << "[WEB_CONTROLLER] System shutdown requested" << std::endl;
    updateLCD("System", "Shutting Down");
    
    handleShutdown();
    
    std::cout << "[WEB_CONTROLLER] Executing system shutdown..." << std::endl;
    system("sudo shutdown -h now");
    
    return true;
}

RecordingStatus DroneWebController::getStatus() const {
    RecordingStatus status;
    status.state = current_state_;
    status.current_file_path = current_recording_path_;
    status.error_message = "";
    status.recording_mode = recording_mode_;
    status.frame_count = 0;
    status.current_fps = 0.0f;
    status.depth_fps = 0.0f;
    status.camera_initializing = camera_initializing_;
    
    // Get status message
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(status_mutex_));
        status.status_message = status_message_;
    }
    
    // Set depth mode name
    if (recording_mode_ == RecordingModeType::RAW_FRAMES && raw_recorder_) {
        status.depth_mode = raw_recorder_->getDepthModeName(depth_mode_);
    } else if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
               recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
        status.depth_mode = getDepthModeName(depth_mode_);
    } else {
        status.depth_mode = "N/A";
    }
    
    if (current_state_ == RecorderState::RECORDING && recording_active_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        status.recording_time_remaining = recording_duration_seconds_ - elapsed;
        status.recording_duration_total = recording_duration_seconds_;
        
        // Get stats based on recording mode
        if (recording_mode_ == RecordingModeType::SVO2 ||
            recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
            recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
            if (svo_recorder_) {
                status.bytes_written = svo_recorder_->getBytesWritten();
                
                // Get depth FPS if depth computation is enabled
                if (svo_recorder_->isDepthComputationEnabled()) {
                    status.depth_fps = svo_recorder_->getDepthComputationFPS();
                }
                
                if (elapsed > 0) {
                    status.mb_per_second = (status.bytes_written / 1024.0 / 1024.0) / elapsed;
                } else {
                    status.mb_per_second = 0.0;
                }
            } else {
                status.bytes_written = 0;
                status.mb_per_second = 0.0;
            }
        } else {
            // RAW_FRAMES mode
            if (raw_recorder_) {
                status.bytes_written = raw_recorder_->getBytesWritten();
                status.frame_count = raw_recorder_->getFrameCount();
                status.current_fps = raw_recorder_->getCurrentFPS();
                
                if (elapsed > 0) {
                    status.mb_per_second = (status.bytes_written / 1024.0 / 1024.0) / elapsed;
                } else {
                    status.mb_per_second = 0.0;
                }
            } else {
                status.bytes_written = 0;
                status.mb_per_second = 0.0;
            }
        }
    } else {
        status.recording_time_remaining = 0;
        status.recording_duration_total = recording_duration_seconds_;
        status.bytes_written = 0;
        status.mb_per_second = 0.0;
    }
    
    return status;
}

void DroneWebController::setRecordingMode(RecordingModeType mode) {
    if (recording_active_) {
        std::cerr << "[WEB_CONTROLLER] Cannot change recording mode while recording" << std::endl;
        return;
    }
    
    RecordingModeType old_mode = recording_mode_;
    recording_mode_ = mode;
    
    std::cout << "[WEB_CONTROLLER] Recording mode change: ";
    switch(mode) {
        case RecordingModeType::SVO2: std::cout << "SVO2"; break;
        case RecordingModeType::SVO2_DEPTH_INFO: std::cout << "SVO2 + Depth Info"; break;
        case RecordingModeType::SVO2_DEPTH_IMAGES: std::cout << "SVO2 + Depth Images"; break;
        case RecordingModeType::RAW_FRAMES: std::cout << "RAW_FRAMES"; break;
    }
    std::cout << std::endl;
    
    // CRITICAL: Automatically set depth mode based on recording mode
    // This prevents "depth map not computed" errors and saves resources
    if (mode == RecordingModeType::SVO2) {
        // SVO2 only: No depth needed (compute later on PC to save Jetson resources)
        if (depth_mode_ != DepthMode::NONE) {
            std::cout << "[WEB_CONTROLLER] Auto-switching depth mode to NONE (SVO2 only)" << std::endl;
            depth_mode_ = DepthMode::NONE;
        }
    } else if (mode == RecordingModeType::SVO2_DEPTH_INFO || mode == RecordingModeType::SVO2_DEPTH_IMAGES || mode == RecordingModeType::RAW_FRAMES) {
        // Depth recording modes: Use best quality depth (NEURAL_PLUS) if currently NONE
        if (depth_mode_ == DepthMode::NONE) {
            std::cout << "[WEB_CONTROLLER] Auto-switching depth mode to NEURAL_PLUS (best quality)" << std::endl;
            depth_mode_ = DepthMode::NEURAL_PLUS;
        }
    }
    
    // Show consolidated message on LCD (avoid spam)
    updateLCD("Mode Change", "Reinitializing...");
    
    bool needs_reinit = false;
    
    // If switching TO RAW mode from SVO mode
    if (mode == RecordingModeType::RAW_FRAMES && svo_recorder_) {
        std::cout << "[WEB_CONTROLLER] Switching from SVO to RAW mode - reinitializing..." << std::endl;
        svo_recorder_->close();
        svo_recorder_.reset();
        needs_reinit = true;
    }
    // If switching TO SVO mode from RAW mode
    else if (mode != RecordingModeType::RAW_FRAMES && old_mode == RecordingModeType::RAW_FRAMES && raw_recorder_) {
        std::cout << "[WEB_CONTROLLER] Switching from RAW to SVO mode - reinitializing..." << std::endl;
        raw_recorder_->close();
        raw_recorder_.reset();
        needs_reinit = true;
    }
    // If switching TO SVO2 only from depth modes (need to disable depth)
    else if (mode == RecordingModeType::SVO2 && (old_mode == RecordingModeType::SVO2_DEPTH_INFO || old_mode == RecordingModeType::SVO2_DEPTH_IMAGES)) {
        std::cout << "[WEB_CONTROLLER] Switching from SVO2+Depth to SVO2 only - reinitializing without depth..." << std::endl;
        if (svo_recorder_) {
            svo_recorder_->close();
            svo_recorder_.reset();
        }
        needs_reinit = true;
    }
    // If switching between SVO depth modes
    else if (old_mode != mode && (mode == RecordingModeType::SVO2_DEPTH_INFO || mode == RecordingModeType::SVO2_DEPTH_IMAGES)) {
        std::cout << "[WEB_CONTROLLER] Switching SVO depth mode - reinitializing..." << std::endl;
        if (svo_recorder_) {
            svo_recorder_->close();
            svo_recorder_.reset();
        }
        needs_reinit = true;
    }
    
    // Reinitialize with SAVED camera_resolution_ (preserve FPS settings!)
    if (needs_reinit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Brief pause for hardware
        
        if (mode == RecordingModeType::RAW_FRAMES) {
            raw_recorder_ = std::make_unique<RawFrameRecorder>();
            if (!raw_recorder_->init(camera_resolution_, depth_mode_)) {
                std::cerr << "[WEB_CONTROLLER] Failed to initialize RAW recorder" << std::endl;
                updateLCD("Init Error", "RAW Failed");
            } else {
                std::cout << "[WEB_CONTROLLER] RAW recorder initialized successfully" << std::endl;
                updateLCD("RAW Mode", "Ready");
            }
        } else {
            svo_recorder_ = std::make_unique<ZEDRecorder>();
            
            // Enable depth computation ONLY if depth recording is needed
            // For SVO2 only: depth_mode_ should be NONE (set automatically above)
            if (mode == RecordingModeType::SVO2_DEPTH_INFO || mode == RecordingModeType::SVO2_DEPTH_IMAGES) {
                sl::DEPTH_MODE zed_depth_mode = convertDepthMode(depth_mode_);
                svo_recorder_->enableDepthComputation(true, zed_depth_mode);
                std::cout << "[WEB_CONTROLLER] Depth computation ENABLED with mode: " 
                          << getDepthModeName(depth_mode_) << std::endl;
            } else {
                // SVO2 only: Explicitly disable depth (save Jetson resources)
                std::cout << "[WEB_CONTROLLER] Depth computation DISABLED (SVO2 only, compute later on PC)" << std::endl;
            }
            
            if (!svo_recorder_->init(camera_resolution_)) {
                std::cerr << "[WEB_CONTROLLER] Failed to initialize SVO recorder" << std::endl;
                updateLCD("Init Error", "SVO Failed");
            } else {
                std::cout << "[WEB_CONTROLLER] SVO recorder initialized with: " << svo_recorder_->getModeName(camera_resolution_) << std::endl;
                updateLCD("SVO Mode", "Ready");
            }
        }
        
        // Show success message longer for visibility
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void DroneWebController::setCameraResolution(RecordingMode mode) {
    if (recording_active_) {
        std::cerr << "[WEB_CONTROLLER] Cannot change resolution while recording" << std::endl;
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "Cannot change resolution during recording";
        return;
    }
    
    // Save current exposure setting before reinit
    int current_exposure = getCameraExposure();
    
    // Store new resolution setting
    camera_resolution_ = mode;
    
    camera_initializing_ = true;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "Reinitializing camera with new resolution...";
    }
    
    updateLCD("Camera Init", "New resolution");
    
    std::cout << "[WEB_CONTROLLER] Changing camera resolution/FPS to: " 
              << svo_recorder_->getModeName(mode) << std::endl;
    
    // Close and reinitialize camera
    if (svo_recorder_) {
        svo_recorder_->close();
        svo_recorder_.reset();
    }
    if (raw_recorder_) {
        raw_recorder_->close();
        raw_recorder_.reset();
    }
    
    // Reinitialize with new mode
    if (recording_mode_ == RecordingModeType::RAW_FRAMES) {
        raw_recorder_ = std::make_unique<RawFrameRecorder>();
        if (!raw_recorder_->init(mode, depth_mode_)) {
            std::cerr << "[WEB_CONTROLLER] Failed to reinitialize RAW recorder" << std::endl;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Camera initialization failed!";
            camera_initializing_ = false;
            updateLCD("Init Error", "Camera failed");
            return;
        }
    } else {
        svo_recorder_ = std::make_unique<ZEDRecorder>();
        
        // Re-enable depth computation if needed (for both DEPTH_INFO and DEPTH_IMAGES modes)
        if (recording_mode_ == RecordingModeType::SVO2_DEPTH_INFO ||
            recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
            sl::DEPTH_MODE zed_depth_mode = convertDepthMode(depth_mode_);
            svo_recorder_->enableDepthComputation(true, zed_depth_mode);
        }
        
        if (!svo_recorder_->init(mode)) {
            std::cerr << "[WEB_CONTROLLER] Failed to reinitialize SVO recorder" << std::endl;
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Camera initialization failed!";
            camera_initializing_ = false;
            updateLCD("Init Error", "Camera failed");
            return;
        }
    }
    
    camera_initializing_ = false;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "Camera reinitialized successfully";
    }
    
    // Reapply exposure setting after reinit
    if (current_exposure != -1) {
        setCameraExposure(current_exposure);
        std::cout << "[WEB_CONTROLLER] Reapplied exposure: " << current_exposure << std::endl;
    }
    
    updateLCD("Camera Ready", svo_recorder_->getModeName(mode).c_str());
    std::cout << "[WEB_CONTROLLER] Camera resolution changed successfully" << std::endl;
}

void DroneWebController::setCameraExposure(int exposure) {
    if (svo_recorder_ && svo_recorder_->setCameraExposure(exposure)) {
        std::cout << "[WEB_CONTROLLER] Exposure set to: " << exposure << std::endl;
    } else if (raw_recorder_ && raw_recorder_->setCameraExposure(exposure)) {
        std::cout << "[WEB_CONTROLLER] Exposure set to: " << exposure << std::endl;
    } else {
        std::cerr << "[WEB_CONTROLLER] Failed to set exposure" << std::endl;
    }
}

RecordingMode DroneWebController::getCameraResolution() {
    // Return stored resolution setting (more reliable than querying recorder)
    return camera_resolution_;
}

int DroneWebController::getCameraExposure() {
    if (svo_recorder_) {
        return svo_recorder_->getCameraExposure();
    } else if (raw_recorder_) {
        return raw_recorder_->getCameraExposure();
    }
    return -1;  // Auto
}

void DroneWebController::setCameraGain(int gain) {
    if (svo_recorder_ && svo_recorder_->setCameraGain(gain)) {
        std::cout << "[WEB_CONTROLLER] Gain set to: " << gain << std::endl;
    } else if (raw_recorder_ && raw_recorder_->setCameraGain(gain)) {
        std::cout << "[WEB_CONTROLLER] Gain set to: " << gain << std::endl;
    } else {
        std::cerr << "[WEB_CONTROLLER] Failed to set gain" << std::endl;
    }
}

int DroneWebController::getCameraGain() {
    if (svo_recorder_) {
        return svo_recorder_->getCameraGain();
    } else if (raw_recorder_) {
        return raw_recorder_->getCameraGain();
    }
    return -1;  // Auto
}

std::string DroneWebController::exposureToShutterSpeed(int exposure, int fps) {
    // Convert exposure percentage back to shutter speed notation
    // Formula: shutter = FPS / (exposure / 100)
    
    if (exposure <= 0) {
        return "Auto";
    }
    
    // Calculate shutter denominator
    int shutter = static_cast<int>(std::round((fps * 100.0) / exposure));
    
    // Return as "1/X" notation
    return "1/" + std::to_string(shutter);
}

void DroneWebController::setDepthMode(DepthMode depth_mode) {
    if (recording_active_) {
        std::cerr << "[WEB_CONTROLLER] Cannot change depth mode while recording" << std::endl;
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "Cannot change depth mode while recording";
        return;
    }
    
    std::cout << "[WEB_CONTROLLER] Changing depth mode from " 
              << (int)depth_mode_ << " to " << (int)depth_mode << std::endl;
    
    depth_mode_ = depth_mode;
    
    // Need to reinitialize camera for both RAW and SVO2+Depth modes
    if (recording_mode_ == RecordingModeType::RAW_FRAMES || 
        recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
        
        camera_initializing_ = true;
        
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Reinitializing camera with new depth mode...";
        }
        
        updateLCD("Camera Init", "Please wait...");
        
        if (recording_mode_ == RecordingModeType::RAW_FRAMES) {
            std::cout << "[WEB_CONTROLLER] Reinitializing RAW recorder with new depth mode..." << std::endl;
            
            // Close and reinitialize raw recorder
            if (raw_recorder_) {
                raw_recorder_->close();
                raw_recorder_.reset();
            }
            
            // CRITICAL: Wait for camera hardware to fully release
            std::cout << "[WEB_CONTROLLER] Waiting 3s for camera hardware to release..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            raw_recorder_ = std::make_unique<RawFrameRecorder>();
            if (!raw_recorder_->init(RecordingMode::HD720_30FPS, depth_mode)) {
                std::cerr << "[WEB_CONTROLLER] Failed to reinitialize raw recorder" << std::endl;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "Camera initialization failed!";
                camera_initializing_ = false;
                updateLCD("Init Error", "Camera failed");
                return;
            }
        } else {
            // SVO2 + Depth modes
            std::cout << "[WEB_CONTROLLER] Reinitializing SVO2 recorder with new depth mode..." << std::endl;
            
            // Close and reinitialize svo recorder
            if (svo_recorder_) {
                svo_recorder_->close();
                svo_recorder_.reset();
            }
            
            // CRITICAL: Wait for camera hardware to fully release
            std::cout << "[WEB_CONTROLLER] Waiting 3s for camera hardware to release..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            svo_recorder_ = std::make_unique<ZEDRecorder>();
            
            // Enable depth computation BEFORE init() so camera opens with correct depth mode
            if (recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) {
                
                sl::DEPTH_MODE zed_depth_mode;
                switch (depth_mode) {
                    case DepthMode::NEURAL_PLUS: zed_depth_mode = sl::DEPTH_MODE::NEURAL_PLUS; break;
                    case DepthMode::NEURAL: zed_depth_mode = sl::DEPTH_MODE::NEURAL; break;
                    case DepthMode::NEURAL_LITE: zed_depth_mode = sl::DEPTH_MODE::NEURAL; break; // Use NEURAL for LITE
                    case DepthMode::ULTRA: zed_depth_mode = sl::DEPTH_MODE::ULTRA; break;
                    case DepthMode::QUALITY: zed_depth_mode = sl::DEPTH_MODE::QUALITY; break;
                    case DepthMode::PERFORMANCE: zed_depth_mode = sl::DEPTH_MODE::PERFORMANCE; break;
                    default: zed_depth_mode = sl::DEPTH_MODE::NONE; break;
                }
                
                std::cout << "[WEB_CONTROLLER] Enabling depth computation with mode: " << (int)zed_depth_mode << std::endl;
                svo_recorder_->enableDepthComputation(true, zed_depth_mode);
            }
            
            if (!svo_recorder_->init(RecordingMode::HD720_30FPS)) {
                std::cerr << "[WEB_CONTROLLER] Failed to reinitialize SVO2 recorder" << std::endl;
                std::lock_guard<std::mutex> lock(status_mutex_);
                status_message_ = "Camera initialization failed!";
                camera_initializing_ = false;
                updateLCD("Init Error", "Camera failed");
                return;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Camera ready";
        }
        
        camera_initializing_ = false;
        updateLCD("Camera Ready", getDepthModeShortName(depth_mode));
        std::cout << "[WEB_CONTROLLER] Camera reinitialized successfully" << std::endl;
    }
    
    std::cout << "[WEB_CONTROLLER] Depth mode set to: ";
    switch (depth_mode) {
        case DepthMode::NEURAL_PLUS: std::cout << "NEURAL_PLUS"; break;
        case DepthMode::NEURAL: std::cout << "NEURAL"; break;
        case DepthMode::NEURAL_LITE: std::cout << "NEURAL_LITE"; break;
        case DepthMode::ULTRA: std::cout << "ULTRA"; break;
        case DepthMode::QUALITY: std::cout << "QUALITY"; break;
        case DepthMode::PERFORMANCE: std::cout << "PERFORMANCE"; break;
        case DepthMode::NONE: std::cout << "NONE"; break;
    }
    std::cout << std::endl;
}

std::string DroneWebController::getDepthModeShortName(DepthMode mode) const {
    switch (mode) {
        case DepthMode::NEURAL_PLUS: return "N+";
        case DepthMode::NEURAL: return "Neural";
        case DepthMode::NEURAL_LITE: return "N-Lite";
        case DepthMode::ULTRA: return "Ultra";
        case DepthMode::QUALITY: return "Quality";
        case DepthMode::PERFORMANCE: return "Perf";
        case DepthMode::NONE: return "No Depth";
        default: return "Unknown";
    }
}

std::string DroneWebController::getDepthModeName(DepthMode mode) const {
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

sl::DEPTH_MODE DroneWebController::convertDepthMode(DepthMode mode) const {
    switch (mode) {
        case DepthMode::NEURAL_PLUS: return sl::DEPTH_MODE::NEURAL_PLUS;
        case DepthMode::NEURAL: return sl::DEPTH_MODE::NEURAL;
        case DepthMode::NEURAL_LITE: return sl::DEPTH_MODE::NEURAL;  // SDK uses NEURAL for lite
        case DepthMode::ULTRA: return sl::DEPTH_MODE::ULTRA;
        case DepthMode::QUALITY: return sl::DEPTH_MODE::QUALITY;
        case DepthMode::PERFORMANCE: return sl::DEPTH_MODE::PERFORMANCE;
        case DepthMode::NONE: return sl::DEPTH_MODE::NONE;
        default: return sl::DEPTH_MODE::NEURAL;
    }
}

bool DroneWebController::startHotspot() {
    std::cout << "[WEB_CONTROLLER] Starting WiFi hotspot..." << std::endl;
    updateLCD("Starting WiFi", "Hotspot...");
    
    if (!setupWiFiHotspot()) {
        std::cout << "[WEB_CONTROLLER] WiFi hotspot setup failed" << std::endl;
        updateLCD("WiFi Error", "Setup Failed");
        return false;
    }
    
    hotspot_active_ = true;
    updateLCD("WiFi Hotspot", "Active");
    std::cout << "[WEB_CONTROLLER] WiFi hotspot started" << std::endl;
    
    return true;
}

bool DroneWebController::stopHotspot() {
    std::cout << "[WEB_CONTROLLER] Stopping WiFi hotspot..." << std::endl;
    
    if (!teardownWiFiHotspot()) {
        std::cout << "[WEB_CONTROLLER] WiFi hotspot teardown failed" << std::endl;
        return false;
    }
    
    hotspot_active_ = false;
    updateLCD("WiFi Hotspot", "Stopped");
    std::cout << "[WEB_CONTROLLER] WiFi hotspot stopped" << std::endl;
    
    return true;
}

void DroneWebController::startWebServer(int port) {
    if (web_server_running_) {
        std::cout << "[WEB_CONTROLLER] Web server already running" << std::endl;
        return;
    }
    
    std::cout << "[WEB_CONTROLLER] Starting web server on port " << port << std::endl;
    updateLCD("Starting Web", "Server...");
    
    web_server_running_ = true;
    web_server_thread_ = std::make_unique<std::thread>(&DroneWebController::webServerLoop, this, port);
    
    updateLCD("Web Server", "http://192.168.4.1");
    std::cout << "[WEB_CONTROLLER] Web server started at http://192.168.4.1:" << port << std::endl;
}

void DroneWebController::stopWebServer() {
    if (!web_server_running_) {
        return;
    }
    
    std::cout << "[WEB_CONTROLLER] Stopping web server..." << std::endl;
    web_server_running_ = false;
    
    if (web_server_thread_ && web_server_thread_->joinable()) {
        web_server_thread_->join();
    }
    
    updateLCD("Web Server", "Stopped");
    std::cout << "[WEB_CONTROLLER] Web server stopped" << std::endl;
}

void DroneWebController::updateLCD(const std::string& line1, const std::string& line2) {
    if (lcd_) {
        lcd_->displayMessage(line1, line2);
    }
}

bool DroneWebController::monitorWiFiStatus() {
    // Check if DroneController connection is active
    int result = system("nmcli -t -f NAME,STATE con show --active | grep -q '^DroneController:activated$'");
    if (result != 0) {
        return false;
    }
    
    // Verify WiFi interface is in AP mode with correct IP
    result = system("ip addr show wlP1p1s0 | grep -q '10.42.0.1'");
    if (result != 0) {
        return false;
    }
    
    // Check if interface is in AP mode
    result = system("iw dev wlP1p1s0 info | grep -q 'type AP'");
    return result == 0;
}

void DroneWebController::restartWiFiIfNeeded() {
    if (!monitorWiFiStatus()) {
        std::cout << "[WEB_CONTROLLER] WiFi issue detected, restarting..." << std::endl;
        updateLCD("WiFi Recovery", "In Progress...");
        
        // Stop and restart hotspot
        teardownWiFiHotspot();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        if (setupWiFiHotspot()) {
            updateLCD("WiFi Recovered", "Restarting Web");
            
            // Restart web server after WiFi recovery
            if (web_server_running_) {
                stopWebServer();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                startWebServer(8080);
            }
        } else {
            updateLCD("WiFi Recovery", "Failed");
        }
    }
}

void DroneWebController::recordingMonitorLoop() {
    last_lcd_update_ = std::chrono::steady_clock::now();
    
    while (recording_active_ && !shutdown_requested_) {
        // If stopping, exit loop cleanly (let "Stopping..." message stay visible)
        if (current_state_ == RecorderState::STOPPING) {
            break;  // Exit loop immediately when stopping
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        
        // Check if recording duration reached
        if (elapsed >= recording_duration_seconds_) {
            std::cout << std::endl << "[WEB_CONTROLLER] Recording duration reached, setting timer_expired flag..." << std::endl;
            
            // Set flag to trigger stopRecording() from main thread (prevents deadlock)
            timer_expired_ = true;
            
            // Exit loop - main thread will call stopRecording() when it sees timer_expired_
            break;
        }
        
        // Update LCD every 3 seconds
        // Line 1: Static - shows progress "Rec: n/240s"
        // Line 2: Rotates every update between mode and settings
        auto lcd_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_lcd_update_).count();
        if (lcd_elapsed >= 3) {
            std::ostringstream line1, line2;
            
            // Line 1: Static progress display (max 16 chars)
            // Format: "Rec: 45/240s" (fits in 16 chars even at 999/999s)
            line1 << "Rec: " << elapsed << "/" << recording_duration_seconds_ << "s";
            
            // Line 2: Alternate between mode and settings every 3 seconds
            if (lcd_display_cycle_ == 0) {
                // Page 1: Recording mode (max 16 chars)
                switch (recording_mode_) {
                    case RecordingModeType::SVO2: 
                        line2 << "SVO2"; 
                        break;
                    case RecordingModeType::SVO2_DEPTH_INFO: 
                        line2 << "SVO2+RawDepth"; 
                        break;
                    case RecordingModeType::SVO2_DEPTH_IMAGES: 
                        line2 << "SVO2+DepthPNG"; 
                        break;
                    case RecordingModeType::RAW_FRAMES: 
                        line2 << "RAW"; 
                        break;
                }
            } else {
                // Page 2: Resolution@FPS shutter (max 16 chars)
                // Examples: "720@60 1/120" or "720@60 Auto"
                int fps;
                std::string res;
                switch (camera_resolution_) {
                    case RecordingMode::HD2K_15FPS: res = "2K"; fps = 15; break;
                    case RecordingMode::HD1080_30FPS: res = "1080"; fps = 30; break;
                    case RecordingMode::HD720_60FPS: res = "720"; fps = 60; break;
                    case RecordingMode::HD720_30FPS: res = "720"; fps = 30; break;
                    case RecordingMode::HD720_15FPS: res = "720"; fps = 15; break;
                    case RecordingMode::VGA_100FPS: res = "VGA"; fps = 100; break;
                }
                
                int exposure = getCameraExposure();
                std::string shutter = exposureToShutterSpeed(exposure, fps);
                line2 << res << "@" << fps << " " << shutter;
            }
            
            updateLCD(line1.str(), line2.str());
            
            // Toggle between page 0 and 1 every 3 seconds
            lcd_display_cycle_ = (lcd_display_cycle_ + 1) % 2;
            last_lcd_update_ = now;
        }
        
        updateRecordingStatus();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void DroneWebController::webServerLoop(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cout << "[WEB_CONTROLLER] Socket creation failed" << std::endl;
        return;
    }
    
    // Store for clean shutdown
    server_fd_ = server_fd;
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cout << "[WEB_CONTROLLER] Bind failed" << std::endl;
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 3) < 0) {
        std::cout << "[WEB_CONTROLLER] Listen failed" << std::endl;
        close(server_fd);
        return;
    }
    
    std::cout << "[WEB_CONTROLLER] Web server listening on port " << port << std::endl;
    
    while (web_server_running_) {
        // Check if recording timer expired (must be checked in main thread to avoid deadlock)
        if (timer_expired_ && recording_active_) {
            std::cout << "[WEB_CONTROLLER] Timer expired detected, calling robust stopRecording()..." << std::endl;
            timer_expired_ = false;  // Reset flag
            stopRecording();  // Use robust stop routine with all cleanup
        }
        
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity <= 0) continue;
        
        int client_socket = accept(server_fd, NULL, NULL);
        if (client_socket < 0) continue;
        
        handleClientRequest(client_socket);
        close(client_socket);
    }
    
    close(server_fd);
    std::cout << "[WEB_CONTROLLER] Web server loop ended" << std::endl;
}

void DroneWebController::systemMonitorLoop() {
    std::cout << "[WEB_CONTROLLER] System monitor thread started" << std::endl;
    int wifi_failure_count = 0;
    
    while (!shutdown_requested_) {
        // Monitor WiFi connection if hotspot is active (but be less aggressive)
        if (hotspot_active_) {
            if (!monitorWiFiStatus()) {
                wifi_failure_count++;
                if (wifi_failure_count >= 3) {  // Only restart after 3 consecutive failures
                    std::cout << "[WEB_CONTROLLER] WiFi has been down for 15 seconds, attempting restart..." << std::endl;
                    restartWiFiIfNeeded();
                    wifi_failure_count = 0;  // Reset counter after restart attempt
                }
            } else {
                wifi_failure_count = 0;  // Reset on success
            }
        }
        
        // Update LCD display with detailed status
        // Note: During recording, the recordingMonitorLoop handles LCD updates
        // Only update LCD here when NOT recording
        if (recording_active_) {
            // Do nothing - recordingMonitorLoop handles LCD during recording
        } else {
            // Check if we just stopped recording (keep "Recording Stopped" visible for 3 seconds)
            auto now = std::chrono::steady_clock::now();
            auto time_since_stop = std::chrono::duration_cast<std::chrono::seconds>(
                now - recording_stopped_time_).count();
            
            // Transition to IDLE immediately when recording stops
            if (current_state_ == RecorderState::STOPPING) {
                current_state_ = RecorderState::IDLE;
                std::cout << "[WEB_CONTROLLER] State transitioned to IDLE" << std::endl;
            }
            
            if (time_since_stop < 3 && time_since_stop >= 0) {
                // Keep "Recording Stopped" message visible for 3 seconds
                // (but state is already IDLE)
            } else if (hotspot_active_ && web_server_running_) {
                updateLCD("Web Controller", "10.42.0.1:8080");
            } else if (hotspot_active_) {
                updateLCD("WiFi Hotspot", "Starting...");
            } else {
                updateLCD("Drone Control", "Initializing...");
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "[WEB_CONTROLLER] System monitor thread stopped" << std::endl;
}

bool DroneWebController::setupWiFiHotspot() {
    std::cout << "\n[WEB_CONTROLLER] ========================================" << std::endl;
    std::cout << "[WEB_CONTROLLER] Starting SAFE WiFi Hotspot" << std::endl;
    std::cout << "[WEB_CONTROLLER] (Complies with NETWORK_SAFETY_POLICY.md)" << std::endl;
    std::cout << "[WEB_CONTROLLER] ========================================\n" << std::endl;
    
    // Create SafeHotspotManager instance (automatically backs up WiFi state)
    if (!hotspot_manager_) {
        hotspot_manager_ = std::make_unique<SafeHotspotManager>();
    }
    
    // Create hotspot with safety checks
    bool success = hotspot_manager_->createHotspot("DroneController", "drone123", "10.42.0.1");
    
    if (success) {
        std::cout << "[WEB_CONTROLLER] ✓ Hotspot created successfully!" << std::endl;
        displayWiFiStatus();
        hotspot_active_ = true;
        return true;
    } else {
        std::cerr << "[WEB_CONTROLLER] ✗ Failed to create hotspot" << std::endl;
        std::cerr << "[WEB_CONTROLLER] Check /var/log/drone_controller_network.log for details" << std::endl;
        hotspot_active_ = false;
        return false;
    }
}

bool DroneWebController::verifyHotspotActive() {
    if (!hotspot_manager_) {
        return false;
    }
    return hotspot_manager_->isHotspotActive();
}

void DroneWebController::displayWiFiStatus() {
    std::cout << "\n[WEB_CONTROLLER] ========================================" << std::endl;
    std::cout << "[WEB_CONTROLLER] WiFi Hotspot Active!" << std::endl;
    std::cout << "[WEB_CONTROLLER] ========================================" << std::endl;
    std::cout << "[WEB_CONTROLLER] SSID: DroneController" << std::endl;
    std::cout << "[WEB_CONTROLLER] Password: drone123" << std::endl;
    std::cout << "[WEB_CONTROLLER] IP Address: 10.42.0.1 (NetworkManager default)" << std::endl;
    std::cout << "[WEB_CONTROLLER] Web Interface: http://10.42.0.1:8080" << std::endl;
    std::cout << "[WEB_CONTROLLER] ========================================\n" << std::endl;
}

bool DroneWebController::teardownWiFiHotspot() {
    std::cout << "[WEB_CONTROLLER] Tearing down WiFi hotspot (SAFE mode)..." << std::endl;
    
    if (!hotspot_manager_) {
        std::cout << "[WEB_CONTROLLER] No hotspot manager active" << std::endl;
        return true;
    }
    
    bool success = hotspot_manager_->teardownHotspot();
    
    if (success) {
        std::cout << "[WEB_CONTROLLER] ✓ Hotspot deactivated, WiFi state restored" << std::endl;
        hotspot_active_ = false;
    } else {
        std::cerr << "[WEB_CONTROLLER] ✗ Hotspot teardown failed" << std::endl;
        std::cerr << "[WEB_CONTROLLER] Check /var/log/drone_controller_network.log for details" << std::endl;
    }
    
    return success;
    
    // Keep DroneController profile for next time (don't delete)
    // Optional: Delete connection for clean slate next time
    // Uncomment if you want to recreate the connection every time:
    // system("nmcli con delete DroneController 2>/dev/null");
    // Uncomment if you want to recreate the connection every time:
    // system("nmcli con delete DroneController 2>/dev/null");
    
    return true;
}

void DroneWebController::updateRecordingStatus() {
    // This method can be used to update internal recording status
    // Currently just a placeholder
}

void DroneWebController::handleClientRequest(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    
    std::string request(buffer);
    std::string response;
    
    // Parse HTTP request
    if (request.find("GET / ") != std::string::npos) {
        response = generateMainPage();
    } else if (request.find("GET /api/snapshot") != std::string::npos) {
        response = generateSnapshotJPEG();
    } else if (request.find("GET /api/status") != std::string::npos) {
        response = generateStatusAPI();
    } else if (request.find("POST /api/start_recording") != std::string::npos) {
        bool success = startRecording();
        response = generateAPIResponse(success ? "Recording started" : "Failed to start recording");
    } else if (request.find("POST /api/stop_recording") != std::string::npos) {
        bool success = stopRecording();
        response = generateAPIResponse(success ? "Recording stopped" : "Failed to stop recording");
    } else if (request.find("POST /api/set_recording_mode") != std::string::npos) {
        // Parse mode from request body
        size_t mode_pos = request.find("mode=");
        if (mode_pos != std::string::npos) {
            std::string mode_str = request.substr(mode_pos + 5, 25);
            if (mode_str.find("svo2_depth_info") != std::string::npos) {
                setRecordingMode(RecordingModeType::SVO2_DEPTH_INFO);
                response = generateAPIResponse("Recording mode set to SVO2 + Depth Info (32-bit raw)");
            } else if (mode_str.find("svo2_depth_images") != std::string::npos) {
                setRecordingMode(RecordingModeType::SVO2_DEPTH_IMAGES);
                response = generateAPIResponse("Recording mode set to SVO2 + Depth Images (PNG)");
            } else if (mode_str.find("svo2") != std::string::npos) {
                setRecordingMode(RecordingModeType::SVO2);
                response = generateAPIResponse("Recording mode set to SVO2");
            } else if (mode_str.find("raw") != std::string::npos) {
                setRecordingMode(RecordingModeType::RAW_FRAMES);
                response = generateAPIResponse("Recording mode set to RAW_FRAMES");
            } else {
                response = generateAPIResponse("Invalid recording mode");
            }
        } else {
            response = generateAPIResponse("Missing mode parameter");
        }
    } else if (request.find("POST /api/set_depth_mode") != std::string::npos) {
        // Parse depth mode from request body
        size_t mode_pos = request.find("depth=");
        if (mode_pos != std::string::npos) {
            std::string depth_str = request.substr(mode_pos + 6, 20);
            DepthMode depth_mode;
            
            if (depth_str.find("neural_plus") != std::string::npos) {
                depth_mode = DepthMode::NEURAL_PLUS;
            } else if (depth_str.find("neural_lite") != std::string::npos) {
                depth_mode = DepthMode::NEURAL_LITE;
            } else if (depth_str.find("neural") != std::string::npos) {
                depth_mode = DepthMode::NEURAL;
            } else if (depth_str.find("ultra") != std::string::npos) {
                depth_mode = DepthMode::ULTRA;
            } else if (depth_str.find("quality") != std::string::npos) {
                depth_mode = DepthMode::QUALITY;
            } else if (depth_str.find("performance") != std::string::npos) {
                depth_mode = DepthMode::PERFORMANCE;
            } else if (depth_str.find("none") != std::string::npos) {
                depth_mode = DepthMode::NONE;
            } else {
                response = generateAPIResponse("Invalid depth mode");
                send(client_socket, response.c_str(), response.length(), 0);
                return;
            }
            
            setDepthMode(depth_mode);
            response = generateAPIResponse("Depth mode updated");
        } else {
            response = generateAPIResponse("Missing depth parameter");
        }
    } else if (request.find("POST /api/set_depth_recording_fps") != std::string::npos) {
        // Parse FPS from request body
        size_t fps_pos = request.find("fps=");
        if (fps_pos != std::string::npos) {
            std::string fps_str = request.substr(fps_pos + 4, 10);
            try {
                int fps = std::stoi(fps_str);
                if (fps >= 0 && fps <= 100) {  // Allow 0-100 range (scales with camera FPS)
                    depth_recording_fps_ = fps;
                    std::cout << "[WEB_CONTROLLER] Depth recording FPS set to: " << fps << std::endl;
                    response = generateAPIResponse("Depth recording FPS updated to " + std::to_string(fps));
                } else {
                    response = generateAPIResponse("FPS must be between 0 and 100");
                }
            } catch (...) {
                response = generateAPIResponse("Invalid FPS value");
            }
        } else {
            response = generateAPIResponse("Missing fps parameter");
        }
    } else if (request.find("POST /api/set_camera_resolution") != std::string::npos) {
        // Parse resolution/FPS mode from request body
        size_t mode_pos = request.find("mode=");
        if (mode_pos != std::string::npos) {
            std::string mode_str = request.substr(mode_pos + 5, 20);
            RecordingMode mode;
            
            if (mode_str.find("hd2k_15") != std::string::npos) {
                mode = RecordingMode::HD2K_15FPS;
            } else if (mode_str.find("hd1080_30") != std::string::npos) {
                mode = RecordingMode::HD1080_30FPS;
            } else if (mode_str.find("hd720_60") != std::string::npos) {
                mode = RecordingMode::HD720_60FPS;
            } else if (mode_str.find("hd720_30") != std::string::npos) {
                mode = RecordingMode::HD720_30FPS;
            } else if (mode_str.find("hd720_15") != std::string::npos) {
                mode = RecordingMode::HD720_15FPS;
            } else if (mode_str.find("vga_100") != std::string::npos) {
                mode = RecordingMode::VGA_100FPS;
            } else {
                response = generateAPIResponse("Invalid resolution/FPS mode");
                send(client_socket, response.c_str(), response.length(), 0);
                return;
            }
            
            setCameraResolution(mode);
            response = generateAPIResponse("Camera resolution updated");
        } else {
            response = generateAPIResponse("Missing mode parameter");
        }
    } else if (request.find("POST /api/set_camera_exposure") != std::string::npos) {
        // Parse exposure from request body
        size_t exp_pos = request.find("exposure=");
        if (exp_pos != std::string::npos) {
            std::string exp_str = request.substr(exp_pos + 9, 10);
            try {
                int exposure = std::stoi(exp_str);
                if (exposure >= -1 && exposure <= 100) {  // -1 = auto, 0-100 = manual
                    setCameraExposure(exposure);
                    response = generateAPIResponse("Exposure updated to " + std::to_string(exposure));
                } else {
                    response = generateAPIResponse("Exposure must be -1 (auto) or 0-100 (manual)");
                }
            } catch (...) {
                response = generateAPIResponse("Invalid exposure value");
            }
        } else {
            response = generateAPIResponse("Missing exposure parameter");
        }
        
    } else if (request.find("POST /api/set_camera_gain") != std::string::npos) {
        // Parse gain from request body
        size_t gain_pos = request.find("gain=");
        if (gain_pos != std::string::npos) {
            std::string gain_str = request.substr(gain_pos + 5, 10);
            try {
                int gain = std::stoi(gain_str);
                if (gain >= -1 && gain <= 100) {  // -1 = auto, 0-100 = manual
                    setCameraGain(gain);
                    response = generateAPIResponse("Gain updated to " + std::to_string(gain));
                } else {
                    response = generateAPIResponse("Gain must be -1 (auto) or 0-100 (manual)");
                }
            } catch (...) {
                response = generateAPIResponse("Invalid gain value");
            }
        } else {
            response = generateAPIResponse("Missing exposure parameter");
        }
    } else if (request.find("POST /api/shutdown") != std::string::npos) {
        response = generateAPIResponse("Shutdown initiated");
        send(client_socket, response.c_str(), response.length(), 0);
        shutdownSystem();
        return;
    } else {
        response = "HTTP/1.1 404 Not Found\r\n\r\n<h1>404 Not Found</h1>";
    }
    
    send(client_socket, response.c_str(), response.length(), 0);
}

std::string DroneWebController::generateMainPage() {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/html; charset=utf-8\r\n"
           "Cache-Control: no-cache, no-store, must-revalidate\r\n"
           "Pragma: no-cache\r\n"
           "Expires: 0\r\n\r\n"
           "<!DOCTYPE html><html><head><title>Drone Controller</title>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<meta charset='utf-8'>"
           "<style>body{font-family:Arial;text-align:center;margin:15px;background:#f0f0f0}"
           ".container{max-width:500px;margin:0 auto;background:white;padding:25px;border-radius:12px;box-shadow:0 4px 6px rgba(0,0,0,0.1)}"
           ".status{padding:15px;margin:15px 0;border-radius:8px;font-weight:bold;font-size:18px}"
           ".status.idle{background:#d4edda;color:#155724;border:2px solid #c3e6cb}"
           ".status.recording{background:#fff3cd;color:#856404;border:2px solid #ffeaa7}"
           ".status.stopping{background:#ffeaa7;color:#b5651d;border:2px solid #ffdf7e;animation:pulse 1s infinite}"
           ".status.error{background:#f8d7da;color:#721c24;border:2px solid #f5c6cb}"
           ".status.initializing{background:#cce5ff;color:#004085;border:2px solid #b8daff;animation:pulse 1.5s infinite}"
           "@keyframes pulse{0%{opacity:1}50%{opacity:0.7}100%{opacity:1}}"
           ".notification{padding:12px;margin:10px 0;border-radius:8px;font-size:14px;display:none}"
           ".notification.show{display:block}"
           ".notification.info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb}"
           ".notification.warning{background:#fff3cd;color:#856404;border:1px solid #ffeaa7}"
           ".config-section{background:#f8f9fa;padding:15px;margin:15px 0;border-radius:8px;text-align:left}"
           ".config-section h3{margin-top:0;color:#495057;font-size:16px;text-align:center}"
           ".radio-group{margin:10px 0}"
           ".radio-group label{display:inline-block;margin-right:20px;cursor:pointer}"
           ".radio-group input[type='radio']{margin-right:5px}"
           ".select-group{margin:10px 0}"
           ".select-group label{display:block;margin-bottom:5px;font-weight:bold;color:#495057}"
           ".select-group select{width:100%;padding:8px;border-radius:6px;border:1px solid #ced4da;font-size:14px}"
           ".mode-info{font-size:12px;color:#6c757d;margin-top:5px;font-style:italic}"
           ".progress{margin:15px 0;padding:10px;background:#f8f9fa;border-radius:8px}"
           ".progress-bar{width:100%;height:25px;background:#e9ecef;border-radius:12px;overflow:hidden;margin:8px 0}"
           ".progress-fill{height:100%;background:#28a745;transition:width 0.3s ease;border-radius:12px}"
           ".info-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:15px 0;font-size:14px}"
           ".info-item{background:#f8f9fa;padding:8px;border-radius:6px;border:1px solid #dee2e6}"
           "button{padding:15px 30px;margin:8px;border:none;border-radius:8px;font-size:16px;cursor:pointer;font-weight:bold}"
           ".start{background:#28a745;color:white;box-shadow:0 2px 4px rgba(40,167,69,0.3)}"
           ".stop{background:#dc3545;color:white;box-shadow:0 2px 4px rgba(220,53,69,0.3)}"
           ".shutdown{background:#6c757d;color:white;box-shadow:0 2px 4px rgba(108,117,125,0.3)}"
           "button:hover{transform:translateY(-1px);box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
           "button:disabled{opacity:0.6;cursor:not-allowed;transform:none}"
           "h1{color:#495057;margin-bottom:25px}"
           ".tabs{display:flex;background:#e9ecef;border-radius:8px 8px 0 0;overflow:hidden;margin:0 -25px;margin-top:-10px;padding:0}"
           ".tab{flex:1;padding:12px 8px;border:none;background:#e9ecef;color:#495057;font-size:14px;font-weight:bold;cursor:pointer;transition:all 0.3s;border-bottom:3px solid transparent}"
           ".tab:hover{background:#dee2e6}"
           ".tab.active{background:#fff;color:#007bff;border-bottom:3px solid #007bff}"
           ".tab-content{display:none;padding-top:15px}"
           ".tab-content.active{display:block}"
           ".slider-container{margin:15px 0}"
           ".slider-container input[type='range']{width:100%}"
           ".gain-guide{display:flex;justify-content:space-between;font-size:11px;margin-top:5px}"
           ".gain-range{text-align:center;flex:1}"
           ".gain-range.bright{color:#28a745}"
           ".gain-range.normal{color:#ffc107}"
           ".gain-range.low{color:#ff8800}"
           ".gain-range.dark{color:#dc3545}"
           ".livestream-container{text-align:center;margin:20px 0}"
           ".livestream-img{max-width:100%;height:auto;border-radius:8px;border:2px solid #dee2e6;background:#000}"
           ".fullscreen-btn{background:#007bff;color:white;padding:10px 20px;font-size:14px}"
           ".fullscreen-overlay{display:none;position:fixed;top:0;left:0;width:100vw;height:100vh;background:rgba(0,0,0,0.95);z-index:9999;justify-content:center;align-items:center;flex-direction:column}"
           ".fullscreen-overlay.active{display:flex;cursor:default}"
           ".fullscreen-img{max-width:90vw;max-height:80vh;border-radius:8px;pointer-events:none}"
           ".close-fullscreen{position:absolute;top:20px;right:20px;background:#dc3545;color:white;border:none;padding:15px 25px;border-radius:8px;font-size:18px;cursor:pointer;font-weight:bold;z-index:10001;pointer-events:auto}"
           ".close-fullscreen:hover{background:#c82333;transform:scale(1.1)}"
           ".close-fullscreen:active{transform:scale(0.95)}"
           ".system-info{background:#f8f9fa;padding:12px;border-radius:8px;margin:10px 0;text-align:left;font-size:14px}"
           ".system-info strong{color:#495057}"
           "</style>"
           "<script>"
           "let currentRecMode='svo2',currentDepthMode='NEURAL_LITE',livestreamActive=false,livestreamInterval=null,fullscreenInterval=null,livestreamFPS=2,currentCameraFPS=60,lastFrameTime=Date.now(),frameCount=0,actualLivestreamFPS=0;"
           "const exposureToShutterSpeed=(exposure,fps)=>{"
           "if(exposure<=0)return 'Auto';"
           "let shutter=Math.round((fps*100)/exposure);"
           "return '1/'+shutter;"
           "};"
           "const getShutterSpeedsForFPS=(fps)=>{"
           "const cleanSpeeds=[60,90,120,150,180,240,360,480,720,960,1200];"
           "return cleanSpeeds.map(s=>{"
           "let exposure=Math.round((fps*100)/s);"
           "if(exposure<1)exposure=1;"
           "if(exposure>100)exposure=100;"
           "return {s:s,e:exposure};"
           "}).filter(item=>item.e>=5&&item.e<=100);"
           "};"
           "let shutterSpeeds=[{s:0,e:-1}];"  // Auto mode, will be populated on init
           "function switchTab(tabName){"
           "document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"
           "document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"
           "document.querySelector('.tab[data-tab=\"'+tabName+'\"]').classList.add('active');"
           "document.getElementById(tabName+'-tab').classList.add('active');"
           "if(tabName==='livestream'&&livestreamActive){"
           "startLivestream();"
           "}else if(tabName!=='livestream'){"
           "stopLivestream();"
           "}"
           "}"
           "function updateStatus(){"
           "fetch('/api/status').then(r=>r.json()).then(data=>{"
           "let stateText=data.state===0?'IDLE':data.state===1?'RECORDING':'STOPPING';"
           "document.getElementById('status').textContent=stateText;"
           "let isRecording=data.state===1;"
           "let isInitializing=data.camera_initializing;"
           "currentRecMode=data.recording_mode;"
           "currentDepthMode=data.depth_mode;"
           "if(data.camera_fps!==undefined&&data.camera_fps!==currentCameraFPS){"
           "currentCameraFPS=data.camera_fps;"
           "shutterSpeeds=[{s:0,e:-1}].concat(getShutterSpeedsForFPS(currentCameraFPS));"
           "console.log('Camera FPS changed to '+currentCameraFPS+', regenerated '+shutterSpeeds.length+' shutter speed options');"
           "let maxIndex=shutterSpeeds.length-1;"
           "if(document.getElementById('exposureSlider').max!=maxIndex){"
           "document.getElementById('exposureSlider').max=maxIndex;"
           "}"
           "}"
           "if(data.camera_exposure!==undefined){"
           "let exposure=data.camera_exposure;"
           "let shutterIndex=0;"
           "if(exposure===-1){shutterIndex=0;}"
           "else{"
           "let minDiff=999;"
           "for(let i=1;i<shutterSpeeds.length;i++){"
           "let diff=Math.abs(shutterSpeeds[i].e-exposure);"
           "if(diff<minDiff){minDiff=diff;shutterIndex=i;}"
           "}"
           "}"
           "document.getElementById('exposureSlider').value=shutterIndex;"
           "let displayLabel=(shutterIndex===0)?'Auto':'1/'+shutterSpeeds[shutterIndex].s;"
           "document.getElementById('exposureValue').textContent=displayLabel;"
           "document.getElementById('exposureActual').textContent='(E:'+exposure+')';"
           "}"
           "if(data.camera_gain!==undefined){"
           "let gain=data.camera_gain;"
           "document.getElementById('gainSlider').value=gain;"
           "document.getElementById('gainValue').textContent=gain===-1?'Auto':gain;"
           "updateGainGuide(gain);"
           "}"
           "document.getElementById('modeRadioSVO2').checked=(currentRecMode==='svo2');"
           "document.getElementById('modeRadioDepthInfo').checked=(currentRecMode==='svo2_depth_info');"
           "document.getElementById('modeRadioDepthImages').checked=(currentRecMode==='svo2_depth_images');"
           "document.getElementById('modeRadioRaw').checked=(currentRecMode==='raw');"
           "if(!isInitializing){"
           "document.getElementById('depthModeSelect').value=currentDepthMode.toLowerCase();"
           "}"
           "let showDepth=(currentRecMode!=='svo2');"
           "let showDepthFps=(currentRecMode==='svo2_depth_info'||currentRecMode==='svo2_depth_images');"
           "document.getElementById('depthModeSelect').disabled=isRecording||isInitializing;"
           "document.getElementById('modeRadioSVO2').disabled=isRecording||isInitializing;"
           "document.getElementById('modeRadioDepthInfo').disabled=isRecording||isInitializing;"
           "document.getElementById('modeRadioDepthImages').disabled=isRecording||isInitializing;"
           "document.getElementById('modeRadioRaw').disabled=isRecording||isInitializing;"
           "document.getElementById('depthFpsSlider').disabled=isRecording||isInitializing;"
           "if(isInitializing){"
           "document.getElementById('statusDiv').className='status initializing';"
           "document.getElementById('status').textContent='INITIALIZING...';"
           "document.getElementById('notification').className='notification warning show';"
           "document.getElementById('notification').textContent=data.status_message||'Camera initializing, please wait...';"
           "document.getElementById('startBtn').disabled=true;"
           "if(livestreamActive){"
           "console.log('⚠️ Camera initializing - auto-stopping livestream for safety');"
           "document.getElementById('livestreamToggle').checked=false;"
           "stopLivestream();"
           "}"
           "}else if(data.status_message){"
           "document.getElementById('notification').className='notification info show';"
           "document.getElementById('notification').textContent=data.status_message;"
           "setTimeout(()=>{document.getElementById('notification').style.display='none';},5000);"
           "}else{"
           "document.getElementById('notification').style.display='none';"
           "}"
           "document.getElementById('statusDiv').className='status '+(isRecording?'recording':'idle');"
           "document.getElementById('startBtn').disabled=isRecording||isInitializing;"
           "document.getElementById('stopBtn').disabled=!isRecording;"
           "document.getElementById('livestreamToggle').disabled=isInitializing;"
           "document.getElementById('livestreamFPSSelect').disabled=isInitializing;"
           "document.getElementById('depthModeGroup').style.display=showDepth?'block':'none';"
           "document.getElementById('depthFpsGroup').style.display=showDepthFps?'block':'none';"
           "if(currentRecMode==='svo2_depth_info'){"
           "document.getElementById('modeInfo').textContent='SVO2 + Raw 32-bit depth data (fast, for post-processing)';"
           "}else if(currentRecMode==='svo2_depth_images'){"
           "document.getElementById('modeInfo').textContent='SVO2 + PNG depth visualization (slower, human-readable)';"
           "}else if(currentRecMode==='svo2'){"
           "document.getElementById('modeInfo').textContent='SVO2: Single compressed file at 30 FPS';"
           "}else if(currentRecMode==='raw'){"
           "document.getElementById('modeInfo').textContent='RAW: Separate left/right/depth images';"
           "}"
           "if(isRecording){"
           "let elapsed=data.recording_duration_total-data.recording_time_remaining;"
           "let percent=Math.round((elapsed/data.recording_duration_total)*100);"
           "let fileSize=(data.bytes_written/(1024*1024*1024)).toFixed(2);"
           "let speed=data.mb_per_second.toFixed(1);"
           "document.getElementById('progress').style.display='block';"
           "document.getElementById('progressBar').style.width=percent+'%';"
           "document.getElementById('elapsed').textContent=elapsed+'s';"
           "document.getElementById('remaining').textContent=data.recording_time_remaining+'s';"
           "document.getElementById('percent').textContent=percent+'%';"
           "document.getElementById('filesize').textContent=fileSize+' GB';"
           "document.getElementById('speed').textContent=speed+' MB/s';"
           "if(currentRecMode==='raw'){"
           "document.getElementById('filename').textContent='Frames: '+data.frame_count+' | FPS: '+data.current_fps.toFixed(1);"
           "}else if(currentRecMode==='svo2_depth_test'||currentRecMode==='svo2_depth_images'){"
           "document.getElementById('filename').textContent='Recording | Depth FPS: '+((data.depth_fps||0).toFixed(1));"
           "}else{"
           "document.getElementById('filename').textContent=data.current_file_path.split('/').pop();"
           "}"
           "}else{"
           "document.getElementById('progress').style.display='none';"
           "}"
           "}).catch(()=>{"
           "document.getElementById('statusDiv').className='status error';"
           "document.getElementById('status').textContent='CONNECTION ERROR';"
           "});"
           "}"
           "function setRecordingMode(mode){"
           "if(currentRecMode===mode)return;"
           "fetch('/api/set_recording_mode',{method:'POST',body:'mode='+mode}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "updateStatus();"
           "});"
           "}"
           "function setDepthMode(){"
           "let mode=document.getElementById('depthModeSelect').value;"
           "fetch('/api/set_depth_mode',{method:'POST',body:'depth='+mode}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "document.getElementById('notification').className='notification warning show';"
           "document.getElementById('notification').textContent='Reinitializing camera, please wait...';"
           "setTimeout(updateStatus,500);"
           "});"
           "}"
           "function setDepthRecordingFPS(fps){"
           "document.getElementById('depthFpsValue').textContent=fps;"
           "fetch('/api/set_depth_recording_fps',{method:'POST',body:'fps='+fps}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "});"
           "}"
           "function setCameraResolution(){"
           "let mode=document.getElementById('cameraResolutionSelectLive').value;"
           "fetch('/api/set_camera_resolution',{method:'POST',body:'mode='+mode}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "document.getElementById('notification').className='notification warning show';"
           "document.getElementById('notification').textContent='Reinitializing camera, please wait...';"
           "setTimeout(updateStatus,2000);"
           "});"
           "}"
           "function setCameraExposure(shutterIndex){"
           "let selected=shutterSpeeds[shutterIndex];"
           "let label=(selected.e===-1)?'Auto':'1/'+selected.s;"
           "document.getElementById('exposureValue').textContent=label;"
           "document.getElementById('exposureActual').textContent='(E:'+selected.e+')';"
           "fetch('/api/set_camera_exposure',{method:'POST',body:'exposure='+selected.e}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "});"
           "}"
           "function startRecording(){"
           "document.getElementById('status').textContent='STARTING RECORDING...';"
           "document.getElementById('statusDiv').className='status recording';"
           "fetch('/api/start_recording',{method:'POST'}).then(()=>updateStatus());"
           "}"
           "function stopRecording(){"
           "document.getElementById('status').textContent='STOPPING...';"
           "document.getElementById('statusDiv').className='status stopping';"
           "fetch('/api/stop_recording',{method:'POST'}).then(()=>updateStatus());"
           "}"
           "function shutdown(){if(confirm('System herunterfahren?')){fetch('/api/shutdown',{method:'POST'});}}"
           "function setCameraGain(gain){"
           "document.getElementById('gainValue').textContent=gain===-1||gain===-2?'Auto':gain;"
           "updateGainGuide(gain);"
           "fetch('/api/set_camera_gain',{method:'POST',body:'gain='+gain}).then(r=>r.json()).then(data=>{"
           "console.log(data.message);"
           "});"
           "}"
           "function updateGainGuide(gain){"
           "document.querySelectorAll('.gain-range').forEach(r=>r.style.fontWeight='normal');"
           "if(gain>=0&&gain<=20){document.querySelector('.gain-range.bright').style.fontWeight='bold';}"
           "else if(gain>=21&&gain<=50){document.querySelector('.gain-range.normal').style.fontWeight='bold';}"
           "else if(gain>=51&&gain<=80){document.querySelector('.gain-range.low').style.fontWeight='bold';}"
           "else if(gain>=81&&gain<=100){document.querySelector('.gain-range.dark').style.fontWeight='bold';}"
           "}"
           "function toggleLivestream(){"
           "livestreamActive=document.getElementById('livestreamToggle').checked;"
           "if(livestreamActive){startLivestream();}else{stopLivestream();}"
           "}"
           "function startLivestream(){"
           "if(livestreamInterval)return;"
           "document.getElementById('livestreamImage').style.display='block';"
           "frameCount=0;"
           "lastFrameTime=Date.now();"
           "actualLivestreamFPS=0;"
           "document.getElementById('actualFPS').textContent='-';"
           "let intervalMs=Math.round(1000/livestreamFPS);"
           "let img=document.getElementById('livestreamImage');"
           "img.onload=function(){"
           "frameCount++;"
           "let now=Date.now();"
           "if(now-lastFrameTime>=1000){"
           "actualLivestreamFPS=frameCount;"
           "document.getElementById('actualFPS').textContent=actualLivestreamFPS+' FPS';"
           "frameCount=0;"
           "lastFrameTime=now;"
           "}"
           "};"
           "livestreamInterval=setInterval(()=>{"
           "img.src='/api/snapshot?t='+Date.now();"
           "},intervalMs);"
           "console.log('Livestream started at '+livestreamFPS+' FPS ('+intervalMs+'ms interval)');"
           "}"
           "function stopLivestream(){"
           "if(livestreamInterval){clearInterval(livestreamInterval);livestreamInterval=null;}"
           "document.getElementById('livestreamImage').style.display='none';"
           "document.getElementById('actualFPS').textContent='-';"
           "console.log('Livestream stopped');"
           "}"
           "function enterFullscreen(){"
           "if(fullscreenInterval){clearInterval(fullscreenInterval);fullscreenInterval=null;}"
           "document.getElementById('fullscreenOverlay').classList.add('active');"
           "document.getElementById('fullscreenImage').src='/api/snapshot?t='+Date.now();"
           "let intervalMs=Math.round(1000/livestreamFPS);"
           "fullscreenInterval=setInterval(()=>{"
           "if(document.getElementById('fullscreenOverlay').classList.contains('active')){"
           "document.getElementById('fullscreenImage').src='/api/snapshot?t='+Date.now();"
           "}"
           "},intervalMs);"
           "console.log('Fullscreen started at '+livestreamFPS+' FPS');"
           "document.getElementById('fullscreenOverlay').onclick=function(e){"
           "if(e.target.id==='fullscreenOverlay'){exitFullscreen();}"
           "};"
           "}"
           "function exitFullscreen(){"
           "console.log('Closing fullscreen...');"
           "if(fullscreenInterval){clearInterval(fullscreenInterval);fullscreenInterval=null;}"
           "document.getElementById('fullscreenOverlay').classList.remove('active');"
           "document.getElementById('fullscreenOverlay').onclick=null;"
           "console.log('Fullscreen closed');"
           "}"
           "function setLivestreamFPS(fps){"
           "livestreamFPS=parseInt(fps);"
           "console.log('Livestream FPS changed to '+livestreamFPS);"
           "updateNetworkStats();"
           "if(livestreamActive){"
           "stopLivestream();"
           "startLivestream();"
           "}"
           "}"
           "function updateNetworkStats(){"
           "let fps=livestreamFPS;"
           "let bytesPerFrame=75000;"
           "let bytesPerSecond=fps*bytesPerFrame;"
           "let kbps=(bytesPerSecond/1024).toFixed(1);"
           "let mbps=(bytesPerSecond/1024/1024).toFixed(2);"
           "let display='';"
           "if(bytesPerSecond<1024*1024){"
           "display=kbps+' KB/s';"
           "}else{"
           "display=mbps+' MB/s';"
           "}"
           "display+=' @ '+fps+' FPS (estimated)';"
           "document.getElementById('networkUsage').textContent=display;"
           "if(document.getElementById('livestreamNetworkUsage')){"
           "document.getElementById('livestreamNetworkUsage').textContent=display;"
           "}"
           "}"
           "function setupFullscreenButton(){"
           "let btn=document.getElementById('closeFullscreenBtn');"
           "if(btn){"
           "btn.addEventListener('click',function(e){"
           "e.stopPropagation();"
           "e.preventDefault();"
           "console.log('Close button clicked (event listener)');"
           "exitFullscreen();"
           "});"
           "console.log('✅ Fullscreen close button event listener attached');"
           "}else{"
           "console.error('❌ Close fullscreen button not found! DOM may not be ready.');"
           "}"
           "}"
           "document.addEventListener('DOMContentLoaded',function(){"
           "console.log('DOM loaded, setting up UI...');"
           "shutterSpeeds=[{s:0,e:-1}].concat(getShutterSpeedsForFPS(currentCameraFPS));"
           "document.getElementById('exposureSlider').max=shutterSpeeds.length-1;"
           "document.getElementById('livestreamToggle').checked=false;"
           "document.getElementById('livestreamFPSSelect').value='2';"
           "livestreamActive=false;"
           "console.log('Livestream initialized: OFF, 2 FPS default');"
           "setupFullscreenButton();"
           "setInterval(updateStatus,1000);"
           "setInterval(updateNetworkStats,2000);"
           "updateStatus();"
           "updateNetworkStats();"
           "console.log('UI setup complete');"
           "});"
           "</script></head><body>"
           "<div class='container'>"
           "<h1>🚁 DRONE CONTROLLER</h1>"
           "<div id='notification' class='notification'></div>"
           "<div id='statusDiv' class='status idle'>Status: <span id='status'>Loading...</span></div>"
           "<div class='tabs'>"
           "<button class='tab active' data-tab='recording' onclick='switchTab(\"recording\")'>📹 Recording</button>"
           "<button class='tab' data-tab='livestream' onclick='switchTab(\"livestream\")'>📷 Livestream</button>"
           "<button class='tab' data-tab='system' onclick='switchTab(\"system\")'>⚙️ System</button>"
           "<button class='tab' data-tab='power' onclick='switchTab(\"power\")'>🔋 Power</button>"
           "</div>"
           "<div id='recording-tab' class='tab-content active'>"
           "<div class='config-section'>"
           "<h3>Recording Mode</h3>"
           "<div class='radio-group'>"
           "<label><input type='radio' id='modeRadioSVO2' name='recMode' value='svo2' checked onclick='setRecordingMode(\"svo2\")'> SVO2 (Standard)</label>"
           "<label><input type='radio' id='modeRadioDepthInfo' name='recMode' value='svo2_depth_info' onclick='setRecordingMode(\"svo2_depth_info\")'> SVO2 + Depth Info (Fast, 32-bit raw)</label>"
           "<label><input type='radio' id='modeRadioDepthImages' name='recMode' value='svo2_depth_images' onclick='setRecordingMode(\"svo2_depth_images\")'> SVO2 + Depth Images (PNG)</label>"
           "<label><input type='radio' id='modeRadioRaw' name='recMode' value='raw' onclick='setRecordingMode(\"raw\")'> RAW (Images+Depth)</label>"
           "</div>"
           "<div class='mode-info' id='modeInfo'>SVO2: Single compressed file at 30 FPS</div>"
           "<div class='select-group' id='depthModeGroup' style='display:none'>"
           "<label>Depth Computation Mode:</label>"
           "<select id='depthModeSelect' onchange='setDepthMode()'>"
           "<option value='neural_plus' selected>Neural Plus ⭐ (Best Quality)</option>"
           "<option value='neural'>Neural</option>"
           "<option value='neural_lite'>Neural Lite (Fast)</option>"
           "<option value='ultra'>Ultra</option>"
           "<option value='quality'>Quality</option>"
           "<option value='performance'>Performance</option>"
           "<option value='none'>None (Images Only)</option>"
           "</select>"
           "<div class='mode-info'>⚠️ Changing depth mode reinitializes the camera</div>"
           "</div>"
           "<div class='select-group' id='depthFpsGroup' style='display:none'>"
           "<label>Depth Recording FPS: <span id='depthFpsValue'>10</span> (0 = test mode)</label>"
           "<input type='range' id='depthFpsSlider' min='0' max='30' value='10' oninput='setDepthRecordingFPS(this.value)'>"
           "<div class='mode-info'>0 FPS = Compute but don't save (performance test)</div>"
           "</div>"
           "</div>"
           "<div id='progress' style='display:none'>"
           "<div class='progress-bar'><div id='progressBar' class='progress-fill' style='width:0%'></div></div>"
           "<div class='info-grid'>"
           "<div class='info-item'>Elapsed: <strong><span id='elapsed'>0</span></strong></div>"
           "<div class='info-item'>Remaining: <strong><span id='remaining'>0</span></strong></div>"
           "<div class='info-item'>Progress: <strong><span id='percent'>0%</span></strong></div>"
           "<div class='info-item'>File Size: <strong><span id='filesize'>0 GB</span></strong></div>"
           "<div class='info-item'>Speed: <strong><span id='speed'>0 MB/s</span></strong></div>"
           "<div class='info-item'>Info: <strong><span id='filename'>-</span></strong></div>"
           "</div></div>"
           "<button id='startBtn' class='start' onclick='startRecording()'>START RECORDING</button><br>"
           "<button id='stopBtn' class='stop' onclick='stopRecording()'>STOP RECORDING</button>"
           "</div>"
           "<div id='livestream-tab' class='tab-content'>"
           "<div class='config-section'>"
           "<h3>📷 Live Preview</h3>"
           "<label style='display:block;margin:15px 0'>"
           "<input type='checkbox' id='livestreamToggle' onchange='toggleLivestream()'> Enable Livestream"
           "</label>"
           "<div class='livestream-container'>"
           "<img id='livestreamImage' class='livestream-img' style='display:none' alt='Livestream'/>"
           "</div>"
           "<div class='select-group'>"
           "<label>Livestream FPS:</label>"
           "<select id='livestreamFPSSelect' onchange='setLivestreamFPS(this.value)'>"
           "<option value='2' selected>2 FPS</option>"
           "<option value='4'>4 FPS</option>"
           "<option value='6'>6 FPS</option>"
           "<option value='8'>8 FPS</option>"
           "<option value='10'>10 FPS</option>"
           "</select>"
           "</div>"
           "<div class='system-info' style='margin:15px 0'>"
           "<strong>📊 Network Usage:</strong> <span id='livestreamNetworkUsage'>Calculating...</span><br>"
           "<strong>📷 Actual FPS:</strong> <span id='actualFPS'>-</span>"
           "</div>"
           "<button class='fullscreen-btn' onclick='enterFullscreen()'>⛶ Fullscreen</button>"
           "</div>"
           "<div class='config-section'>"
           "<h3>⚙️ Camera Settings</h3>"
           "<div class='select-group'>"
           "<label>Resolution & FPS:</label>"
           "<select id='cameraResolutionSelectLive' onchange='setCameraResolution()'>"
           "<option value='hd2k_15'>HD2K (2208×1242) @ 15 FPS</option>"
           "<option value='hd1080_30'>HD1080 (1920×1080) @ 30 FPS</option>"
           "<option value='hd720_60' selected>HD720 (1280×720) @ 60 FPS ⭐</option>"
           "<option value='hd720_30'>HD720 (1280×720) @ 30 FPS</option>"
           "<option value='hd720_15'>HD720 (1280×720) @ 15 FPS</option>"
           "<option value='vga_100'>VGA (672×376) @ 100 FPS</option>"
           "</select>"
           "<div class='mode-info'>⚠️ Changing resolution/FPS reinitializes the camera</div>"
           "</div>"
           "<div class='slider-container'>"
           "<label>Shutter Speed: <span id='exposureValue'>1/120</span> <span id='exposureActual' style='font-size:11px;color:#888'>(E:50)</span></label>"
           "<input type='range' id='exposureSlider' min='0' max='11' value='3' step='1' oninput='setCameraExposure(this.value)'>"
           "<div class='mode-info'>Exposure-based (Auto to Fast). Shutter speed adapts to camera FPS.</div>"
           "</div>"
           "<div class='slider-container'>"
           "<label>Gain: <span id='gainValue'>30</span></label>"
           "<input type='range' id='gainSlider' min='0' max='100' value='30' step='1' oninput='setCameraGain(this.value)'>"
           "<div class='gain-guide'>"
           "<div class='gain-range bright'>0-20<br>Bright</div>"
           "<div class='gain-range normal'>21-50<br>Normal ⭐</div>"
           "<div class='gain-range low'>51-80<br>Low Light</div>"
           "<div class='gain-range dark'>81-100<br>Very Dark</div>"
           "</div>"
           "<div class='mode-info'>Higher gain = brighter image, more noise. Default: 30</div>"
           "</div>"
           "</div>"
           "</div>"
           "<div id='system-tab' class='tab-content'>"
           "<div class='config-section'>"
           "<h3>🌐 Network Status</h3>"
           "<div class='system-info'>"
           "<strong>WiFi AP:</strong> DroneController<br>"
           "<strong>IP Address:</strong> 192.168.4.1 / 10.42.0.1<br>"
           "<strong>Web UI:</strong> http://192.168.4.1:8080<br>"
           "<strong>Network Usage:</strong> <span id='networkUsage'>Calculating...</span>"
           "</div>"
           "<div class='mode-info'>Use 'iftop' or 'nethogs' in terminal for detailed monitoring:<br>"
           "<code style='background:#e9ecef;padding:2px 6px;border-radius:4px'>sudo iftop -i wlP1p1s0</code></div>"
           "</div>"
           "<div class='config-section'>"
           "<h3>💾 Storage Status</h3>"
           "<div class='system-info'>"
           "<strong>USB Label:</strong> DRONE_DATA<br>"
           "<strong>Mount:</strong> /media/angelo/DRONE_DATA/<br>"
           "<strong>Filesystem:</strong> NTFS/exFAT (recommended)"
           "</div>"
           "</div>"
           "<div class='config-section'>"
           "<h3>🖥️ System Control</h3>"
           "<button class='shutdown' onclick='shutdown()'>🔴 SHUTDOWN SYSTEM</button>"
           "</div>"
           "</div>"
           "<div id='power-tab' class='tab-content'>"
           "<div class='config-section'>"
           "<h3>🔋 Battery Monitor</h3>"
           "<div class='system-info' style='text-align:center;padding:40px 20px'>"
           "<p style='font-size:16px;color:#6c757d'>Battery monitoring hardware not yet installed.</p>"
           "<p style='font-size:14px;color:#888'>Future: Voltage, current, capacity, estimated runtime</p>"
           "</div>"
           "</div>"
           "</div>"
           "</div>"
           "<div id='fullscreenOverlay' class='fullscreen-overlay'>"
           "<button class='close-fullscreen' id='closeFullscreenBtn'>✕ Close</button>"
           "<img id='fullscreenImage' class='fullscreen-img' alt='Fullscreen View'/>"
           "</div>"
           "</body></html>";
}

// Helper function to extract FPS from RecordingMode
int getCameraFPSFromMode(RecordingMode mode) {
    switch(mode) {
        case RecordingMode::HD720_60FPS: return 60;
        case RecordingMode::HD720_30FPS: return 30;
        case RecordingMode::HD720_15FPS: return 15;
        case RecordingMode::HD1080_30FPS: return 30;
        case RecordingMode::HD2K_15FPS: return 15;
        case RecordingMode::VGA_100FPS: return 100;
        default: return 60;  // Fallback
    }
}

std::string DroneWebController::generateStatusAPI() {
    RecordingStatus status = getStatus();
    std::ostringstream json;
    
    // Recording mode string
    std::string mode_str;
    switch (status.recording_mode) {
        case RecordingModeType::SVO2: mode_str = "svo2"; break;
        case RecordingModeType::SVO2_DEPTH_INFO: mode_str = "svo2_depth_info"; break;
        case RecordingModeType::SVO2_DEPTH_IMAGES: mode_str = "svo2_depth_images"; break;
        case RecordingModeType::RAW_FRAMES: mode_str = "raw"; break;
    }
    
    json << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
         << "{\"state\":" << static_cast<int>(status.state) << ","
         << "\"recording_time_remaining\":" << status.recording_time_remaining << ","
         << "\"recording_duration_total\":" << status.recording_duration_total << ","
         << "\"bytes_written\":" << status.bytes_written << ","
         << "\"mb_per_second\":" << std::fixed << std::setprecision(2) << status.mb_per_second << ","
         << "\"current_file_path\":\"" << status.current_file_path << "\","
         << "\"recording_mode\":\"" << mode_str << "\","
         << "\"depth_mode\":\"" << status.depth_mode << "\","
         << "\"frame_count\":" << status.frame_count << ","
         << "\"current_fps\":" << std::fixed << std::setprecision(1) << status.current_fps << ","
         << "\"depth_fps\":" << std::fixed << std::setprecision(1) << status.depth_fps << ","
         << "\"camera_fps\":" << getCameraFPSFromMode(camera_resolution_) << ","
         << "\"camera_initializing\":" << (status.camera_initializing ? "true" : "false") << ","
         << "\"camera_exposure\":" << getCameraExposure() << ","
         << "\"camera_gain\":" << getCameraGain() << ","
         << "\"status_message\":\"" << status.status_message << "\","
         << "\"error_message\":\"" << status.error_message << "\"}";
    return json.str();
}

std::string DroneWebController::generateAPIResponse(const std::string& message) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"message\":\"" + message + "\"}";
}

std::string DroneWebController::generateSnapshotJPEG() {
    // CRITICAL: Check shutdown flag FIRST to prevent camera access during teardown
    if (shutdown_requested_) {
        // Return a 1x1 transparent pixel instead of error to prevent GUI flicker
        std::string tiny_gif = "\x47\x49\x46\x38\x39\x61\x01\x00\x01\x00\x80\x00\x00\x00\x00\x00\xFF\xFF\xFF\x21\xF9\x04\x01\x00\x00\x00\x00\x2C\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02\x44\x01\x00\x3B";
        return "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\nContent-Length: " + std::to_string(tiny_gif.length()) + "\r\n\r\n" + tiny_gif;
    }
    
    // SAFETY: Reject snapshot requests during camera reinitialization
    if (camera_initializing_) {
        std::cout << "[WEB_CONTROLLER] Snapshot request rejected - camera reinitializing" << std::endl;
        return "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nCamera reinitializing";
    }
    
    // Check if camera is available
    if (!svo_recorder_ && !raw_recorder_) {
        std::cerr << "[WEB_CONTROLLER] No camera available for snapshot" << std::endl;
        return "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nCamera not initialized";
    }
    
    sl::Camera* camera = nullptr;
    if (svo_recorder_) {
        camera = svo_recorder_->getCamera();
    } else if (raw_recorder_) {
        camera = raw_recorder_->getCamera();
    }
    
    if (!camera || !camera->isOpened()) {
        std::cerr << "[WEB_CONTROLLER] Camera not opened for snapshot" << std::endl;
        return "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nCamera not open";
    }
    
    // Double-check shutdown flag before camera grab (race condition protection)
    if (shutdown_requested_) {
        std::cout << "[WEB_CONTROLLER] Snapshot aborted - shutdown detected" << std::endl;
        return "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\n\r\nServer shutting down";
    }
    
    // Grab a frame
    sl::ERROR_CODE err = camera->grab();
    if (err != sl::ERROR_CODE::SUCCESS) {
        // CORRUPTED_FRAME is common with dark images or covered lens - treat as warning, not error
        if (err == sl::ERROR_CODE::CORRUPTED_FRAME) {
            std::cout << "[WEB_CONTROLLER] Warning: Frame may be corrupted (dark image or covered lens), continuing anyway..." << std::endl;
            // Continue to retrieve image - it will be dark/corrupted but better than no image
        } else {
            std::cerr << "[WEB_CONTROLLER] Failed to grab frame: " << toString(err) << std::endl;
            return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to grab frame";
        }
    }
    
    // Retrieve left image (even if frame was corrupted - ZED Explorer does the same)
    sl::Mat zed_image;
    err = camera->retrieveImage(zed_image, sl::VIEW::LEFT);
    if (err != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "[WEB_CONTROLLER] Failed to retrieve image: " << toString(err) << std::endl;
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to retrieve image";
    }
    
    // Convert ZED Mat to OpenCV Mat
    cv::Mat cv_image(zed_image.getHeight(), zed_image.getWidth(), CV_8UC4, zed_image.getPtr<sl::uchar1>(sl::MEM::CPU));
    cv::Mat cv_image_rgb;
    cv::cvtColor(cv_image, cv_image_rgb, cv::COLOR_BGRA2BGR);
    
    // Encode to JPEG (quality 85 for good balance)
    std::vector<uchar> jpeg_buffer;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 85};
    if (!cv::imencode(".jpg", cv_image_rgb, jpeg_buffer, params)) {
        std::cerr << "[WEB_CONTROLLER] Failed to encode JPEG" << std::endl;
        return "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to encode JPEG";
    }
    
    // Build HTTP response with JPEG data
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: image/jpeg\r\n"
             << "Content-Length: " << jpeg_buffer.size() << "\r\n"
             << "Cache-Control: no-cache, no-store, must-revalidate\r\n"
             << "Pragma: no-cache\r\n"
             << "Expires: 0\r\n"
             << "\r\n";
    
    // Append binary JPEG data
    std::string response_str = response.str();
    response_str.append(reinterpret_cast<const char*>(jpeg_buffer.data()), jpeg_buffer.size());
    
    return response_str;
}

void DroneWebController::signalHandler(int signal) {
    if (instance_) {
        std::cout << "[WEB_CONTROLLER] Received signal " << signal << ", shutting down..." << std::endl;
        instance_->handleShutdown();
    }
}

void DroneWebController::handleShutdown() {
    std::cout << std::endl << "[WEB_CONTROLLER] Initiating shutdown sequence..." << std::endl;
    
    // STEP 1: Set shutdown flag FIRST to stop all new operations
    shutdown_requested_ = true;
    
    // STEP 2: Stop web server IMMEDIATELY to reject new snapshot requests
    // This prevents race conditions with camera closure
    web_server_running_ = false;
    
    // Close server socket to unblock accept() and force server thread to exit
    int fd = server_fd_.load();
    if (fd >= 0) {
        std::cout << "[WEB_CONTROLLER] Closing web server socket..." << std::endl;
        shutdown(fd, SHUT_RDWR);  // Shutdown both directions
        close(fd);
        server_fd_ = -1;
    }
    
    // Wait for web server thread to finish (ensures no more snapshot requests)
    if (web_server_thread_ && web_server_thread_->joinable()) {
        std::cout << "[WEB_CONTROLLER] Waiting for web server thread..." << std::endl;
        web_server_thread_->join();
        web_server_thread_.reset();
        std::cout << "[WEB_CONTROLLER] ✓ Web server thread stopped" << std::endl;
    }
    
    // STEP 3: Now safe to stop recording (no more snapshot interference)
    if (recording_active_) {
        std::cout << "[WEB_CONTROLLER] Stopping active recording..." << std::endl;
        recording_active_ = false;
        current_state_ = RecorderState::STOPPING;
        
        // Stop appropriate recorder
        if ((recording_mode_ == RecordingModeType::SVO2 || 
             recording_mode_ == RecordingModeType::SVO2_DEPTH_IMAGES) && svo_recorder_) {
            svo_recorder_->stopRecording();
        } else if (recording_mode_ == RecordingModeType::RAW_FRAMES && raw_recorder_) {
            raw_recorder_->stopRecording();
        }
        
        current_state_ = RecorderState::IDLE;
        std::cout << "[WEB_CONTROLLER] ✓ Recording stopped" << std::endl;
    }
    
    // STEP 4: Close ZED cameras (safe now - no snapshot requests can arrive)
    std::cout << "[ZED] Closing camera explicitly..." << std::endl;
    if (svo_recorder_) {
        std::cout << "[ZED] Closing ZED camera..." << std::endl;
        svo_recorder_->close();
        std::cout << "[ZED] ✓ SVO recorder closed" << std::endl;
    }
    if (raw_recorder_) {
        std::cout << "[ZED] Closing RAW recorder..." << std::endl;
        raw_recorder_->close();
        std::cout << "[ZED] ✓ RAW recorder closed" << std::endl;
    }
    
    // STEP 5: Teardown WiFi hotspot
    if (hotspot_active_) {
        std::cout << "[WEB_CONTROLLER] Tearing down WiFi hotspot..." << std::endl;
        teardownWiFiHotspot();
        hotspot_active_ = false;
        std::cout << "[WEB_CONTROLLER] ✓ Hotspot teardown complete" << std::endl;
    }
    
    // STEP 6: Stop system monitor thread
    if (system_monitor_thread_ && system_monitor_thread_->joinable()) {
        std::cout << "[WEB_CONTROLLER] Stopping system monitor..." << std::endl;
        system_monitor_thread_->join();
        system_monitor_thread_.reset();
        std::cout << "[WEB_CONTROLLER] ✓ System monitor stopped" << std::endl;
    }
    
    updateLCD("Shutdown", "Complete");
    std::cout << "[WEB_CONTROLLER] ✓✓✓ Shutdown complete ✓✓✓" << std::endl;
}

void DroneWebController::depthVisualizationLoop() {
    int target_fps = depth_recording_fps_.load();
    std::cout << "[DEPTH_VIZ] Depth visualization thread started (target " << target_fps << " FPS)" << std::endl;
    
    sl::Mat depth_map;
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    std::string depth_dir = storage_->getRecordingDir() + "/depth_viz";
    
    while (depth_viz_running_ && recording_active_) {
        // Get current target FPS (in case it changed during recording)
        target_fps = depth_recording_fps_.load();
        
        // Get latest depth map from recorder
        if (svo_recorder_ && svo_recorder_->getLatestDepthMap(depth_map)) {
            
            // Only save if FPS > 0 (FPS=0 is test mode: compute but don't save)
            if (target_fps > 0) {
                // Get current frame number from SVO2 recorder for synchronized naming
                int current_frame = svo_recorder_->getCurrentFrameNumber();
                
                // Convert ZED depth map to OpenCV format
                cv::Mat depth_ocv(depth_map.getHeight(), depth_map.getWidth(), CV_32FC1, depth_map.getPtr<sl::float1>(sl::MEM::CPU));
                
                // Normalize depth to 0-255 (0-10 meters range)
                cv::Mat depth_normalized;
                depth_ocv.convertTo(depth_normalized, CV_8UC1, 255.0 / 10.0);  // 10m max range
                
                // Apply colormap (JET colormap: blue=close, red=far)
                cv::Mat depth_colored;
                cv::applyColorMap(depth_normalized, depth_colored, cv::COLORMAP_JET);
                
                // Save with zero-padded frame number (4 digits: 0001, 0002, ... 0005, ...)
                // This matches the SVO2 frame numbering for synchronized extraction
                char filename_buffer[256];
                snprintf(filename_buffer, sizeof(filename_buffer), "%s/depth_%04d.jpg", 
                         depth_dir.c_str(), current_frame);
                std::string filename(filename_buffer);
                
                cv::imwrite(filename, depth_colored, {cv::IMWRITE_JPEG_QUALITY, 90});
                
                frame_count++;
                
                // Log progress every 30 frames
                if (frame_count % 30 == 0) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
                    float fps = (elapsed > 0) ? (float)frame_count / elapsed : 0;
                    std::cout << "[DEPTH_VIZ] Saved " << frame_count << " depth images (" 
                              << fps << " FPS, last frame: " << current_frame << ")" << std::endl;
                }
            }
        }
        
        // Dynamic sleep based on target FPS
        // FPS=0: Sleep 1 second (test mode, no saving)
        // FPS>0: Sleep according to target frame interval
        int sleep_ms = (target_fps > 0) ? (1000 / target_fps) : 1000;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    
    std::cout << "[DEPTH_VIZ] Depth visualization thread stopped. Total frames saved: " << frame_count << std::endl;
}

