#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <map>
#include <string>
#include <future>
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

// Vordefinierte Profile für verschiedene Use Cases
struct RecordingProfile {
    RecordingMode mode;
    int duration_seconds;
    std::string description;
    std::string use_case;
};

std::map<std::string, RecordingProfile> getRecordingProfiles() {
    return {
        {"training", {
            RecordingMode::HD1080_30FPS,
            60,
            "Training-Qualität für AI Model Development",
            "Beste Balance für KI-Training mit hoher Auflösung"
        }},
        {"realtime_light", {
            RecordingMode::HD720_15FPS,
            30,
            "Feldaufnahmen mit maximaler Zuverlässigkeit",
            "15 FPS für garantiert stabile Performance ohne Frame Drops"
        }},
        {"standard", {
            RecordingMode::HD720_30FPS,
            240,
            "Standard Drone Recording (4min HD720@30FPS)",
            "Neues Standard-Profil: 4 Minuten, keine 4GB Limits mit NTFS"
        }},
        {"realtime_30fps", {
            RecordingMode::HD720_30FPS,
            30,
            "Optimierte Feldaufnahmen mit 30 FPS",
            "HD720@30FPS mit Frame Drop Prevention für AI-Training"
        }},
        {"realtime_heavy", {
            RecordingMode::VGA_100FPS,
            30,
            "Maximale Performance für schnelle Bewegungen",
            "100 FPS für High-Speed Tracking und Analyse"
        }},
        {"development", {
            RecordingMode::HD720_60FPS,
            10,
            "Schnelle Entwicklungstests",
            "Kurze Tests für Code-Entwicklung"
        }},
        {"ultra_quality", {
            RecordingMode::HD2K_15FPS,
            30,
            "Maximale Bildqualität",
            "2K Auflösung für detaillierte Analyse"
        }},
        {"quick_test", {
            RecordingMode::HD720_30FPS,
            5,
            "Schneller Systemtest",
            "5-Sekunden Test für Funktionalität"
        }},
        {"long_mission", {
            RecordingMode::HD720_15FPS,  // REDUCED: 15fps to prevent buffer buildup
            120,  // 2 Minuten LOSSLESS - reduced fps for gap prevention
            "Gap-Free Mission (2min 15fps)",
            "2-Minute 15FPS to prevent ZED buffer overruns and gaps"
        }},
        {"extended_mission", {
            RecordingMode::HD720_15FPS,
            180,  // 3 Minuten 15FPS - reduzierte Memory Usage
            "Erweiterte Mission (3min 15FPS)",
            "3-Minute 15FPS reduziert Memory-Usage"
        }},
        {"endurance_mission", {
            RecordingMode::HD720_15FPS,
            240,  // 4 Minuten 15FPS - Maximum für LOSSLESS
            "Ausdauer-Mission (4min 15FPS)",
            "4-Minute 15FPS für längere stabile Aufnahmen"
        }},
        {"zed_explorer_test", {
            RecordingMode::HD2K_15FPS,
            200,  // 200 seconds = 3.33 minutes (same as ZED Explorer test)
            "ZED Explorer Replication Test",
            "HD2K@15fps mimicking ZED Explorer recording approach"
        }},
        {"test_4gb_plus", {
            RecordingMode::HD1080_30FPS,
            240,  // 4 Minutes HD1080@30fps = ~9.6GB (definitely >4GB)
            "TEST: Force >4GB file creation",
            "Test large file corruption fixes - HD1080@30fps for 4min"
        }},
        {"basic_4min_test", {
            RecordingMode::HD720_15FPS,
            240,  // 4 minutes basic test
            "BASIC: 4min HD720@15fps corruption test",
            "Most basic 4-minute recording to test corruption fixes"
        }},
        {"long_mission_5min", {
            RecordingMode::HD720_15FPS,
            300,  // 5 minutes - continuous recording (no segmentation needed)
            "Long Mission: 5min HD720@15fps continuous",
            "5-minute continuous recording - demonstrates >4GB support on NTFS/exFAT"
        }},
        {"extended_mission_4min", {
            RecordingMode::HD720_15FPS,
            240,  // 4 minutes - continuous recording 
            "Extended Mission: 4min HD720@15fps continuous",
            "4-minute continuous recording - reliable field deployment profile"
        }},
        {"instant_swap_test", {
            RecordingMode::HD720_15FPS,
            240,  // 4 minutes - test true dual camera instant swapping
            "INSTANT-SWAP: 4min HD720@15fps dual camera (<10s goal)",
            "Test dual camera instances for instant swapping with <10s gaps"
        }}
    };
}

void printProfiles() {
    auto profiles = getRecordingProfiles();
    
    std::cout << "\n🎯 VERFÜGBARE RECORDING-PROFILE:" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    for (const auto& [key, profile] : profiles) {
        std::cout << key << ":" << std::endl;
        std::cout << "  Modus: " << profile.description << std::endl;
        std::cout << "  Dauer: " << profile.duration_seconds << " Sekunden" << std::endl;
        std::cout << "  Use Case: " << profile.use_case << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "Verwendung: ./smart_recorder [profil_name]" << std::endl;
    std::cout << "Standard: ./smart_recorder realtime_light" << std::endl;
}

int main(int argc, char** argv) {
    // Signal-Handler registrieren
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Profile laden
    auto profiles = getRecordingProfiles();
    
    // Standard-Profil oder aus Kommandozeile
    std::string selected_profile = "realtime_light";
    
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            printProfiles();
            return 0;
        }
        if (profiles.find(arg) != profiles.end()) {
            selected_profile = arg;
        } else {
            std::cout << "❌ Unbekanntes Profil: " << arg << std::endl;
            printProfiles();
            return 1;
        }
    }
    
    RecordingProfile profile = profiles[selected_profile];
    
    std::cout << "🚁 SMART DRONE RECORDER" << std::endl;
    std::cout << "Gewähltes Profil: " << selected_profile << std::endl;
    std::cout << "Modus: " << profile.description << std::endl;
    std::cout << "Dauer: " << profile.duration_seconds << " Sekunden" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    // Komponenten initialisieren
    LCDHandler lcd;
    StorageHandler storage;
    ZEDRecorder recorder;
    
    // Für Signal-Handler
    g_recorder = &recorder;
    g_storage = &storage;
    
    // LCD initialisieren
    if (!lcd.init()) {
        std::cerr << "Failed to initialize LCD" << std::endl;
    }
    
    lcd.showStartupMessage();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // USB prüfen
    bool usb_ready = false;
    int retry_count = 0;
    const int max_retries = 10;
    
    lcd.showUSBWaiting();
    
    while (!usb_ready && retry_count < max_retries && g_running) {
        usb_ready = storage.findAndMountUSB();
        std::cout << "USB check attempt " << (retry_count + 1) << "/" << max_retries 
                  << " - Result: " << (usb_ready ? "SUCCESS" : "FAILED") << std::endl;
        
        if (!usb_ready) {
            std::cout << "Waiting for USB drive..." << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        retry_count++;
    }
    
    if (!usb_ready) {
        lcd.showError("No USB found");
        std::cerr << "No USB drive found!" << std::endl;
        return 1;
    }
    
    // Aufnahmeverzeichnis erstellen
    if (!storage.createRecordingDir()) {
        lcd.showError("Dir creation");
        std::cerr << "Failed to create recording directory" << std::endl;
        return 1;
    }
    
    // ZED Kamera mit gewähltem Profil initialisieren
    lcd.showInitializing("ZED Camera");
    if (!recorder.init(profile.mode)) {
        lcd.showError("Camera init");
        std::cerr << "Failed to initialize ZED camera" << std::endl;
        return 1;
    }
    
    // Initialize dual camera mode for instant swap test
    if (selected_profile == "instant_swap_test") {
        lcd.showInitializing("Dual Camera");
        std::cout << "[INSTANT-SWAP] Initializing dual camera mode for <10s gaps..." << std::endl;
        if (!recorder.initDualCamera()) {
            std::cerr << "Failed to initialize dual camera mode, continuing with single camera" << std::endl;
        } else {
            std::cout << "[INSTANT-SWAP] Dual camera mode ready!" << std::endl;
        }
    }
    
    // Auto-segmentation DISABLED - no longer needed with NTFS/exFAT >4GB support
    std::cout << "[INFO] Continuous recording enabled - files can exceed 4GB on NTFS/exFAT" << std::endl;
    
    // Starte Aufnahme
    if (!recorder.startRecording(storage.getVideoPath(), storage.getSensorDataPath())) {
        lcd.showError("Start recording");
        std::cerr << "Failed to start recording" << std::endl;
        return 1;
    }
    
    std::cout << "Recording started to: " << storage.getVideoPath() << std::endl;
    
    // Hauptschleife mit Timer
    auto start_time = std::chrono::steady_clock::now();
    const int recording_duration_seconds = profile.duration_seconds;
    
    while (g_running) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();
        
        if (elapsed >= recording_duration_seconds) {
            std::cout << "Recording timer expired (" << recording_duration_seconds << "s), stopping..." << std::endl;
            lcd.showError("Time up!");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            break;
        }
        
        // LCD aktualisieren mit verbleibender Zeit
        int remaining = recording_duration_seconds - elapsed;
        lcd.showRecording("Recording", recording_duration_seconds, remaining);
        
        // Status alle 5 Sekunden
        static int last_logged_time = -1;
        if (elapsed % 5 == 0 && elapsed > 0 && elapsed != last_logged_time) {
            last_logged_time = elapsed;
            long bytes = recorder.getBytesWritten();
            float mb_per_sec = (bytes / (1024.0f * 1024.0f)) / (elapsed + 1);
            
            // Show current segment if auto-segmentation is enabled
            // Segmentation removed - continuous recording only
            std::string segment_info = "";
            std::cout << "Recording... " << remaining << "s remaining, " 
                     << (bytes / (1024 * 1024)) << " MB, " 
                     << std::fixed << std::setprecision(1) << mb_per_sec << " MB/s" 
                     << segment_info << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Sauberes Herunterfahren
    std::cout << "Stopping recording..." << std::endl;
    lcd.showError("Shutdown...");
    
    // Stoppe Aufnahme mit Timeout
    auto shutdown_start = std::chrono::steady_clock::now();
    recorder.stopRecording();
    auto shutdown_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - shutdown_start).count();
    std::cout << "Recording stopped in " << shutdown_time << " seconds." << std::endl;
    
    // Explicitly close ZED camera for clean shutdown
    recorder.close();
    
    std::cout << "Unmounting USB..." << std::endl;
    storage.unmountUSB();
    std::cout << "USB unmounted." << std::endl;
    
    std::cout << "Recording finished successfully" << std::endl;
    return 0;
}
