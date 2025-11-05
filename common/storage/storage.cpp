#include "storage.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sys/types.h>

namespace fs = std::filesystem;

StorageHandler::StorageHandler() : is_mounted_(false) {
    // Temporärer Mount-Punkt im Home-Verzeichnis
    mount_path_ = "/home/angelo/drone_usb";
}

StorageHandler::~StorageHandler() {
    if (is_mounted_) {
        unmountUSB();
    }
}

std::vector<std::string> StorageHandler::findUSBDevices() {
    std::vector<std::string> devices;
    DIR* dir = opendir("/dev/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            // Suche nach typischen USB-Block-Geräten (sda, sdb, etc.)
            if (name.length() >= 3 && name.substr(0, 2) == "sd") {
                // Nur Partitionen (sda1, sdb1, etc.)
                if (isdigit(name[name.length()-1])) {
                    devices.push_back("/dev/" + name);
                }
            }
        }
        closedir(dir);
    }
    return devices;
}

bool StorageHandler::checkUSBDrive(const std::string& dev_path, const std::string& label) {
    // Prüfe ob Gerät existiert
    if (access(dev_path.c_str(), F_OK) != 0) {
        return false;
    }
    
    // Wir könnten hier noch den Label prüfen, wenn nötig
    // Für jetzt akzeptieren wir jedes gefundene USB-Gerät
    return true;
}

bool StorageHandler::mountUSB(const std::string& dev_path, const std::string& mount_point) {
    // Erstelle Mount-Verzeichnis falls nötig
    if (access(mount_point.c_str(), F_OK) != 0) {
        if (mkdir(mount_point.c_str(), 0755) != 0) {
            std::cerr << "Error creating mount directory: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    // Versuche zu mounten
    int result = mount(dev_path.c_str(), mount_point.c_str(), "vfat", 0, nullptr);
    if (result != 0) {
        // Versuche mit ext4
        result = mount(dev_path.c_str(), mount_point.c_str(), "ext4", 0, nullptr);
    }
    
    if (result != 0) {
        std::cerr << "Error mounting USB drive: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Teste Schreibzugriff
    std::string test_file = mount_point + "/.test_write";
    std::ofstream test(test_file);
    if (!test) {
        std::cerr << "Cannot write to USB drive" << std::endl;
        umount(mount_point.c_str());
        return false;
    }
    test.close();
    unlink(test_file.c_str());
    
    return true;
}

bool StorageHandler::findAndMountUSB(const std::string& label) {
    // Erst prüfen, ob bereits ein USB-Stick unter /media/USER gemountet ist
    std::string user_media_path = "/media/angelo";
    if (access(user_media_path.c_str(), F_OK) == 0) {
        DIR* dir = opendir(user_media_path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                    std::string mount_point = user_media_path + "/" + entry->d_name;
                    
                    // Teste Schreibzugriff
                    std::string test_file = mount_point + "/.test_write";
                    std::ofstream test(test_file);
                    if (test) {
                        test.close();
                        unlink(test_file.c_str());
                        
                        // Verwende diesen bereits gemounteten USB-Stick
                        mount_path_ = mount_point;
                        is_mounted_ = true;
                        closedir(dir);
                        std::cout << "Found already mounted USB at: " << mount_path_ << std::endl;
                        return true;
                    }
                }
            }
            closedir(dir);
        }
    }
    
    // Falls kein bereits gemounteter USB gefunden, versuche selbst zu mounten
    auto devices = findUSBDevices();
    
    for (const auto& dev : devices) {
        std::cout << "Checking device: " << dev << std::endl;
        
        if (checkUSBDrive(dev, label)) {
            if (mountUSB(dev, mount_path_)) {
                is_mounted_ = true;
                return true;
            }
        }
    }
    
    return false;
}

bool StorageHandler::isUSBReady() const {
    return is_mounted_;
}

void StorageHandler::createTimestampedDir() {
    // Aktuelles Datum/Zeit für Verzeichnisname
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << mount_path_ << "/flight_";
    
    struct tm time_info;
    localtime_r(&now_time, &time_info);
    ss << (time_info.tm_year + 1900) 
       << std::setfill('0') << std::setw(2) << (time_info.tm_mon + 1)
       << std::setfill('0') << std::setw(2) << time_info.tm_mday
       << "_"
       << std::setfill('0') << std::setw(2) << time_info.tm_hour
       << std::setfill('0') << std::setw(2) << time_info.tm_min
       << std::setfill('0') << std::setw(2) << time_info.tm_sec;
       
    recording_dir_ = ss.str();
}

bool StorageHandler::createRecordingDir() {
    if (!is_mounted_) return false;
    
    createTimestampedDir();
    
    if (mkdir(recording_dir_.c_str(), 0755) != 0) {
        std::cerr << "Error creating recording directory: " << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

std::string StorageHandler::getVideoPath() const {
    return recording_dir_ + "/video.svo";
}

std::string StorageHandler::getSensorDataPath() const {
    return recording_dir_ + "/sensors.csv";
}

std::string StorageHandler::getLogPath() const {
    return recording_dir_ + "/log.txt";
}

std::string StorageHandler::getMountPath() const {
    return mount_path_;
}

void StorageHandler::unmountUSB() {
    if (is_mounted_) {
        sync();  // Sicherstellen, dass alle Daten geschrieben wurden
        
        // Nur unmounten wenn wir selbst gemountet haben (nicht unter /media/angelo)
        if (mount_path_.find("/media/angelo") != 0) {
            umount(mount_path_.c_str());
        }
        is_mounted_ = false;
    }
}