#include "safe_hotspot_manager.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <array>
#include <unistd.h>

// WiFi interface name (Jetson Orin Nano)
static const std::string WIFI_INTERFACE = "wlP1p1s0";
static const std::string LOG_FILE_PATH = "/var/log/drone_controller_network.log";

SafeHotspotManager::SafeHotspotManager() 
    : backup_successful_(false), hotspot_created_(false) {
    
    // Open log file (append mode)
    log_file_.open(LOG_FILE_PATH, std::ios::app);
    if (!log_file_.is_open()) {
        // Try creating in current directory if /var/log/ is not writable
        log_file_.open("drone_network.log", std::ios::app);
    }
    
    log("INFO", "=== SafeHotspotManager initialized ===");
    
    // Automatic backup on construction
    if (!backupWiFiState()) {
        log("ERROR", "Failed to backup WiFi state in constructor");
    } else {
        log("SUCCESS", "WiFi state backup successful");
    }
}

SafeHotspotManager::~SafeHotspotManager() {
    log("INFO", "=== SafeHotspotManager destructor called ===");
    
    // Automatic teardown on destruction (RAII) - only if hotspot was created
    if (hotspot_created_) {
        log("INFO", "Hotspot was created by this instance, tearing down...");
        teardownHotspot();
        
        // Only restore WiFi state if hotspot was actually created
        // (meaning we may have changed autoconnect settings)
        if (backup_successful_) {
            log("INFO", "Restoring original WiFi state...");
            if (restoreWiFiState()) {
                log("SUCCESS", "WiFi state restored successfully");
            } else {
                log("ERROR", "Failed to restore WiFi state");
            }
        }
    } else {
        log("INFO", "Hotspot was never created, skipping restore");
    }
    
    log("INFO", "=== SafeHotspotManager destroyed ===");
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

bool SafeHotspotManager::executeCommand(const std::string& command, std::string& output) const {
    log("INFO", "Executing: " + command);
    
    std::array<char, 128> buffer;
    output.clear();
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        log("ERROR", "popen() failed for command: " + command);
        return false;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    
    int exit_code = pclose(pipe);
    bool success = (exit_code == 0);
    
    if (success) {
        log("INFO", "Command succeeded (exit code 0)");
    } else {
        log("ERROR", "Command failed with exit code: " + std::to_string(exit_code));
    }
    
    return success;
}

int SafeHotspotManager::executeCommandSimple(const std::string& command) const {
    log("INFO", "Executing (simple): " + command);
    int result = system(command.c_str());
    int exit_code = WEXITSTATUS(result);
    
    if (exit_code == 0) {
        log("INFO", "Command succeeded");
    } else {
        log("WARN", "Command exit code: " + std::to_string(exit_code));
    }
    
    return exit_code;
}

void SafeHotspotManager::log(const std::string& level, const std::string& message) const {
    // Get timestamp
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    
    std::string log_line = "[" + timestamp.str() + "] [" + level + "] " + message;
    
    // Write to log file
    if (log_file_.is_open()) {
        log_file_ << log_line << std::endl;
        log_file_.flush();
    }
    
    // Also print to console for debugging
    if (level == "ERROR") {
        std::cerr << "🔴 " << log_line << std::endl;
    } else if (level == "WARN") {
        std::cout << "⚠️  " << log_line << std::endl;
    } else if (level == "SUCCESS") {
        std::cout << "✅ " << log_line << std::endl;
    } else {
        std::cout << "ℹ️  " << log_line << std::endl;
    }
}

bool SafeHotspotManager::isNetworkManagerActive() const {
    std::string output;
    bool active = executeCommand("systemctl is-active NetworkManager 2>/dev/null", 
                                 const_cast<std::string&>(output));
    
    return active && (output.find("active") != std::string::npos);
}

bool SafeHotspotManager::isInterfaceAvailable(const std::string& interface) const {
    std::string output;
    std::string cmd = "ip link show " + interface + " 2>/dev/null";
    bool exists = executeCommand(cmd, const_cast<std::string&>(output));
    
    if (!exists) {
        return false;
    }
    
    // Check if interface is not already in AP mode or busy
    cmd = "nmcli dev status | grep " + interface;
    executeCommand(cmd, const_cast<std::string&>(output));
    
    // Interface should be in "disconnected" or "connected" state (not "unavailable")
    return (output.find("unavailable") == std::string::npos);
}

bool SafeHotspotManager::performPreFlightChecks() {
    log("INFO", "=== Performing pre-flight checks ===");
    
    // Check 1: NetworkManager running
    if (!isNetworkManagerActive()) {
        log("ERROR", "Pre-flight FAILED: NetworkManager is not active");
        return false;
    }
    log("SUCCESS", "✓ NetworkManager is active");
    
    // Check 2: rfkill not blocking WiFi (do this BEFORE checking interface)
    std::string output;
    executeCommand("rfkill list wifi", output);
    if (output.find("Soft blocked: yes") != std::string::npos ||
        output.find("Hard blocked: yes") != std::string::npos) {
        log("WARN", "WiFi is blocked by rfkill, attempting to unblock...");
        executeCommandSimple("rfkill unblock wifi");
        executeCommandSimple("rfkill unblock all");
        sleep(1); // Give it time to unblock
        
        // Verify unblock succeeded
        executeCommand("rfkill list wifi", output);
        if (output.find("Soft blocked: yes") != std::string::npos) {
            log("ERROR", "Pre-flight FAILED: Failed to unblock WiFi");
            return false;
        }
    }
    log("SUCCESS", "✓ rfkill checks passed");
    
    // Check 3: Bring WiFi interface up if it's down
    std::string cmd = "ip link show " + WIFI_INTERFACE + " 2>/dev/null";
    if (!executeCommand(cmd, output)) {
        log("ERROR", "Pre-flight FAILED: WiFi interface " + WIFI_INTERFACE + " does not exist");
        return false;
    }
    
    // Check if interface is down and bring it up
    if (output.find("state DOWN") != std::string::npos) {
        log("INFO", "WiFi interface is down, bringing it up...");
        executeCommandSimple("ip link set " + WIFI_INTERFACE + " up");
        sleep(1);
    }
    log("SUCCESS", "✓ WiFi interface " + WIFI_INTERFACE + " exists");
    
    // Check 4: Ensure NetworkManager is managing the interface
    cmd = "nmcli dev set " + WIFI_INTERFACE + " managed yes 2>/dev/null";
    executeCommandSimple(cmd);
    sleep(1);
    
    // Check 5: Wait for interface to become available
    log("INFO", "Waiting for WiFi interface to become available...");
    for (int i = 0; i < 5; i++) {
        cmd = "nmcli dev status | grep " + WIFI_INTERFACE;
        executeCommand(cmd, output);
        
        if (output.find("unavailable") == std::string::npos) {
            log("SUCCESS", "✓ WiFi interface is available");
            break;
        }
        
        if (i < 4) {
            log("INFO", "Interface still unavailable, waiting... (" + std::to_string(i+1) + "/5)");
            sleep(2);
        } else {
            log("ERROR", "Pre-flight FAILED: WiFi interface remains unavailable after 10 seconds");
            log("ERROR", "Current status: " + output);
            return false;
        }
    }
    
    // Check 6: Verify we have write access to create connection
    if (geteuid() != 0) {
        log("WARN", "Not running as root - may need sudo for network operations");
    }
    
    log("SUCCESS", "=== All pre-flight checks passed ===");
    return true;
}

std::vector<std::string> SafeHotspotManager::getWiFiConnections() const {
    std::vector<std::string> connections;
    std::string output;
    
    std::string cmd = "nmcli -t -f NAME,TYPE con show | grep ':802-11-wireless$' | cut -d: -f1";
    if (executeCommand(cmd, const_cast<std::string&>(output))) {
        std::istringstream stream(output);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line != "\n") {
                // Remove trailing whitespace/newline
                line.erase(line.find_last_not_of(" \n\r\t") + 1);
                connections.push_back(line);
            }
        }
    }
    
    return connections;
}

bool SafeHotspotManager::getAutoconnectState(const std::string& connection_name) const {
    std::string output;
    std::string cmd = "nmcli -t -f connection.autoconnect con show \"" + connection_name + "\" 2>/dev/null";
    
    if (executeCommand(cmd, const_cast<std::string&>(output))) {
        // Output format: "connection.autoconnect:yes" or "connection.autoconnect:no"
        return (output.find(":yes") != std::string::npos);
    }
    
    return false;
}

bool SafeHotspotManager::setAutoconnectState(const std::string& connection_name, bool enable) {
    std::string value = enable ? "yes" : "no";
    std::string cmd = "nmcli con modify \"" + connection_name + "\" connection.autoconnect " + value + " 2>/dev/null";
    
    int exit_code = executeCommandSimple(cmd);
    
    if (exit_code == 0) {
        // Verify the change took effect
        bool current_state = getAutoconnectState(connection_name);
        if (current_state == enable) {
            log("SUCCESS", "Set autoconnect=" + value + " for: " + connection_name);
            return true;
        } else {
            log("ERROR", "Autoconnect change verification FAILED for: " + connection_name);
            return false;
        }
    }
    
    log("ERROR", "Failed to set autoconnect for: " + connection_name);
    return false;
}

bool SafeHotspotManager::backupWiFiState() {
    log("INFO", "=== Backing up WiFi state ===");
    
    wifi_backup_.clear();
    
    // Get all WiFi connections
    std::vector<std::string> connections = getWiFiConnections();
    
    if (connections.empty()) {
        log("WARN", "No WiFi connections found to backup");
        // This is actually OK - might be first time setup
        backup_successful_ = true;
        return true;
    }
    
    log("INFO", "Found " + std::to_string(connections.size()) + " WiFi connection(s)");
    
    // Backup autoconnect state for each connection
    for (const auto& conn : connections) {
        bool autoconnect = getAutoconnectState(conn);
        wifi_backup_[conn] = autoconnect;
        
        std::string state = autoconnect ? "enabled" : "disabled";
        log("INFO", "  Backup: " + conn + " -> autoconnect=" + state);
    }
    
    // Capture full network snapshot for debugging
    std::string snapshot = captureNetworkSnapshot();
    log("INFO", "Network snapshot:\n" + snapshot);
    
    backup_successful_ = true;
    log("SUCCESS", "WiFi state backup completed successfully");
    
    return true;
}

bool SafeHotspotManager::restoreWiFiState() {
    log("INFO", "=== Restoring WiFi state ===");
    
    if (!backup_successful_) {
        log("ERROR", "Cannot restore - no valid backup available");
        return false;
    }
    
    if (wifi_backup_.empty()) {
        log("INFO", "No WiFi connections to restore (backup was empty)");
        return true;
    }
    
    bool all_success = true;
    
    // Restore autoconnect state for each backed-up connection
    for (const auto& pair : wifi_backup_) {
        const std::string& conn_name = pair.first;
        bool autoconnect = pair.second;
        
        std::string state = autoconnect ? "enabled" : "disabled";
        log("INFO", "Restoring: " + conn_name + " -> autoconnect=" + state);
        
        if (!setAutoconnectState(conn_name, autoconnect)) {
            log("ERROR", "Failed to restore autoconnect for: " + conn_name);
            all_success = false;
        }
    }
    
    if (all_success) {
        log("SUCCESS", "All WiFi connections restored successfully");
    } else {
        log("WARN", "Some WiFi connections failed to restore");
    }
    
    return all_success;
}

std::string SafeHotspotManager::captureNetworkSnapshot() const {
    std::ostringstream snapshot;
    std::string output;
    
    // nmcli device status
    if (executeCommand("nmcli dev status 2>/dev/null", const_cast<std::string&>(output))) {
        snapshot << "Device Status:\n" << output << "\n";
    }
    
    // nmcli connection show (active)
    if (executeCommand("nmcli con show --active 2>/dev/null", const_cast<std::string&>(output))) {
        snapshot << "Active Connections:\n" << output << "\n";
    }
    
    // IP addresses
    if (executeCommand("ip addr show " + WIFI_INTERFACE + " 2>/dev/null", const_cast<std::string&>(output))) {
        snapshot << "WiFi Interface IPs:\n" << output << "\n";
    }
    
    return snapshot.str();
}

bool SafeHotspotManager::createHotspot(const std::string& ssid, 
                                       const std::string& password,
                                       const std::string& ip_address) {
    log("INFO", "=== Creating WiFi hotspot ===");
    log("INFO", "SSID: " + ssid);
    log("INFO", "IP: " + ip_address);
    
    // Validate parameters
    if (ssid.empty()) {
        log("ERROR", "SSID cannot be empty");
        return false;
    }
    
    if (password.length() < 8) {
        log("ERROR", "Password must be at least 8 characters");
        return false;
    }
    
    // Pre-flight checks
    if (!performPreFlightChecks()) {
        log("ERROR", "Pre-flight checks failed, aborting hotspot creation");
        return false;
    }
    
    // Store hotspot info
    current_hotspot_ssid_ = ssid;
    hotspot_profile_name_ = ssid;
    
    // Step 1: Disconnect from any existing WiFi (non-destructive)
    log("INFO", "Step 1: Disconnecting from existing WiFi (if connected)...");
    std::string output;
    executeCommand("nmcli dev disconnect " + WIFI_INTERFACE + " 2>/dev/null", output);
    sleep(1);
    
    // Step 2: Check if hotspot profile already exists
    log("INFO", "Step 2: Checking for existing hotspot profile...");
    std::string check_cmd = "nmcli con show \"" + ssid + "\" 2>/dev/null";
    if (executeCommand(check_cmd, output)) {
        log("INFO", "Hotspot profile already exists, deleting old profile...");
        executeCommandSimple("nmcli con delete \"" + ssid + "\" 2>/dev/null");
        sleep(1);
    }
    
    // Step 3: Create hotspot profile
    log("INFO", "Step 3: Creating hotspot profile...");
    std::ostringstream create_cmd;
    create_cmd << "nmcli con add type wifi ifname " << WIFI_INTERFACE
               << " con-name \"" << ssid << "\""
               << " autoconnect no"
               << " ssid \"" << ssid << "\""
               << " mode ap"
               << " 802-11-wireless.band bg"
               << " ipv4.method shared"
               << " ipv4.addresses " << ip_address << "/24";
    
    if (executeCommandSimple(create_cmd.str()) != 0) {
        log("ERROR", "Failed to create hotspot profile");
        rollbackHotspotCreation();
        return false;
    }
    log("SUCCESS", "Hotspot profile created");
    sleep(1);
    
    // Step 4: Set WiFi security
    log("INFO", "Step 4: Configuring WiFi security...");
    std::string security_cmd = "nmcli con modify \"" + ssid + "\" " +
                               "802-11-wireless-security.key-mgmt wpa-psk " +
                               "802-11-wireless-security.psk \"" + password + "\"";
    
    if (executeCommandSimple(security_cmd) != 0) {
        log("ERROR", "Failed to set WiFi security");
        rollbackHotspotCreation();
        return false;
    }
    log("SUCCESS", "WiFi security configured");
    
    // Step 5: Activate hotspot
    log("INFO", "Step 5: Activating hotspot...");
    std::string activate_cmd = "nmcli con up \"" + ssid + "\" 2>&1";
    if (executeCommand(activate_cmd, output)) {
        log("SUCCESS", "Hotspot activated!");
        sleep(2); // Give it time to fully initialize
        
        // Step 6: Verify hotspot is working
        if (verifyHotspot()) {
            log("SUCCESS", "=== Hotspot creation successful and verified ===");
            hotspot_created_ = true;
            return true;
        } else {
            log("ERROR", "Hotspot verification failed");
            rollbackHotspotCreation();
            return false;
        }
    } else {
        log("ERROR", "Failed to activate hotspot");
        log("ERROR", "Output: " + output);
        rollbackHotspotCreation();
        return false;
    }
}

bool SafeHotspotManager::teardownHotspot() {
    log("INFO", "=== Tearing down hotspot ===");
    
    if (current_hotspot_ssid_.empty()) {
        log("INFO", "No hotspot to tear down");
        return true;
    }
    
    // Step 1: Deactivate hotspot connection
    log("INFO", "Deactivating hotspot: " + current_hotspot_ssid_);
    std::string cmd = "nmcli con down \"" + current_hotspot_ssid_ + "\" 2>/dev/null";
    executeCommandSimple(cmd);
    sleep(1);
    
    // Step 2: Delete hotspot profile
    log("INFO", "Deleting hotspot profile: " + current_hotspot_ssid_);
    cmd = "nmcli con delete \"" + current_hotspot_ssid_ + "\" 2>/dev/null";
    executeCommandSimple(cmd);
    sleep(1);
    
    // Step 3: Reset interface to managed mode
    log("INFO", "Resetting WiFi interface to managed mode...");
    cmd = "nmcli dev set " + WIFI_INTERFACE + " managed yes 2>/dev/null";
    executeCommandSimple(cmd);
    
    // Step 4: Bring interface down and up
    executeCommandSimple("ip link set " + WIFI_INTERFACE + " down 2>/dev/null");
    sleep(1);
    executeCommandSimple("ip link set " + WIFI_INTERFACE + " up 2>/dev/null");
    sleep(1);
    
    // Clear state
    current_hotspot_ssid_.clear();
    hotspot_created_ = false;
    
    log("SUCCESS", "Hotspot teardown completed");
    
    return true;
}

void SafeHotspotManager::rollbackHotspotCreation() {
    log("WARN", "=== Rolling back hotspot creation ===");
    
    // Try to delete the partially created profile
    if (!current_hotspot_ssid_.empty()) {
        executeCommandSimple("nmcli con delete \"" + current_hotspot_ssid_ + "\" 2>/dev/null");
        current_hotspot_ssid_.clear();
    }
    
    // Reset interface
    executeCommandSimple("nmcli dev set " + WIFI_INTERFACE + " managed yes 2>/dev/null");
    executeCommandSimple("ip link set " + WIFI_INTERFACE + " down 2>/dev/null");
    sleep(1);
    executeCommandSimple("ip link set " + WIFI_INTERFACE + " up 2>/dev/null");
    
    log("INFO", "Rollback completed");
}

bool SafeHotspotManager::verifyHotspot() const {
    log("INFO", "=== Verifying hotspot ===");
    
    std::string output;
    
    // Check 1: Hotspot connection is active
    std::string cmd = "nmcli -t -f NAME,DEVICE con show --active | grep \"^" + 
                      current_hotspot_ssid_ + ":" + WIFI_INTERFACE + "$\"";
    if (!executeCommand(cmd, const_cast<std::string&>(output)) || output.empty()) {
        log("ERROR", "Verification failed: Hotspot connection not active");
        return false;
    }
    log("SUCCESS", "✓ Hotspot connection is active");
    
    // Check 2: Interface has correct IP address
    cmd = "ip addr show " + WIFI_INTERFACE + " | grep 'inet '";
    if (!executeCommand(cmd, const_cast<std::string&>(output))) {
        log("ERROR", "Verification failed: Cannot get IP address");
        return false;
    }
    
    if (output.find("10.42.0.1") == std::string::npos) {
        log("WARN", "IP address might be different from expected (10.42.0.1)");
        log("INFO", "Current IP info: " + output);
    } else {
        log("SUCCESS", "✓ IP address 10.42.0.1 configured");
    }
    
    // Check 3: Interface is in AP mode
    cmd = "iw dev " + WIFI_INTERFACE + " info | grep type";
    if (executeCommand(cmd, const_cast<std::string&>(output))) {
        if (output.find("AP") != std::string::npos) {
            log("SUCCESS", "✓ Interface is in AP mode");
        } else {
            log("WARN", "Interface may not be in AP mode: " + output);
        }
    }
    
    log("SUCCESS", "=== Hotspot verification passed ===");
    return true;
}

bool SafeHotspotManager::isHotspotActive() const {
    if (current_hotspot_ssid_.empty()) {
        return false;
    }
    
    std::string output;
    std::string cmd = "nmcli -t -f NAME,DEVICE con show --active | grep \"^" + 
                      current_hotspot_ssid_ + ":" + WIFI_INTERFACE + "$\"";
    
    return executeCommand(cmd, const_cast<std::string&>(output)) && !output.empty();
}

std::string SafeHotspotManager::getStatus() const {
    if (isHotspotActive()) {
        return "active";
    } else if (!current_hotspot_ssid_.empty()) {
        return "error";
    } else {
        return "inactive";
    }
}
