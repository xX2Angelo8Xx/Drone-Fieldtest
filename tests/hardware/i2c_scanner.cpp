#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <vector>

void scanBus(int bus) {
    std::string device = "/dev/i2c-" + std::to_string(bus);
    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        std::cout << "Bus " << bus << ": Could not open device" << std::endl;
        return;
    }

    std::cout << "Bus " << bus << ": ";
    bool found_any = false;
    
    for (int addr = 0x03; addr <= 0x77; addr++) {
        if (ioctl(fd, I2C_SLAVE, addr) >= 0) {
            // Try to read a byte
            char data;
            if (read(fd, &data, 1) >= 0 || write(fd, &data, 0) >= 0) {
                std::cout << "0x" << std::hex << addr << " ";
                found_any = true;
            }
        }
    }
    
    if (!found_any) {
        std::cout << "No devices found";
    }
    std::cout << std::endl;
    
    close(fd);
}

int main() {
    std::cout << "=== I2C Bus Scanner ===" << std::endl;
    std::cout << "Scanning all available I2C buses for devices..." << std::endl;
    
    std::vector<int> buses = {0, 1, 2, 4, 5, 7, 9};
    
    for (int bus : buses) {
        scanBus(bus);
    }
    
    return 0;
}