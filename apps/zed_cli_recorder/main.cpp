#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstring>
#include "lcd_handler.h"
#include "storage.h"

// Globale Variablen für Signalhandler
pid_t g_zed_process = 0;
StorageHandler* g_storage = nullptr;
bool g_running = true;
bool g_recording_finished = false;

// Signal-Handler für sauberes Beenden
void signalHandler(int signal) {
    std::cout << "\n[ZED-CLI-RECORDER] Signal received: " << signal << std::endl;
    g_running = false;
    
    // Beende ZED Explorer Prozess sanft
    if (g_zed_process > 0) {
        std::cout << "[ZED-CLI-RECORDER] Terminating ZED Explorer process..." << std::endl;
        kill(g_zed_process, SIGTERM);
        
        // Warte kurz auf sauberes Beenden
        int status;
        if (waitpid(g_zed_process, &status, WNOHANG) == 0) {
            // Prozess läuft noch, force kill
            std::this_thread::sleep_for(std::chrono::seconds(2));
            kill(g_zed_process, SIGKILL);
            waitpid(g_zed_process, &status, 0);
            std::cout << "[ZED-CLI-RECORDER] ZED Explorer process terminated forcefully" << std::endl;
        } else {
            std::cout << "[ZED-CLI-RECORDER] ZED Explorer process terminated gracefully" << std::endl;
        }
        g_zed_process = 0;
    }
}

// Hilfsfunktion um Pfade für 16-Zeichen Display zu formatieren
std::string formatPath(const std::string& path) {
    if (path.length() <= 16) return path;
    
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash != std::string::npos && lastSlash < path.length() - 1) {
        std::string lastPart = path.substr(lastSlash + 1);
        if (lastPart.length() <= 16) return lastPart;
        return lastPart.substr(0, 13) + "...";
    }
    
    return path.substr(0, 13) + "...";
}

int main(int argc, char** argv) {
    std::cout << "🎥 ZED CLI RECORDER - Explorer Backend Approach" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "Using ZED Explorer for 4GB+ corruption-free recording" << std::endl << std::endl;
    
    // Parse command line arguments
    int duration_seconds = 240; // Default: 4 minutes
    std::string resolution = "HD720"; // Default resolution
    int framerate = 15; // Default: 15fps für stabile Performance
    
    if (argc > 1) {
        duration_seconds = std::stoi(argv[1]);
        std::cout << "Custom duration: " << duration_seconds << " seconds" << std::endl;
    }
    if (argc > 2) {
        resolution = argv[2];
        std::cout << "Custom resolution: " << resolution << std::endl;
    }
    if (argc > 3) {
        framerate = std::stoi(argv[3]);
        std::cout << "Custom framerate: " << framerate << " fps" << std::endl;
    }
    
    // Signal-Handler registrieren
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Komponenten initialisieren
    LCDHandler lcd;
    StorageHandler storage;
    g_storage = &storage;
    
    // LCD initialisieren (non-blocking)
    if (!lcd.init()) {
        std::cout << "⚠️  LCD init failed - continuing without display" << std::endl;
    } else {
        lcd.displayMessage("ZED CLI Init", "Starting...");
    }
    
    // USB-Speicher suchen
    std::cout << "🔍 Searching for USB storage..." << std::endl;
    lcd.displayMessage("USB Search", "Finding DRONE_DATA");
    
    while (!storage.findAndMountUSB("DRONE_DATA") && g_running) {
        std::cout << "USB storage not found, retrying in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    if (!g_running) {
        std::cout << "Interrupted during USB search" << std::endl;
        return 1;
    }
    
    // Recording-Verzeichnis erstellen
    if (!storage.createRecordingDir()) {
        std::cerr << "❌ Failed to create recording directory" << std::endl;
        return 1;
    }
    
    std::string video_path = storage.getVideoPath();
    std::cout << "📹 Recording to: " << video_path << std::endl;
    std::cout << "⏱️  Duration: " << duration_seconds << " seconds (" << duration_seconds/60.0 << " minutes)" << std::endl;
    
    lcd.displayMessage("Recording Setup", formatPath(video_path));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // ZED Explorer Kommando erstellen
    int total_frames = duration_seconds * framerate;  // Berechne Gesamtzahl der Frames
    std::vector<std::string> zed_cmd = {
        "/usr/local/zed/tools/ZED_Explorer",
        "--output", video_path,
        "--resolution", resolution,
        "--frequency", std::to_string(framerate),
        "--length", std::to_string(total_frames),  // Setze Frame-Limit für gewünschte Dauer
        "--compression_mode", "1"  // LOSSLESS ZST (wie im aktuellen System)
    };
    
    std::cout << "🚀 Starting ZED Explorer with command:" << std::endl;
    std::cout << "   ";
    for (const auto& arg : zed_cmd) {
        std::cout << arg << " ";
    }
    std::cout << std::endl << std::endl;
    
    // Fork und exec ZED Explorer
    g_zed_process = fork();
    if (g_zed_process == 0) {
        // Child process - exec ZED Explorer
        std::vector<char*> args;
        for (const auto& arg : zed_cmd) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);
        
        execv(args[0], args.data());
        
        // Wenn wir hier ankommen, ist exec fehlgeschlagen
        std::cerr << "❌ Failed to exec ZED Explorer" << std::endl;
        exit(1);
    } else if (g_zed_process > 0) {
        // Parent process - Monitor und Timer
        std::cout << "✅ ZED Explorer started (PID: " << g_zed_process << ")" << std::endl;
        lcd.displayMessage("Recording", "ZED Explorer ON");
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        // Monitoring Loop
        while (g_running && std::chrono::steady_clock::now() < end_time) {
            // Check if ZED process is still running
            int status;
            pid_t result = waitpid(g_zed_process, &status, WNOHANG);
            
            if (result == g_zed_process) {
                // Process finished
                if (WIFEXITED(status)) {
                    std::cout << "✅ ZED Explorer finished with exit code: " << WEXITSTATUS(status) << std::endl;
                } else {
                    std::cout << "⚠️  ZED Explorer terminated abnormally" << std::endl;
                }
                g_zed_process = 0;
                g_recording_finished = true;
                break;
            } else if (result == -1) {
                std::cerr << "❌ Error checking ZED process status" << std::endl;
                break;
            }
            
            // Update LCD with remaining time
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(end_time - std::chrono::steady_clock::now()).count();
            if (remaining > 0) {
                std::string time_msg = std::to_string(remaining) + "s left";
                lcd.displayMessage("Recording", time_msg);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Timer abgelaufen - ZED Explorer beenden
        if (g_running && g_zed_process > 0) {
            std::cout << "⏰ Recording time finished - stopping ZED Explorer..." << std::endl;
            lcd.displayMessage("Stopping", "Time finished");
            
            kill(g_zed_process, SIGTERM);
            
            // Warte auf sauberes Beenden
            int status;
            auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < timeout) {
                if (waitpid(g_zed_process, &status, WNOHANG) == g_zed_process) {
                    std::cout << "✅ ZED Explorer terminated gracefully" << std::endl;
                    g_zed_process = 0;
                    g_recording_finished = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Force kill wenn nötig
            if (g_zed_process > 0) {
                std::cout << "⚠️  Force killing ZED Explorer..." << std::endl;
                kill(g_zed_process, SIGKILL);
                waitpid(g_zed_process, &status, 0);
                g_zed_process = 0;
                g_recording_finished = true;
            }
        }
        
    } else {
        std::cerr << "❌ Failed to fork ZED Explorer process" << std::endl;
        return 1;
    }
    
    // Final status
    if (g_recording_finished) {
        std::cout << std::endl << "🎉 RECORDING COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "📄 Video file: " << video_path << std::endl;
        
        lcd.displayMessage("Completed!", "Check video file");
        
        // Prüfe Dateigröße
        if (access(video_path.c_str(), F_OK) == 0) {
            std::string size_cmd = "ls -lh \"" + video_path + "\" | awk '{print $5}'";
            FILE* fp = popen(size_cmd.c_str(), "r");
            if (fp) {
                char size_str[64];
                if (fgets(size_str, sizeof(size_str), fp) != nullptr) {
                    // Remove newline
                    size_t len = strlen(size_str);
                    if (len > 0 && size_str[len-1] == '\n') {
                        size_str[len-1] = '\0';
                    }
                    std::cout << "📊 File size: " << size_str << std::endl;
                }
                pclose(fp);
            }
        } else {
            std::cout << "⚠️  Warning: Video file not found at expected location" << std::endl;
        }
        
    } else {
        std::cout << std::endl << "⚠️  Recording interrupted or failed" << std::endl;
        lcd.displayMessage("Interrupted", "Check logs");
    }
    
    // Cleanup
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    return g_recording_finished ? 0 : 1;
}