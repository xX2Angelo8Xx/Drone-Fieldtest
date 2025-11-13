#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <chrono>
#include <thread>

class I2C_Tester {
private:
    int fd_;
    int addr_;

public:
    I2C_Tester(const std::string& device, int address) : fd_(-1), addr_(address) {
        fd_ = open(device.c_str(), O_RDWR);
        if (fd_ < 0) {
            std::cerr << "Failed to open " << device << std::endl;
            return;
        }
        
        if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
            std::cerr << "Failed to set I2C slave address 0x" << std::hex << addr_ << std::endl;
            close(fd_);
            fd_ = -1;
            return;
        }
        
        std::cout << "I2C device " << device << " opened successfully at address 0x" 
                  << std::hex << addr_ << std::dec << std::endl;
    }

    ~I2C_Tester() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    bool isValid() { return fd_ >= 0; }

    // Test basic I2C communication
    bool testCommunication() {
        std::cout << "\n=== Testing I2C Communication ===" << std::endl;
        
        // Test 1: Simple write
        std::cout << "Test 1: Simple write test..." << std::endl;
        uint8_t test_byte = 0x00;
        int result = write(fd_, &test_byte, 1);
        if (result != 1) {
            std::cout << "  FAILED: Cannot write to device (error: " << result << ")" << std::endl;
            return false;
        }
        std::cout << "  SUCCESS: Write operation successful" << std::endl;

        // Test 2: Try different values
        std::cout << "Test 2: Testing different byte values..." << std::endl;
        uint8_t test_values[] = {0x00, 0x08, 0xFF, 0x55, 0xAA};
        for (uint8_t val : test_values) {
            result = write(fd_, &val, 1);
            if (result != 1) {
                std::cout << "  FAILED: Cannot write 0x" << std::hex << (int)val << std::dec << std::endl;
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "  SUCCESS: All write operations successful" << std::endl;

        // Test 3: Read test (may fail on write-only devices like PCF8574)
        std::cout << "Test 3: Read test..." << std::endl;
        uint8_t read_byte;
        result = read(fd_, &read_byte, 1);
        if (result == 1) {
            std::cout << "  SUCCESS: Read 0x" << std::hex << (int)read_byte << std::dec << std::endl;
        } else {
            std::cout << "  INFO: Read failed (normal for PCF8574-based LCD)" << std::endl;
        }

        return true;
    }

    // Test LCD-specific initialization sequence
    void testLCDSequence() {
        std::cout << "\n=== Testing LCD Initialization Sequence ===" << std::endl;
        
        // HD44780 initialization in 4-bit mode via PCF8574
        std::cout << "Step 1: Initial delay and backlight on..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        uint8_t backlight_on = 0x08; // P3 = backlight
        write(fd_, &backlight_on, 1);
        std::cout << "  Backlight should be ON now. Check display!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "Step 2: HD44780 4-bit initialization..." << std::endl;
        
        // Function to send 4-bit command
        auto send4bit = [this](uint8_t data) {
            uint8_t backlight = 0x08;
            uint8_t enable = 0x04;
            
            // Send high nibble
            uint8_t byte = data | backlight;
            write(fd_, &byte, 1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            
            // Pulse enable
            byte |= enable;
            write(fd_, &byte, 1);
            std::this_thread::sleep_for(std::chrono::microseconds(1));
            
            byte &= ~enable;
            write(fd_, &byte, 1);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        };

        // Initialize sequence
        std::cout << "  Sending 0x30 three times..." << std::endl;
        send4bit(0x30); std::this_thread::sleep_for(std::chrono::milliseconds(5));
        send4bit(0x30); std::this_thread::sleep_for(std::chrono::milliseconds(5));
        send4bit(0x30); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        std::cout << "  Setting 4-bit mode (0x20)..." << std::endl;
        send4bit(0x20); std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::cout << "Step 3: Testing backlight off..." << std::endl;
        uint8_t backlight_off = 0x00;
        write(fd_, &backlight_off, 1);
        std::cout << "  Backlight should be OFF now. Check display!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "Step 4: Backlight on again..." << std::endl;
        write(fd_, &backlight_on, 1);
        std::cout << "  Backlight should be ON again. Check display!" << std::endl;
    }

    // Test I2C bus integrity
    void testBusIntegrity() {
        std::cout << "\n=== Testing I2C Bus Integrity ===" << std::endl;
        
        // Test rapid writes
        std::cout << "Test: Rapid write operations..." << std::endl;
        for (int i = 0; i < 100; i++) {
            uint8_t val = (i % 2) ? 0x08 : 0x00; // Alternate backlight
            if (write(fd_, &val, 1) != 1) {
                std::cout << "  FAILED at iteration " << i << std::endl;
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        std::cout << "  SUCCESS: 100 rapid writes completed" << std::endl;

        // Final state: backlight on
        uint8_t final = 0x08;
        write(fd_, &final, 1);
    }
};

int main() {
    std::cout << "=== I2C LCD Communication Tester ===" << std::endl;
    std::cout << "This will test the I2C communication with your LCD" << std::endl;
    std::cout << "Watch the LCD display for backlight changes!" << std::endl;
    std::cout << "\nPress Enter to start..." << std::endl;
    std::cin.get();

    I2C_Tester tester("/dev/i2c-7", 0x27);
    
    if (!tester.isValid()) {
        std::cout << "Failed to initialize I2C tester!" << std::endl;
        return 1;
    }

    // Run tests
    if (!tester.testCommunication()) {
        std::cout << "Basic communication test failed!" << std::endl;
        return 1;
    }

    std::cout << "\nPress Enter to continue with LCD-specific tests..." << std::endl;
    std::cin.get();

    tester.testLCDSequence();

    std::cout << "\nPress Enter to continue with bus integrity test..." << std::endl;
    std::cin.get();

    tester.testBusIntegrity();

    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Did you see the backlight turning on/off during the tests? (y/n): ";
    
    char response;
    std::cin >> response;
    
    if (response == 'y' || response == 'Y') {
        std::cout << "Great! I2C communication is working." << std::endl;
        std::cout << "The problem is likely in the LCD initialization sequence." << std::endl;
    } else {
        std::cout << "I2C communication issue detected." << std::endl;
        std::cout << "Check SDA/SCL connections and pull-up resistors." << std::endl;
    }

    return 0;
}