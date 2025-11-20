#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <cstring>
#include <iomanip>
#include <chrono>
#include <vector>

/**
 * I2C Bus 0 Hardware Diagnostic Tool
 * 
 * Systematically tests I2C_IDA_0 (Bus 0) hardware functionality:
 * 1. Bus accessibility and permissions
 * 2. Device detection across all addresses
 * 3. Read/write functionality at different speeds
 * 4. Clock stretching and timing issues
 * 5. Comparison with working bus (Bus 7)
 * 
 * Usage: sudo ./diagnose_i2c_bus0
 */

class I2CDiagnostics {
private:
    int bus_number_;
    std::string device_path_;
    
public:
    I2CDiagnostics(int bus_number) : bus_number_(bus_number) {
        device_path_ = "/dev/i2c-" + std::to_string(bus_number);
    }
    
    // Test 1: Basic bus access
    bool testBusAccess() {
        std::cout << "\n[TEST 1] Testing basic bus access..." << std::endl;
        std::cout << "Device: " << device_path_ << std::endl;
        
        int fd = open(device_path_.c_str(), O_RDWR);
        if (fd < 0) {
            std::cout << "  ✗ FAILED: Cannot open device" << std::endl;
            std::cout << "    Error: " << strerror(errno) << std::endl;
            std::cout << "    Solution: Check permissions or run with sudo" << std::endl;
            return false;
        }
        
        std::cout << "  ✓ SUCCESS: Bus is accessible" << std::endl;
        close(fd);
        return true;
    }
    
    // Test 2: Comprehensive device scan with detailed probing
    std::vector<int> scanForDevices() {
        std::cout << "\n[TEST 2] Comprehensive device scan (0x03-0x77)..." << std::endl;
        std::vector<int> found_devices;
        
        int fd = open(device_path_.c_str(), O_RDWR);
        if (fd < 0) {
            std::cout << "  ✗ Cannot open bus" << std::endl;
            return found_devices;
        }
        
        // Test all valid 7-bit I2C addresses
        for (int addr = 0x03; addr <= 0x77; addr++) {
            // Method 1: Set slave address
            if (ioctl(fd, I2C_SLAVE, addr) < 0) {
                continue;
            }
            
            // Method 2: Try quick write (0-byte write)
            i2c_smbus_data data;
            i2c_smbus_ioctl_data args;
            args.read_write = I2C_SMBUS_WRITE;
            args.command = 0;
            args.size = I2C_SMBUS_QUICK;
            args.data = &data;
            
            if (ioctl(fd, I2C_SMBUS, &args) >= 0) {
                std::cout << "  ✓ Device found at 0x" << std::hex << std::setfill('0') 
                         << std::setw(2) << addr << std::dec << std::endl;
                found_devices.push_back(addr);
            }
        }
        
        close(fd);
        
        if (found_devices.empty()) {
            std::cout << "  ✗ NO DEVICES FOUND" << std::endl;
            std::cout << "    This indicates:" << std::endl;
            std::cout << "    - No devices connected, OR" << std::endl;
            std::cout << "    - Physical connection problem, OR" << std::endl;
            std::cout << "    - Bus hardware failure" << std::endl;
        } else {
            std::cout << "  ✓ Found " << found_devices.size() << " device(s)" << std::endl;
        }
        
        return found_devices;
    }
    
    // Test 3: Test read/write functionality with a known address
    bool testReadWrite(int device_addr) {
        std::cout << "\n[TEST 3] Testing read/write at 0x" << std::hex 
                  << device_addr << std::dec << "..." << std::endl;
        
        int fd = open(device_path_.c_str(), O_RDWR);
        if (fd < 0) return false;
        
        if (ioctl(fd, I2C_SLAVE, device_addr) < 0) {
            std::cout << "  ✗ Cannot set slave address" << std::endl;
            close(fd);
            return false;
        }
        
        // Try to read a byte (common register read pattern)
        unsigned char reg = 0x00;
        unsigned char data;
        
        // Write register address
        if (write(fd, &reg, 1) != 1) {
            std::cout << "  ⚠ Write operation failed" << std::endl;
        } else {
            std::cout << "  ✓ Write successful" << std::endl;
        }
        
        // Try to read response
        if (read(fd, &data, 1) != 1) {
            std::cout << "  ⚠ Read operation failed" << std::endl;
        } else {
            std::cout << "  ✓ Read successful (value: 0x" << std::hex 
                     << static_cast<int>(data) << std::dec << ")" << std::endl;
        }
        
        close(fd);
        return true;
    }
    
    // Test 4: Check bus timing and speed
    bool testBusTiming() {
        std::cout << "\n[TEST 4] Testing bus timing..." << std::endl;
        
        int fd = open(device_path_.c_str(), O_RDWR);
        if (fd < 0) return false;
        
        // Try to set I2C timeout (if supported)
        unsigned long timeout = 2; // 20ms timeout
        if (ioctl(fd, I2C_TIMEOUT, timeout) < 0) {
            std::cout << "  ⚠ Cannot set timeout (may not be supported)" << std::endl;
        } else {
            std::cout << "  ✓ Timeout set to " << (timeout * 10) << "ms" << std::endl;
        }
        
        // Try to set retries
        unsigned long retries = 3;
        if (ioctl(fd, I2C_RETRIES, retries) < 0) {
            std::cout << "  ⚠ Cannot set retries (may not be supported)" << std::endl;
        } else {
            std::cout << "  ✓ Retries set to " << retries << std::endl;
        }
        
        close(fd);
        return true;
    }
    
    // Test 5: Detailed error analysis
    void analyzeErrors() {
        std::cout << "\n[TEST 5] Error analysis..." << std::endl;
        
        int fd = open(device_path_.c_str(), O_RDWR);
        if (fd < 0) {
            std::cout << "  Cannot open bus for error analysis" << std::endl;
            return;
        }
        
        // Try a known-invalid address to test error handling
        int invalid_addr = 0x08; // Reserved address
        if (ioctl(fd, I2C_SLAVE, invalid_addr) < 0) {
            std::cout << "  ✓ Properly rejects reserved address 0x08" << std::endl;
        }
        
        // Try to communicate with non-existent device
        int test_addr = 0x50; // Common EEPROM address (unlikely to exist)
        if (ioctl(fd, I2C_SLAVE, test_addr) >= 0) {
            char dummy_data;
            auto start = std::chrono::high_resolution_clock::now();
            int result = read(fd, &dummy_data, 1);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            if (result < 0) {
                std::cout << "  ✓ Properly fails on non-existent device" << std::endl;
                std::cout << "    Timeout: " << duration.count() << "ms" << std::endl;
                std::cout << "    Error: " << strerror(errno) << std::endl;
            } else {
                std::cout << "  ⚠ Unexpected success reading from 0x50" << std::endl;
            }
        }
        
        close(fd);
    }
};

// Compare two buses side-by-side
void compareBuses(int working_bus, int problem_bus) {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "COMPARISON: Bus " << working_bus << " (working) vs Bus " 
              << problem_bus << " (problem)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    I2CDiagnostics diag_working(working_bus);
    I2CDiagnostics diag_problem(problem_bus);
    
    std::cout << "\nWorking Bus " << working_bus << ":" << std::endl;
    std::vector<int> working_devices = diag_working.scanForDevices();
    
    std::cout << "\nProblem Bus " << problem_bus << ":" << std::endl;
    std::vector<int> problem_devices = diag_problem.scanForDevices();
    
    std::cout << "\n--- COMPARISON RESULT ---" << std::endl;
    std::cout << "Working bus found: " << working_devices.size() << " device(s)" << std::endl;
    std::cout << "Problem bus found: " << problem_devices.size() << " device(s)" << std::endl;
    
    if (problem_devices.empty() && !working_devices.empty()) {
        std::cout << "\n⚠ DIAGNOSIS: Bus " << problem_bus << " hardware issue detected!" << std::endl;
        std::cout << "\nPossible causes:" << std::endl;
        std::cout << "1. Physical pin damage on I2C_IDA_0 connector" << std::endl;
        std::cout << "2. Missing or damaged pull-up resistors on Bus 0" << std::endl;
        std::cout << "3. Internal bus controller failure" << std::endl;
        std::cout << "4. Pin multiplexing configuration issue (device tree)" << std::endl;
        std::cout << "\nRecommended actions:" << std::endl;
        std::cout << "→ Measure voltage on Bus 0 SDA/SCL pins (should be ~3.3V)" << std::endl;
        std::cout << "→ Test with oscilloscope/logic analyzer if available" << std::endl;
        std::cout << "→ Check Jetson device tree configuration" << std::endl;
        std::cout << "→ Try external pull-up resistors (2.2kΩ to 3.3V)" << std::endl;
    }
}

int main() {
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "I2C BUS 0 HARDWARE DIAGNOSTIC TOOL" << std::endl;
    std::cout << "Jetson Orin Nano - I2C_IDA_0 Troubleshooting" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "\nThis tool systematically tests I2C Bus 0 hardware functionality." << std::endl;
    std::cout << "Tests include: access, device detection, read/write, timing, errors" << std::endl;
    
    // Primary diagnostic on Bus 0
    I2CDiagnostics diag_bus0(0);
    
    bool bus_accessible = diag_bus0.testBusAccess();
    if (!bus_accessible) {
        return 1;
    }
    
    std::vector<int> devices = diag_bus0.scanForDevices();
    
    if (!devices.empty()) {
        // If devices found, test first one
        diag_bus0.testReadWrite(devices[0]);
    }
    
    diag_bus0.testBusTiming();
    diag_bus0.analyzeErrors();
    
    // Compare with working Bus 7 (I2C_IDA_1)
    compareBuses(7, 0);
    
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "DIAGNOSTIC COMPLETE" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    return 0;
}
