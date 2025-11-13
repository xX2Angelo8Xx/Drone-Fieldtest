#ifndef SAFE_HOTSPOT_MANAGER_H
#define SAFE_HOTSPOT_MANAGER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>

/**
 * SafeHotspotManager - NETWORK SAFETY POLICY COMPLIANT
 * 
 * This class provides safe WiFi hotspot management that NEVER breaks system connectivity.
 * 
 * Key Safety Features:
 * - Automatic backup of ALL WiFi connection states before ANY changes
 * - Verification after every operation
 * - Automatic restore on failure (RAII pattern)
 * - Comprehensive logging to /var/log/drone_controller_network.log
 * - Pre-flight checks (NetworkManager running, interface available, etc.)
 * - NO direct process killing (pkill forbidden)
 * - NO manual DNS modification (uses NetworkManager only)
 * 
 * Usage:
 *   SafeHotspotManager manager;
 *   if (manager.createHotspot("DroneController", "drone123", "10.42.0.1")) {
 *       // Hotspot is active
 *   }
 *   // Destructor automatically restores WiFi state on exit
 * 
 * Complies with: /home/angelo/Projects/Drone-Fieldtest/NETWORK_SAFETY_POLICY.md
 */
class SafeHotspotManager {
public:
    /**
     * Constructor - Automatically backs up current WiFi state
     */
    SafeHotspotManager();
    
    /**
     * Destructor - Automatically restores WiFi state (RAII)
     */
    ~SafeHotspotManager();
    
    /**
     * Create WiFi hotspot with safety checks
     * 
     * @param ssid Hotspot SSID (e.g., "DroneController")
     * @param password Hotspot password (min 8 chars)
     * @param ip_address IP address for hotspot (e.g., "10.42.0.1")
     * @return true if hotspot created successfully, false on error
     * 
     * Safety: Performs pre-flight checks, backs up state, verifies creation
     */
    bool createHotspot(const std::string& ssid, 
                      const std::string& password,
                      const std::string& ip_address = "10.42.0.1");
    
    /**
     * Teardown hotspot and restore original WiFi state
     * 
     * @return true if teardown successful, false on error
     * 
     * Safety: Verifies restore, logs all operations
     */
    bool teardownHotspot();
    
    /**
     * Check if hotspot is currently active
     * 
     * @return true if DroneController hotspot is active
     */
    bool isHotspotActive() const;
    
    /**
     * Get current hotspot status (for monitoring)
     * 
     * @return Status string: "active", "inactive", "error"
     */
    std::string getStatus() const;
    
    /**
     * Manually backup WiFi state (normally done in constructor)
     * 
     * @return true if backup successful
     */
    bool backupWiFiState();
    
    /**
     * Manually restore WiFi state (normally done in destructor)
     * 
     * @return true if restore successful
     */
    bool restoreWiFiState();
    
    /**
     * Verify hotspot is working (checks connection, IP, AP mode)
     * 
     * @return true if hotspot verified working
     */
    bool verifyHotspot() const;

private:
    // WiFi connection state backup: connection_name -> autoconnect_enabled
    std::map<std::string, bool> wifi_backup_;
    
    // Current hotspot SSID (empty if no hotspot active)
    std::string current_hotspot_ssid_;
    
    // Hotspot connection profile name
    std::string hotspot_profile_name_;
    
    // Log file for network operations (mutable for const methods)
    mutable std::ofstream log_file_;
    
    // Flag: true if backup was successful
    bool backup_successful_;
    
    // Flag: true if hotspot was created by this instance
    bool hotspot_created_;
    
    /**
     * Execute shell command and capture output
     * 
     * @param command Command to execute
     * @param output Reference to store command output
     * @return true if command succeeded (exit code 0)
     */
    bool executeCommand(const std::string& command, std::string& output) const;
    
    /**
     * Execute shell command without capturing output (fire-and-forget)
     * 
     * @param command Command to execute
     * @return exit code (0 = success)
     */
    int executeCommandSimple(const std::string& command) const;
    
    /**
     * Log message to file and console
     * 
     * @param level Log level: "INFO", "WARN", "ERROR", "SUCCESS"
     * @param message Log message
     */
    void log(const std::string& level, const std::string& message) const;
    
    /**
     * Pre-flight checks before creating hotspot
     * 
     * @return true if all checks passed
     */
    bool performPreFlightChecks();
    
    /**
     * Check if NetworkManager is running and managing WiFi
     * 
     * @return true if NetworkManager is active
     */
    bool isNetworkManagerActive() const;
    
    /**
     * Check if WiFi interface exists and is available
     * 
     * @param interface Interface name (e.g., "wlP1p1s0")
     * @return true if interface exists and is not busy
     */
    bool isInterfaceAvailable(const std::string& interface) const;
    
    /**
     * Query current autoconnect state for a connection
     * 
     * @param connection_name Connection name
     * @return true if autoconnect enabled, false otherwise
     */
    bool getAutoconnectState(const std::string& connection_name) const;
    
    /**
     * Set autoconnect state for a connection
     * 
     * @param connection_name Connection name
     * @param enable true to enable autoconnect, false to disable
     * @return true if operation successful
     */
    bool setAutoconnectState(const std::string& connection_name, bool enable);
    
    /**
     * Get list of all WiFi connections
     * 
     * @return Vector of connection names
     */
    std::vector<std::string> getWiFiConnections() const;
    
    /**
     * Create snapshot of current network state for debugging
     * 
     * @return String with network state information
     */
    std::string captureNetworkSnapshot() const;
    
    /**
     * Rollback partial hotspot creation on failure
     */
    void rollbackHotspotCreation();
};

#endif // SAFE_HOTSPOT_MANAGER_H
