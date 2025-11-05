#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
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

// Hilfsfunktion um Pfade für 16-Zeichen Display zu formatieren
std::string formatPath(const std::string& path) {
    if (path.length() <= 16) return path;
    
    // Zeige nur letzten Teil des Pfades
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash < path.length() - 1) {
        std::string lastPart = path.substr(lastSlash + 1);
        if (lastPart.length() <= 16) return lastPart;
        return lastPart.substr(0, 13) + "...";
    }
    
    return path.substr(0, 13) + "...";
}

int main(int argc, char** argv) {
    // Signal-Handler registrieren
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
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
        // Weiter ausführen, auch wenn LCD fehlschlägt
    }
    
    lcd.showStartupMessage();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Warte auf USB-Stick
    bool usb_ready = false;
    int retry_count = 0;
    const int max_retries = 30; // 30 Sekunden warten
    
    lcd.showUSBWaiting();
    
    while (!usb_ready && retry_count < max_retries && g_running) {
        usb_ready = storage.findAndMountUSB();
        std::cout << "USB check attempt " << (retry_count + 1) << "/" << max_retries 
                  << " - Result: " << (usb_ready ? "SUCCESS" : "FAILED") << std::endl;
        
        if (!usb_ready) {
            std::cout << "Waiting for USB drive... " << (max_retries - retry_count) << "s" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        retry_count++; // Zähler wird IMMER erhöht, um Endlosschleifen zu vermeiden
    }
    
    if (!usb_ready) {
        lcd.showError("No USB found");
        std::cerr << "No USB drive found after " << max_retries << " attempts" << std::endl;
        return 1;
    }
    
    lcd.displayMessage("USB mounted at:", formatPath(storage.getMountPath()));
    std::cout << "USB mounted at: " << storage.getMountPath() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Aufnahmeverzeichnis erstellen
    if (!storage.createRecordingDir()) {
        lcd.showError("Dir creation");
        std::cerr << "Failed to create recording directory" << std::endl;
        return 1;
    }
    
    // ZED Kamera initialisieren (Standard HD720@30fps)
    lcd.showInitializing("ZED Camera");
    if (!recorder.init(RecordingMode::HD720_30FPS)) {
        lcd.showError("Camera init");
        std::cerr << "Failed to initialize ZED camera" << std::endl;
        return 1;
    }
    
    // Starte Aufnahme
    if (!recorder.startRecording(storage.getVideoPath(), storage.getSensorDataPath())) {
        lcd.showError("Start recording");
        std::cerr << "Failed to start recording" << std::endl;
        return 1;
    }
    
    std::cout << "Recording started to: " << storage.getVideoPath() << std::endl;
    
    // Hauptschleife mit Timer
    auto start_time = std::chrono::steady_clock::now();
    const int recording_duration_seconds = 60; // 60 Sekunden für AI-Training (war 20)
    const int max_loop_iterations = recording_duration_seconds * 20; // Sicherheit: max 20 Iterationen pro Sekunde
    int loop_count = 0;
    
    while (g_running) {
        // Sicherheitscheck gegen Endlosschleifen
        if (loop_count > max_loop_iterations) {
            std::cout << "Safety break: maximum loop iterations reached!" << std::endl;
            lcd.showError("Safety stop!");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            break;
        }
        loop_count++;
        
        // Berechne Aufnahmedauer
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - start_time).count();
        
        // Prüfe ob Timer abgelaufen ist
        if (elapsed >= recording_duration_seconds) {
            std::cout << "Recording timer expired (" << recording_duration_seconds << "s), stopping..." << std::endl;
            lcd.showError("Time up!");
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Kurz anzeigen
            break;
        }
        
        // LCD aktualisieren mit verbleibender Zeit
        int remaining = recording_duration_seconds - elapsed;
        lcd.showRecording("Data Collect", recording_duration_seconds, remaining);
        
        // Zeige verbleibende Zeit alle 5 Sekunden (aber nur einmal pro Sekunde prüfen)
        static int last_logged_time = -1;
        if (elapsed % 5 == 0 && elapsed > 0 && elapsed != last_logged_time) {
            last_logged_time = elapsed;
            std::cout << "Recording... " << remaining << "s remaining" << std::endl;
        }
        
        // Kurze Pause
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Sauberes Herunterfahren
    std::cout << "Stopping recording..." << std::endl;
    lcd.showError("Shutdown...");
    recorder.stopRecording();
    storage.unmountUSB();
    
    std::cout << "Recording finished successfully" << std::endl;
    return 0;
}