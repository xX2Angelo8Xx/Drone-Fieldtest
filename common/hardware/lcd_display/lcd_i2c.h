// cpp
#pragma once
#include <string>

class LCD_I2C {
public:
    LCD_I2C(const std::string &i2c_dev = "/dev/i2c-1", int addr = 0x27, bool backlight = true);
    ~LCD_I2C();

    // Öffnet das I2C-Device und initialisiert das Display. true=OK.
    bool init();

    // Löscht Display.
    void clear();

    // Zeigt bis zu 32 Zeichen an. Entweder ein String ohne Zeilenumbruch
    // (erste 16 -> Zeile1, nächste 16 -> Zeile2) oder mit '\n' als Trenner.
    void printMessage(const std::string &msg);

private:
    int fd_;
    std::string dev_;
    int addr_;
    uint8_t backlight_mask_;

    void expanderWrite(uint8_t data);
    void pulseEnable(uint8_t data);
    void write4bits(uint8_t data);
    void send(uint8_t value, uint8_t mode);
    void writeCommand(uint8_t cmd);
    void writeChar(uint8_t ch);
};