#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

int main() {
    std::cout << "Minimal LCD Test - Just backlight toggle" << std::endl;
    
    int fd = open("/dev/i2c-7", O_RDWR);
    if (fd < 0) {
        std::cerr << "Cannot open I2C device" << std::endl;
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, 0x27) < 0) {
        std::cerr << "Cannot set I2C address" << std::endl;
        close(fd);
        return 1;
    }

    std::cout << "Turning backlight ON..." << std::endl;
    uint8_t on = 0x08;
    write(fd, &on, 1);
    
    std::cout << "Check LCD now - is backlight visible? (y/n): ";
    char response;
    std::cin >> response;
    
    if (response == 'y' || response == 'Y') {
        std::cout << "Great! Backlight works. Problem is initialization or contrast." << std::endl;
    } else {
        std::cout << "Backlight not working. Check hardware connections." << std::endl;
    }
    
    std::cout << "Turning backlight OFF..." << std::endl;
    uint8_t off = 0x00;
    write(fd, &off, 1);
    
    close(fd);
    return 0;
}