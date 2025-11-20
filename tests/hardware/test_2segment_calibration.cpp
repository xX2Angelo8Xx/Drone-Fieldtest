/**
 * Test 2-segment calibration loading
 */
#include "common/hardware/battery/battery_monitor.h"
#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "=" << std::string(80, '=') << std::endl;
    std::cout << "  2-SEGMENT CALIBRATION TEST" << std::endl;
    std::cout << "=" << std::string(80, '=') << std::endl;
    std::cout << std::endl;
    
    // Initialize battery monitor (will auto-load calibration)
    BatteryMonitor monitor;
    
    if (!monitor.initialize()) {
        std::cerr << "Failed to initialize battery monitor" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Waiting 3 seconds for readings..." << std::endl;
    sleep(3);
    
    // Get status
    BatteryStatus status = monitor.getStatus();
    
    std::cout << std::endl;
    std::cout << "=" << std::string(80, '=') << std::endl;
    std::cout << "  CURRENT READING" << std::endl;
    std::cout << "=" << std::string(80, '=') << std::endl;
    std::cout << std::endl;
    std::cout << "Voltage:          " << status.voltage << "V" << std::endl;
    std::cout << "Cell Voltage:     " << status.cell_voltage << "V" << std::endl;
    std::cout << "Current:          " << status.current << "A" << std::endl;
    std::cout << "Power:            " << status.power << "W" << std::endl;
    std::cout << "Battery %:        " << status.battery_percentage << "%" << std::endl;
    std::cout << "Status:           " << (status.is_healthy ? "Healthy" : 
                                          status.is_critical ? "Critical" : 
                                          status.is_warning ? "Warning" : "Unknown") << std::endl;
    std::cout << std::endl;
    
    monitor.shutdown();
    
    return 0;
}
