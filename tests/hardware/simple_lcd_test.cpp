#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <chrono>
#include <thread>

class SimpleLCD {
private:
    int fd_;
    int addr_;
    uint8_t backlight_mask_;

    // LCD commands
    static const uint8_t LCD_CLEARDISPLAY = 0x01;
    static const uint8_t LCD_RETURNHOME = 0x02;
    static const uint8_t LCD_ENTRYMODESET = 0x04;
    static const uint8_t LCD_DISPLAYCONTROL = 0x08;
    static const uint8_t LCD_FUNCTIONSET = 0x20;
    static const uint8_t LCD_SETDDRAMADDR = 0x80;

    // Flags for display entry mode
    static const uint8_t LCD_ENTRYRIGHT = 0x00;
    static const uint8_t LCD_ENTRYLEFT = 0x02;

    // Flags for display on/off control
    static const uint8_t LCD_DISPLAYON = 0x04;
    static const uint8_t LCD_CURSOROFF = 0x00;
    static const uint8_t LCD_BLINKOFF = 0x00;

    // Flags for function set
    static const uint8_t LCD_4BITMODE = 0x00;
    static const uint8_t LCD_2LINE = 0x08;
    static const uint8_t LCD_5x8DOTS = 0x00;

    // PCF8574 pins
    static const uint8_t LCD_RS = 0x01; // P0
    static const uint8_t LCD_EN = 0x04; // P2
    static const uint8_t LCD_BACKLIGHT = 0x08; // P3

    void expanderWrite(uint8_t data) {
        uint8_t byte = data | backlight_mask_;
        if (write(fd_, &byte, 1) != 1) {
            std::cerr << "I2C write failed" << std::endl;
        }
    }

    void pulseEnable(uint8_t data) {
        expanderWrite(data | LCD_EN);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        expanderWrite(data & ~LCD_EN);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    void write4bits(uint8_t data) {
        expanderWrite(data);
        pulseEnable(data);
    }

    void send(uint8_t value, uint8_t mode) {
        uint8_t highnib = value & 0xF0;
        uint8_t lownib = (value << 4) & 0xF0;
        write4bits(highnib | mode);
        write4bits(lownib | mode);
    }

    void writeCommand(uint8_t cmd) {
        send(cmd, 0);
    }

    void writeChar(uint8_t ch) {
        send(ch, LCD_RS);
    }

public:
    SimpleLCD(const std::string& device, int address) 
        : fd_(-1), addr_(address), backlight_mask_(LCD_BACKLIGHT) {}

    ~SimpleLCD() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    bool init() {
        std::cout << "Opening I2C device..." << std::endl;
        fd_ = open("/dev/i2c-7", O_RDWR);
        if (fd_ < 0) {
            std::cerr << "Failed to open I2C device" << std::endl;
            return false;
        }

        if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
            std::cerr << "Failed to set I2C slave address" << std::endl;
            close(fd_);
            fd_ = -1;
            return false;
        }

        std::cout << "Starting LCD initialization sequence..." << std::endl;

        // Initial delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // First, turn on backlight
        std::cout << "Turning on backlight..." << std::endl;
        expanderWrite(LCD_BACKLIGHT);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Initialize display (HD44780 4-bit initialization sequence)
        std::cout << "Sending initialization commands..." << std::endl;
        
        // Three times 0x03 (8-bit interface)
        write4bits(0x30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        write4bits(0x30);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        write4bits(0x30);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Set to 4-bit interface
        write4bits(0x20);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Function set: 4-bit, 2 lines, 5x8 font
        writeCommand(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Display control: display on, cursor off, blink off
        writeCommand(LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Clear display
        writeCommand(LCD_CLEARDISPLAY);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Entry mode set: left to right
        writeCommand(LCD_ENTRYMODESET | LCD_ENTRYLEFT);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::cout << "LCD initialization complete!" << std::endl;
        return true;
    }

    void clear() {
        writeCommand(LCD_CLEARDISPLAY);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    void printText(const std::string& text) {
        for (char c : text) {
            writeChar(c);
            std::this_thread::sleep_for(std::chrono::microseconds(40));
        }
    }

    void setCursor(int row, int col) {
        uint8_t row_offsets[] = {0x00, 0x40};
        if (row >= 2) row = 1;
        if (col >= 16) col = 15;
        writeCommand(LCD_SETDDRAMADDR | (col + row_offsets[row]));
    }
};

int main() {
    std::cout << "=== Simple LCD Test ===" << std::endl;
    
    SimpleLCD lcd("/dev/i2c-7", 0x27);
    
    if (!lcd.init()) {
        std::cout << "LCD initialization failed!" << std::endl;
        return 1;
    }

    std::cout << "Testing LCD display..." << std::endl;
    
    // Test 1: Clear and write text
    std::cout << "Test 1: Writing 'Hello World'" << std::endl;
    lcd.clear();
    lcd.printText("Hello World");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 2: Two lines
    std::cout << "Test 2: Two lines" << std::endl;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printText("Line 1: Test");
    lcd.setCursor(1, 0);
    lcd.printText("Line 2: Success");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test 3: Drone specific
    std::cout << "Test 3: Drone messages" << std::endl;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.printText("Drone Control");
    lcd.setCursor(1, 0);
    lcd.printText("Ready!");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    lcd.clear();
    std::cout << "LCD test completed successfully!" << std::endl;
    return 0;
}