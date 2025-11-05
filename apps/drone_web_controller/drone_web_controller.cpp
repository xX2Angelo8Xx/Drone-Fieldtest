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
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstdlib>

DroneWebController* g_controller = nullptr;

void signalHandler(int signal) {
    std::cout << "[WEB_CONTROLLER] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_controller) {
        g_controller->handleShutdown();
    }
    exit(0);
}

DroneWebController* DroneWebController::instance_ = nullptr;

DroneWebController::DroneWebController() : 
    recorder_(nullptr),
    storage_(nullptr),
    lcd_(nullptr),
    current_state_(RecorderState::IDLE),
    recording_active_(false),
    hotspot_active_(false),
    web_server_running_(false),
    shutdown_requested_(false),
    wifi_recovery_attempts_(0) {
    
    instance_ = this;
    g_controller = this;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

DroneWebController::~DroneWebController() {
    handleShutdown();
}

bool DroneWebController::initialize() {
    std::cout << "[WEB_CONTROLLER] Initializing..." << std::endl;
    
    if (!initializeHardware()) {
        return false;
    }
    
    if (!initializeStorage()) {
        return false;
    }
    
    updateLCD("Initialization", "Complete!");
    system_monitor_thread_ = std::make_unique<std::thread>(&DroneWebController::systemMonitorLoop, this);
    
    std::cout << "[WEB_CONTROLLER] Initialization complete" << std::endl;
    return true;
}

bool DroneWebController::initializeHardware() {
    try {
        recorder_ = std::make_unique<ZEDRecorder>();
        if (!recorder_->initialize()) {
            std::cout << "[WEB_CONTROLLER] ZED camera initialization failed" << std::endl;
            return false;
        }
        
        lcd_ = std::make_unique<LCDDisplay>("/dev/i2c-7");
        lcd_->displayMessage("Drone Control", "Starting...");
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WEB_CONTROLLER] Hardware error: " << e.what() << std::endl;
        return false;
    }
}

bool DroneWebController::initializeStorage() {
    try {
        storage_ = std::make_unique<StorageHandler>();
        if (!storage_->detectUSB()) {
            std::cout << "[WEB_CONTROLLER] USB storage not detected" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WEB_CONTROLLER] Storage error: " << e.what() << std::endl;
        return false;
    }
}

void DroneWebController::handleShutdown() {
    std::cout << "[WEB_CONTROLLER] Shutdown sequence..." << std::endl;
    
    shutdown_requested_ = true;
    
    if (recording_active_) {
        stopRecording();
    }
    
    stopWebServer();
    
    if (hotspot_active_) {
        stopHotspot();
    }
    
    if (system_monitor_thread_ && system_monitor_thread_->joinable()) {
        system_monitor_thread_->join();
    }
    
    updateLCD("Shutdown", "Complete");
}

void DroneWebController::signalHandler(int signal) {
    if (instance_) {
        instance_->handleShutdown();
    }
}

bool DroneWebController::startHotspot() {
    std::cout << "[WEB_CONTROLLER] Starting WiFi hotspot..." << std::endl;
    updateLCD("Starting WiFi", "Hotspot...");
    
    if (!setupWiFiHotspot()) {
        updateLCD("WiFi Error", "Setup Failed");
        return false;
    }
    
    hotspot_active_ = true;
    updateLCD("WiFi Hotspot", "Active");
    return true;
}

bool DroneWebController::setupWiFiHotspot() {
    int result = 0;
    
    result |= system("sudo iw dev wlP1p1s0 set type managed");
    result |= system("sudo ip link set dev wlP1p1s0 up");
    
    if (result != 0) return false;
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    result |= system("sudo iw dev wlP1p1s0 set type __ap");
    result |= system("sudo ip addr add 192.168.4.1/24 dev wlP1p1s0");
    
    if (result != 0) return false;
    
    std::ofstream hostapd_conf("/tmp/hostapd.conf");
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
    
    system("sudo pkill hostapd");
    system("sudo pkill dnsmasq");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::ofstream dnsmasq_conf("/tmp/dnsmasq.conf");
    dnsmasq_conf << "interface=wlP1p1s0\n"
                 << "dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h\n";
    dnsmasq_conf.close();
    
    system("sudo hostapd /tmp/hostapd.conf &");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    system("sudo dnsmasq -C /tmp/dnsmasq.conf &");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return true;
}

void DroneWebController::stopHotspot() {
    system("sudo pkill hostapd");
    system("sudo pkill dnsmasq");
    system("sudo iw dev wlP1p1s0 set type managed");
    system("sudo ip addr flush dev wlP1p1s0");
    hotspot_active_ = false;
}

bool DroneWebController::startRecording() {
    if (recording_active_) return false;
    
    std::string video_path = storage_->getVideoPath();
    std::string sensor_path = storage_->getSensorDataPath();
    
    if (!recorder_->startRecording(video_path, sensor_path)) {
        return false;
    }
    
    recording_active_ = true;
    current_state_ = RecorderState::RECORDING;
    recording_start_time_ = std::chrono::steady_clock::now();
    
    updateLCD("Recording", "Active");
    return true;
}

bool DroneWebController::stopRecording() {
    if (!recording_active_) return false;
    
    if (recorder_) {
        recorder_->stopRecording();
    }
    
    recording_active_ = false;
    current_state_ = RecorderState::IDLE;
    
    updateLCD("Recording", "Stopped");
    return true;
}

RecordingStatus DroneWebController::getStatus() const {
    RecordingStatus status;
    status.is_recording = recording_active_;
    status.hotspot_active = hotspot_active_;
    status.web_server_running = web_server_running_;
    
    if (current_state_ == RecorderState::RECORDING && recording_active_) {
        status.state = "RECORDING";
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - recording_start_time_).count();
        status.recording_time_remaining = elapsed;
        
        if (recorder_) {
            status.bytes_written = recorder_->getBytesWritten();
            if (elapsed > 0) {
                status.mb_per_second = (status.bytes_written / 1024.0 / 1024.0) / elapsed;
            }
        }
    } else {
        status.state = "IDLE";
        status.recording_time_remaining = 0;
        status.bytes_written = 0;
        status.mb_per_second = 0.0;
    }
    
    return status;
}

void DroneWebController::systemMonitorLoop() {
    while (!shutdown_requested_) {
        if (hotspot_active_ && !isWiFiHealthy()) {
            wifi_recovery_attempts_++;
            if (wifi_recovery_attempts_ >= 3) {
                updateLCD("Connection Error", "Please Wait");
                recoverWiFi();
            }
        } else {
            wifi_recovery_attempts_ = 0;
        }
        
        updateStatusDisplay();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool DroneWebController::isWiFiHealthy() {
    int result = system("ip addr show wlP1p1s0 | grep -q '192.168.4.1'");
    if (result != 0) return false;
    
    result = system("pgrep hostapd > /dev/null && pgrep dnsmasq > /dev/null");
    return result == 0;
}

void DroneWebController::recoverWiFi() {
    updateLCD("WiFi Recovery", "In Progress...");
    stopHotspot();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    if (startHotspot()) {
        wifi_recovery_attempts_ = 0;
        if (web_server_running_) {
            stopWebServer();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            startWebServer(8080);
        }
    }
}

void DroneWebController::updateStatusDisplay() {
    if (!lcd_) return;
    
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
}

bool DroneWebController::shutdownSystem() {
    handleShutdown();
    system("sudo shutdown -h now");
    return true;
}

void DroneWebController::startWebServer(int port) {
    if (web_server_running_) return;
    
    web_server_running_ = true;
    web_server_thread_ = std::make_unique<std::thread>(&DroneWebController::webServerLoop, this, port);
    updateLCD("Web Server", "Active");
}

void DroneWebController::stopWebServer() {
    if (!web_server_running_) return;
    
    web_server_running_ = false;
    if (web_server_thread_ && web_server_thread_->joinable()) {
        web_server_thread_->join();
    }
}

void DroneWebController::webServerLoop(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) return;
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 3) < 0) {
        close(server_fd);
        return;
    }
    
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
}

void DroneWebController::handleClientRequest(int client_socket) {
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    
    std::string request(buffer);
    std::string response;
    
    if (request.find("GET / ") != std::string::npos) {
        response = generateWebInterface();
    } else if (request.find("GET /api/status") != std::string::npos) {
        response = generateStatusAPI();
    } else if (request.find("POST /api/start_recording") != std::string::npos) {
        bool success = startRecording();
        response = generateAPIResponse(success ? "Recording started" : "Failed to start");
    } else if (request.find("POST /api/stop_recording") != std::string::npos) {
        bool success = stopRecording();
        response = generateAPIResponse(success ? "Recording stopped" : "Failed to stop");
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

std::string DroneWebController::generateWebInterface() {
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
           "document.getElementById('status').textContent=data.state;"
           "document.getElementById('statusDiv').className='status '+(data.is_recording?'recording':'idle');"
           "document.getElementById('startBtn').disabled=data.is_recording;"
           "document.getElementById('stopBtn').disabled=!data.is_recording;"
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
         << "{\"state\":\"" << status.state << "\","
         << "\"is_recording\":" << (status.is_recording ? "true" : "false") << ","
         << "\"hotspot_active\":" << (status.hotspot_active ? "true" : "false") << ","
         << "\"web_server_running\":" << (status.web_server_running ? "true" : "false") << ","
         << "\"recording_time_remaining\":" << status.recording_time_remaining << ","
         << "\"bytes_written\":" << status.bytes_written << ","
         << "\"mb_per_second\":" << std::fixed << std::setprecision(2) << status.mb_per_second << "}";
    return json.str();
}

std::string DroneWebController::generateAPIResponse(const std::string& message) {
    return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"message\":\"" + message + "\"}";
}

void DroneWebController::updateLCD(const std::string& line1, const std::string& line2) {
    if (lcd_) {
        lcd_->displayMessage(line1, line2);
    }
}

int main() {
    std::cout << "=== Drone Web Controller v1.0 ===" << std::endl;
    
    auto controller = std::make_unique<DroneWebController>();
    
    if (!controller->initialize()) {
        std::cout << "[MAIN] Initialization failed" << std::endl;
        return 1;
    }
    
    if (!controller->startHotspot()) {
        std::cout << "[MAIN] Failed to start WiFi hotspot" << std::endl;
        return 1;
    }
    
    controller->startWebServer(8080);
    
    std::cout << "[MAIN] System ready - Access via http://192.168.4.1:8080" << std::endl;
    std::cout << "[MAIN] WiFi: DroneController / Password: drone123" << std::endl;
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
