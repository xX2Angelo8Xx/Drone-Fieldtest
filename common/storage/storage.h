#pragma once
#include <string>
#include <vector>
#include <functional>

class StorageHandler {
public:
    StorageHandler();
    ~StorageHandler();
    
    // USB-Stick finden und mounten
    bool findAndMountUSB(const std::string& label = "DRONE_DATA");
    
    // Prüft ob USB-Stick bereit ist
    bool isUSBReady() const;
    
    // Erstellt ein Verzeichnis für die aktuelle Aufnahme
    bool createRecordingDir();
    
    // Create directory structure for raw frame recording
    bool createRawRecordingStructure();
    
    // Pfade für die Dateiausgabe
    std::string getVideoPath() const;
    std::string getSensorDataPath() const;
    std::string getLogPath() const;
    
    // Paths for raw frame recording
    std::string getRawBasePath() const;
    std::string getRawLeftPath() const;
    std::string getRawRightPath() const;
    std::string getRawDepthPath() const;
    std::string getRawSensorPath() const;
    
    // USB-Pfad
    std::string getMountPath() const;
    
    // Get current recording directory
    std::string getRecordingDir() const;
    
    // Cleanup
    void unmountUSB();
    
private:
    std::string mount_path_;
    std::string recording_dir_;
    bool is_mounted_;
    
    bool checkUSBDrive(const std::string& dev_path, const std::string& label);
    bool mountUSB(const std::string& dev_path, const std::string& mount_point);
    void createTimestampedDir();
    std::vector<std::string> findUSBDevices();
};