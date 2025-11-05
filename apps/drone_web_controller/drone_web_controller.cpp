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
#include <iomanip>
#include <cstdlib>

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
        // Initialize ZED recorder
        recorder_ = std::make_unique<ZEDRecorder>();
        if (!recorder_->init()) {
            std::cout << "[WEB_CONTROLLER] ZED camera initialization failed" << std::endl;
            return false;
        }
        
        // Initialize LCD display (no constructor parameters)
        lcd_ = std::make_unique<LCDHandler>();
        if (!lcd_->init()) {
            std::cout << "[WEB_CONTROLLER] LCD initialization failed" << std::endl;
            return false;
        }
        lcd_->displayMessage("Drone Control", "Starting...");
        
        // Initialize storage
        storage_ = std::make_unique<StorageHandler>();
        if (!storage_->findAndMountUSB("DRONE_DATA")) {
            std::cout << "[WEB_CONTROLLER] USB storage not detected" << std::endl;
            return false;
        }
        
        updateLCD("Initialization", "Complete!");
        
        // Start system monitor thread
        system_monitor_thread_ = std::make_unique<std::thread>(&DroneWebController::systemMonitorLoop, this);
        
        std::cout << "[WEB_CONTROLLER] Initialization complete" << std::endl;
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
    
    std::cout << "[WEB_CONTROLLER] Starting recording..." << std::endl;
    updateLCD("Starting", "Recording...");
    
    // Create recording directory
    if (!storage_->createRecordingDir()) {
        std::cout << "[WEB_CONTROLLER] Failed to create recording directory" << std::endl;
        updateLCD("Recording Error", "Dir Failed");
        return false;
    }
    
    // Generate file paths
    std::string video_path = storage_->getVideoPath();
    std::string sensor_path = storage_->getSensorDataPath();
    
    // Start ZED recording
    if (!recorder_->startRecording(video_path, sensor_path)) {
        std::cout << "[WEB_CONTROLLER] Failed to start ZED recording" << std::endl;
        updateLCD("Recording Error", "ZED Failed");
        return false;
    }
    
    recording_active_ = true;
    current_state_ = RecorderState::RECORDING;
    recording_start_time_ = std::chrono::steady_clock::now();
    current_recording_path_ = video_path;
    
    // Start recording monitor thread
    recording_monitor_thread_ = std::make_unique<std::thread>(&DroneWebController::recordingMonitorLoop, this);
    
    updateLCD("Recording", "Active");
    std::cout << "[WEB_CONTROLLER] Recording started: " << video_path << std::endl;
    
    return true;
}

bool DroneWebController::stopRecording() {
    if (!recording_active_) {
        return false;
    }
    
    std::cout << "[WEB_CONTROLLER] Stopping recording..." << std::endl;
    updateLCD("Stopping", "Recording...");
    
    current_state_ = RecorderState::STOPPING;
    
    // Stop ZED recording
    if (recorder_) {
        recorder_->stopRecording();
    }
    
    // Wait for recording monitor thread to finish
    if (recording_monitor_thread_ && recording_monitor_thread_->joinable()) {
        recording_monitor_thread_->join();
    }
    
    recording_active_ = false;
    current_state_ = RecorderState::IDLE;
    
    updateLCD("Recording", "Stopped");
    std::cout << "[WEB_CONTROLLER] Recording stopped" << std::endl;
    
    return true;
}

bool DroneWebController::shutdownSystem() {
    std::cout << "[WEB_CONTROLLER] System shutdown requested" << std::endl;
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
    
    if (current_state_ == RecorderState::RECORDING && recording_active_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        status.recording_time_remaining = recording_duration_seconds_ - elapsed;
        status.recording_duration_total = recording_duration_seconds_;
        
        if (recorder_) {
            status.bytes_written = recorder_->getBytesWritten();
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
        status.recording_time_remaining = 0;
        status.recording_duration_total = recording_duration_seconds_;
        status.bytes_written = 0;
        status.mb_per_second = 0.0;
    }
    
    return status;
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
    // Check if WiFi interface is up and configured
    int result = system("ip addr show wlP1p1s0 | grep -q '192.168.4.1'");
    if (result != 0) {
        return false;
    }
    
    // Check if hostapd and dnsmasq are running
    result = system("pgrep hostapd > /dev/null && pgrep dnsmasq > /dev/null");
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
    while (recording_active_ && !shutdown_requested_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        
        // Check if recording duration reached
        if (elapsed >= recording_duration_seconds_) {
            std::cout << "[WEB_CONTROLLER] Recording duration reached, stopping..." << std::endl;
            break;
        }
        
        updateRecordingStatus();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Auto-stop recording when duration reached
    if (recording_active_) {
        stopRecording();
    }
}

void DroneWebController::webServerLoop(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cout << "[WEB_CONTROLLER] Socket creation failed" << std::endl;
        return;
    }
    
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
    
    while (!shutdown_requested_) {
        // Monitor WiFi connection if hotspot is active
        if (hotspot_active_) {
            restartWiFiIfNeeded();
        }
        
        // Update LCD display
        if (recording_active_) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
            
            std::ostringstream line1;
            line1 << "REC " << elapsed << "s";
            updateLCD(line1.str(), "Recording...");
        } else if (hotspot_active_ && web_server_running_) {
            updateLCD("Web Controller", "192.168.4.1");
        } else {
            updateLCD("Drone Control", "Ready");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "[WEB_CONTROLLER] System monitor thread stopped" << std::endl;
}

bool DroneWebController::setupWiFiHotspot() {
    int result = 0;
    
    // Configure interface step by step
    result |= system("sudo iw dev wlP1p1s0 set type managed");
    result |= system("sudo ip link set dev wlP1p1s0 up");
    
    if (result != 0) {
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    result |= system("sudo iw dev wlP1p1s0 set type __ap");
    result |= system("sudo ip addr add 192.168.4.1/24 dev wlP1p1s0");
    
    if (result != 0) {
        return false;
    }
    
    // Create hostapd configuration
    std::ofstream hostapd_conf("/tmp/hostapd.conf");
    if (!hostapd_conf.is_open()) {
        return false;
    }
    
    hostapd_conf << "interface=wlP1p1s0\n"
                 << "driver=nl80211\n"
                 << "ssid=DroneController\n"
                 << "hw_mode=g\n"
                 << "channel=7\n"
                 << "wpa=2\n"
                 << "wpa_passphrase=drone123\n"
                 << "wpa_key_mgmt=WPA-PSK\n"
                 << "rsn_pairwise=CCMP\n";
    hostapd_conf.close();
    
    // Create dnsmasq configuration
    std::ofstream dnsmasq_conf("/tmp/dnsmasq.conf");
    if (!dnsmasq_conf.is_open()) {
        return false;
    }
    
    dnsmasq_conf << "interface=wlP1p1s0\n"
                 << "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n";
    dnsmasq_conf.close();
    
    // Kill existing processes and start new ones
    system("sudo pkill hostapd");
    system("sudo pkill dnsmasq");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    system("sudo hostapd /tmp/hostapd.conf &");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    system("sudo dnsmasq -C /tmp/dnsmasq.conf &");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return true;
}

bool DroneWebController::teardownWiFiHotspot() {
    system("sudo pkill hostapd");
    system("sudo pkill dnsmasq");
    system("sudo iw dev wlP1p1s0 set type managed");
    system("sudo ip addr flush dev wlP1p1s0");
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
    } else if (request.find("GET /api/status") != std::string::npos) {
        response = generateStatusAPI();
    } else if (request.find("POST /api/start_recording") != std::string::npos) {
        bool success = startRecording();
        response = generateAPIResponse(success ? "Recording started" : "Failed to start recording");
    } else if (request.find("POST /api/stop_recording") != std::string::npos) {
        bool success = stopRecording();
        response = generateAPIResponse(success ? "Recording stopped" : "Failed to stop recording");
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
    return "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
           "<!DOCTYPE html><html><head><title>Drone Controller</title>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<style>body{font-family:Arial;text-align:center;margin:20px;background:#f0f0f0}"
           ".container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:10px}"
           ".status{padding:15px;margin:10px 0;border-radius:5px;font-weight:bold}"
           ".status.idle{background:#e8f5e8;color:#2d5a2d}"
           ".status.recording{background:#fff3cd;color:#856404}"
           ".status.error{background:#f8d7da;color:#721c24}"
           "button{padding:15px 30px;margin:10px;border:none;border-radius:5px;font-size:16px;cursor:pointer}"
           ".start{background:#28a745;color:white}.stop{background:#dc3545;color:white}"
           ".shutdown{background:#6c757d;color:white}</style>"
           "<script>function updateStatus(){fetch('/api/status').then(r=>r.json()).then(data=>{"
           "let stateText = data.state === 0 ? 'IDLE' : data.state === 1 ? 'RECORDING' : 'UNKNOWN';"
           "document.getElementById('status').textContent=stateText;"
           "let isRecording = data.state === 1;"
           "document.getElementById('statusDiv').className='status '+(isRecording?'recording':'idle');"
           "document.getElementById('startBtn').disabled=isRecording;"
           "document.getElementById('stopBtn').disabled=!isRecording;"
           "}).catch(()=>{document.getElementById('statusDiv').className='status error';"
           "document.getElementById('status').textContent='CONNECTION ERROR';});}"
           "function startRecording(){fetch('/api/start_recording',{method:'POST'}).then(()=>updateStatus());}"
           "function stopRecording(){fetch('/api/stop_recording',{method:'POST'}).then(()=>updateStatus());}"
           "function shutdown(){if(confirm('Shutdown?')){fetch('/api/shutdown',{method:'POST'});}}"
           "setInterval(updateStatus,2000);updateStatus();</script></head><body>"
           "<div class='container'><h1>🚁 Drone Controller</h1>"
           "<div id='statusDiv' class='status idle'>Status: <span id='status'>Loading...</span></div>"
           "<button id='startBtn' class='start' onclick='startRecording()'>▶️ Start Recording</button><br>"
           "<button id='stopBtn' class='stop' onclick='stopRecording()'>⏹️ Stop Recording</button><br>"
           "<button class='shutdown' onclick='shutdown()'>🔌 Shutdown</button></div></body></html>";
}

std::string DroneWebController::generateStatusAPI() {
    RecordingStatus status = getStatus();
    std::ostringstream json;
    json << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
         << "{\"state\":" << static_cast<int>(status.state) << ","
         << "\"recording_time_remaining\":" << status.recording_time_remaining << ","
         << "\"recording_duration_total\":" << status.recording_duration_total << ","
         << "\"bytes_written\":" << status.bytes_written << ","
         << "\"mb_per_second\":" << std::fixed << std::setprecision(2) << status.mb_per_second << ","
         << "\"current_file_path\":\"" << status.current_file_path << "\","
         << "\"error_message\":\"" << status.error_message << "\"}";
    return json.str();
}

std::string DroneWebController::generateAPIResponse(const std::string& message) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"message\":\"" + message + "\"}";
}

void DroneWebController::signalHandler(int signal) {
    if (instance_) {
        std::cout << "[WEB_CONTROLLER] Received signal " << signal << ", shutting down..." << std::endl;
        instance_->handleShutdown();
    }
}

void DroneWebController::handleShutdown() {
    std::cout << "[WEB_CONTROLLER] Initiating shutdown sequence..." << std::endl;
    
    shutdown_requested_ = true;
    
    // Stop recording if active
    if (recording_active_) {
        stopRecording();
    }
    
    // Stop web server
    stopWebServer();
    
    // Stop hotspot
    if (hotspot_active_) {
        stopHotspot();
    }
    
    // Wait for system monitor thread
    if (system_monitor_thread_ && system_monitor_thread_->joinable()) {
        system_monitor_thread_->join();
    }
    
    updateLCD("Shutdown", "Complete");
    std::cout << "[WEB_CONTROLLER] Shutdown complete" << std::endl;
}

