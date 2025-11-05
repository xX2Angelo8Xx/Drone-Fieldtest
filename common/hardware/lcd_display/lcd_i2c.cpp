#include "lcd_i2c.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

static const uint8_t LCD_RS = 0x01; // P0
static const uint8_t LCD_RW = 0x02; // P1 (unused)
static const uint8_t LCD_EN = 0x04; // P2
static const uint8_t LCD_BACKLIGHT = 0x08; // P3

LCD_I2C::LCD_I2C(const std::string &i2c_dev, int addr, bool backlight)
    : fd_(-1), dev_(i2c_dev), addr_(addr), backlight_mask_(backlight ? LCD_BACKLIGHT : 0x00) {}

LCD_I2C::~LCD_I2C() {
    if (fd_ >= 0) {
        try {
            clear();
        } catch (...) {}
        close(fd_);
        fd_ = -1;
    }
}

bool LCD_I2C::init() {
    fd_ = open(dev_.c_str(), O_RDWR);
    if (fd_ < 0) {
        std::cerr << "LCD_I2C: cannot open " << dev_ << " - " << strerror(errno) << "\n";
        return false;
    }
    if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
        std::cerr << "LCD_I2C: ioctl I2C_SLAVE failed - " << strerror(errno) << "\n";
        close(fd_); fd_ = -1;
        return false;
    }

    // Initial sequence matching the successful Python implementation
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // 4-bit initialization sequence
    write4bits(0x30);  // 0x30 = 0011 0000
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    write4bits(0x30);
    std::this_thread::sleep_for(std::chrono::microseconds(150));
    write4bits(0x30);
    write4bits(0x20);  // 0x20 = 0010 0000 (4-bit mode)

    // Function set: 4-bit, 2 line, 5x8 dots
    writeCommand(0x28);  
    // Display off
    writeCommand(0x08);
    // Clear display
    writeCommand(0x01);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    // Entry mode set
    writeCommand(0x06);
    // Display on, cursor off, blink off
    writeCommand(0x0C);

    std::cout << "LCD_I2C: Initialized successfully on " << dev_ << " at address 0x" 
              << std::hex << addr_ << std::dec << std::endl;
    return true;
}

void LCD_I2C::expanderWrite(uint8_t data) {
    uint8_t buf = data | backlight_mask_;
    if (fd_ >= 0) {
        // Schreibe ohne Fehlermeldung - wenn fd_ gültig ist,
        // wurde die Verbindung bereits initialisiert
        write(fd_, &buf, 1);
        // Keine Fehlermeldung bei Schreibfehlern
    }
}

void LCD_I2C::pulseEnable(uint8_t data) {
    expanderWrite(data | LCD_EN);
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    expanderWrite(data & ~LCD_EN);
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
}

void LCD_I2C::write4bits(uint8_t data) {
    expanderWrite(data);
    pulseEnable(data);
}

void LCD_I2C::send(uint8_t value, uint8_t mode) {
    uint8_t high = mode | (value & 0xF0) | backlight_mask_;
    uint8_t low = mode | ((value << 4) & 0xF0) | backlight_mask_;
    
    expanderWrite(high);
    pulseEnable(high);
    
    expanderWrite(low);
    pulseEnable(low);
}

void LCD_I2C::writeCommand(uint8_t cmd) {
    send(cmd, 0x00);
}

void LCD_I2C::writeChar(uint8_t ch) {
    send(ch, LCD_RS);
}

void LCD_I2C::clear() {
    writeCommand(0x01);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

void LCD_I2C::printMessage(const std::string &msg) {
    // Wenn fd_ ungültig ist, wurde die Verbindung nicht hergestellt - nichts tun
    if (fd_ < 0) return;
    
    std::string line1, line2;
    auto pos = msg.find('\n');
    if (pos != std::string::npos) {
        line1 = msg.substr(0, pos);
        line2 = msg.substr(pos + 1);
    } else {
        if (msg.size() <= 16) {
            line1 = msg;
        } else {
            line1 = msg.substr(0, 16);
            line2 = msg.substr(16, 16);
        }
    }
    // ensure lengths <= 16
    if (line1.size() > 16) line1.resize(16);
    if (line2.size() > 16) line2.resize(16);

    // pad to avoid leftover chars
    line1.append(16 - line1.size(), ' ');
    line2.append(16 - line2.size(), ' ');

    // set DDRAM address to line1 start and write
    writeCommand(0x80); // line 1
    for (char c : line1) writeChar(static_cast<uint8_t>(c));

    writeCommand(0xC0); // line 2
    for (char c : line2) writeChar(static_cast<uint8_t>(c));
}