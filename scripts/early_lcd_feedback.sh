#!/bin/bash
# Early LCD Feedback - Shows message IMMEDIATELY on boot
# This runs BEFORE anything else to give user instant feedback

I2C_DEV="/dev/i2c-7"
I2C_ADDR=0x27

# Only show message if LCD is available (non-blocking)
if [ -c "$I2C_DEV" ]; then
    # Wait longer for I2C bus to fully stabilize after boot
    sleep 1.0
    
    # Simple Python script for immediate LCD feedback with ROBUST timing
    python3 - <<EOF 2>/dev/null || true
import smbus2
import time

try:
    bus = smbus2.SMBus(7)
    addr = 0x27
    
    # LCD Commands
    LCD_BACKLIGHT = 0x08
    ENABLE = 0b00000100
    
    def lcd_byte(bits, mode):
        bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT
        bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT
        
        # Send high nibble with MUCH longer delays for stability
        bus.write_byte(addr, bits_high)
        time.sleep(0.003)  # 3ms - much more robust
        bus.write_byte(addr, (bits_high | ENABLE))
        time.sleep(0.003)
        bus.write_byte(addr, (bits_high & ~ENABLE))
        time.sleep(0.003)
        
        # Send low nibble
        bus.write_byte(addr, bits_low)
        time.sleep(0.003)
        bus.write_byte(addr, (bits_low | ENABLE))
        time.sleep(0.003)
        bus.write_byte(addr, (bits_low & ~ENABLE))
        time.sleep(0.003)
    
    # Proper HD44780 initialization sequence (datasheet-compliant)
    # Wait for LCD power-on (>40ms after power on) - be conservative
    time.sleep(0.1)  # 100ms to be very safe
    
    # Function set: 8-bit mode (repeat 3 times as per datasheet)
    lcd_byte(0x33, 0)
    time.sleep(0.01)  # 10ms between init commands
    lcd_byte(0x33, 0)
    time.sleep(0.01)
    lcd_byte(0x33, 0)
    time.sleep(0.01)
    
    # Function set: 4-bit mode
    lcd_byte(0x32, 0)
    time.sleep(0.01)
    
    # Function set: 4-bit, 2 lines, 5x8 font
    lcd_byte(0x28, 0)
    time.sleep(0.005)
    
    # Display control: Display on, cursor off, blink off
    lcd_byte(0x0C, 0)
    time.sleep(0.005)
    
    # Entry mode: Increment cursor, no shift
    lcd_byte(0x06, 0)
    time.sleep(0.005)
    
    # Clear display
    lcd_byte(0x01, 0)
    time.sleep(0.005)  # Clear needs extra time
    
    # Display message
    message_line1 = "Drone Starting  "
    message_line2 = "Please wait...  "
    
    # Line 1
    lcd_byte(0x80, 0)
    for char in message_line1:
        lcd_byte(ord(char), 1)
    
    # Line 2
    lcd_byte(0xC0, 0)
    for char in message_line2:
        lcd_byte(ord(char), 1)
    
    bus.close()
except:
    pass  # Silent fail if LCD not available
EOF
fi

exit 0
