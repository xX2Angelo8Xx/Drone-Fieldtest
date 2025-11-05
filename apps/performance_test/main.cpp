#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <sys/stat.h>
#include "lcd_handler.h"
#include "storage.h"
#include "zed_recorder.h"

// Globale Variablen für Signalhandler
ZEDRecorder* g_recorder = nullptr;
StorageHandler* g_storage = nullptr;
bool g_running = true;

// Signal-Handler für sauberes Beenden
void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

bool testRecordingMode(RecordingMode mode, LCDHandler& lcd, StorageHandler& storage, int duration_seconds = 10) {
    ZEDRecorder recorder;
    g_recorder = &recorder;
    
    std::cout << "\n=== Testing Mode: " << recorder.getModeName(mode) << " ===" << std::endl;
    
    // Kamera initialisieren
    if (!recorder.init(mode)) {
        std::cerr << "Failed to initialize ZED camera for mode: " << recorder.getModeName(mode) << std::endl;
        return false;
    }
    
    // Starte Aufnahme
    auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    
    struct tm time_info;
    localtime_r(&now_time, &time_info);
    
    std::string mode_name = recorder.getModeName(mode);
    std::replace(mode_name.begin(), mode_name.end(), '@', '_');
    std::replace(mode_name.begin(), mode_name.end(), '/', '_');
    
    std::string test_dir = storage.getMountPath() + "/test_" + mode_name + "_" + 
                          std::to_string(time_info.tm_hour) + std::to_string(time_info.tm_min) + 
                          std::to_string(time_info.tm_sec);
    
    if (mkdir(test_dir.c_str(), 0755) != 0) {
        std::cerr << "Failed to create test directory: " << test_dir << std::endl;
        return false;
    }
    
    std::string video_path = test_dir + "/video.svo2";
    std::string sensor_path = test_dir + "/sensors.csv";
    
    if (!recorder.startRecording(video_path, sensor_path)) {
        std::cerr << "Failed to start recording for mode: " << recorder.getModeName(mode) << std::endl;
        return false;
    }
    
    std::cout << "Recording started for " << duration_seconds << " seconds..." << std::endl;
    
    // Aufnahme-Loop
    while (g_running) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();
        
        if (elapsed >= duration_seconds) {
            break;
        }
        
        // Status alle 2 Sekunden
        if (elapsed % 2 == 0) {
            static int last_status = -1;
            if (elapsed != last_status) {
                last_status = elapsed;
                long bytes = recorder.getBytesWritten();
                float mb_per_sec = (bytes / (1024.0f * 1024.0f)) / (elapsed + 1);
                std::cout << "  " << (duration_seconds - elapsed) << "s remaining, " 
                         << (bytes / (1024 * 1024)) << " MB, " 
                         << std::fixed << std::setprecision(1) << mb_per_sec << " MB/s" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop recording
    recorder.stopRecording();
    
    // Performance-Statistiken
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start_time).count();
    long total_bytes = recorder.getBytesWritten();
    float avg_mb_per_sec = (total_bytes / (1024.0f * 1024.0f)) / total_time;
    
    std::cout << "Mode " << recorder.getModeName(mode) << " completed:" << std::endl;
    std::cout << "  Duration: " << total_time << " seconds" << std::endl;
    std::cout << "  File size: " << (total_bytes / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Average rate: " << std::fixed << std::setprecision(2) << avg_mb_per_sec << " MB/s" << std::endl;
    
    return true;
}

int main(int argc, char** argv) {
    // Signal-Handler registrieren
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Komponenten initialisieren
    LCDHandler lcd;
    StorageHandler storage;
    g_storage = &storage;
    
    std::cout << "ZED Camera Performance Test Suite" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // LCD initialisieren
    if (!lcd.init()) {
        std::cerr << "Failed to initialize LCD" << std::endl;
    }
    
    // USB prüfen
    if (!storage.findAndMountUSB()) {
        std::cerr << "No USB drive found!" << std::endl;
        return 1;
    }
    
    std::cout << "USB mounted at: " << storage.getMountPath() << std::endl;
    
    // Test-Modi definieren
    std::vector<RecordingMode> test_modes = {
        RecordingMode::VGA_100FPS,      // Schnellster Modus zuerst
        RecordingMode::HD720_60FPS,     // Hochauflösend aber schnell
        RecordingMode::HD720_30FPS,     // Balanced
        RecordingMode::HD1080_30FPS,    // High-Res
        RecordingMode::HD2K_15FPS       // Anspruchsvollster Modus
    };
    
    int test_duration = 15; // 15 Sekunden pro Test
    
    for (auto mode : test_modes) {
        if (!g_running) break;
        
        bool success = testRecordingMode(mode, lcd, storage, test_duration);
        if (!success) {
            std::cerr << "Test failed for mode!" << std::endl;
        }
        
        // Kurze Pause zwischen Tests
        std::cout << "Waiting 3 seconds before next test..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}