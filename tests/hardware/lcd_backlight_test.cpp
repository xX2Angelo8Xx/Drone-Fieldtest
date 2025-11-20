#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <chrono>
#include <thread>

int main() {
    std::cout << "=== LCD Backlight & Basic Test ===" << std::endl;
    
    int fd = open("/dev/i2c-7", O_RDWR);
    if (fd < 0) {
        std::cerr << "Failed to open I2C device" << std::endl;
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, 0x27) < 0) {
        std::cerr << "Failed to set I2C slave address" << std::endl;
        close(fd);
        return 1;
    }

    std::cout << "Testing backlight states..." << std::endl;

    // Test different backlight states
    uint8_t commands[] = {
        0x00,  // All off
        0x08,  // Backlight only
        0x0F,  // All pins high
        0x07,  // All except backlight
    };

    const char* names[] = {
        "All OFF",
        "Backlight ON only", 
        "All pins HIGH",
        "All except backlight"
    };

    for (int i = 0; i < 4; i++) {
        std::cout << "Testing: " << names[i] << " (0x" << std::hex << (int)commands[i] << ")" << std::endl;
        
        if (write(fd, &commands[i], 1) != 1) {
            std::cerr << "Write failed" << std::endl;
        }
        
        std::cout << "Look at the LCD now! Press Enter to continue..." << std::endl;
        std::cin.get();
    }

    // Try to send some actual LCD commands
    std::cout << "\nTesting LCD initialization with visible steps..." << std::endl;
    
    // Turn on backlight and set pins
    uint8_t backlight_on = 0x08;
    write(fd, &backlight_on, 1);
    std::cout << "1. Backlight should be ON. Press Enter..." << std::endl;
    std::cin.get();

    // Try to send clear screen command
    uint8_t clear_cmd[] = {0x08, 0x0C, 0x08, 0x08, 0x18, 0x1C, 0x18, 0x08}; // Clear display in 4-bit mode
    for (uint8_t cmd : clear_cmd) {
        write(fd, &cmd, 1);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    std::cout << "2. Sent clear command. See any change? Press Enter..." << std::endl;
    std::cin.get();

    // Turn off backlight
    uint8_t backlight_off = 0x00;
    write(fd, &backlight_off, 1);
    std::cout << "3. Backlight should be OFF now. Press Enter..." << std::endl;
    std::cin.get();

    close(fd);
    std::cout << "Test completed!" << std::endl;
    return 0;
}