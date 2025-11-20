#!/bin/bash
# Visual Wiring Guide for LCD + INA219 on Bus 7
# Shows how to connect both devices to same I2C bus

cat << 'EOF'
================================================================================
WIRING GUIDE: LCD + INA219 on Bus 7 (PARALLEL CONNECTION)
================================================================================

IMPORTANT: Both devices share the SAME 4 pins - this is normal for I2C!
No multiplexer needed - different addresses prevent conflicts.

╔══════════════════════════════════════════════════════════════════════════╗
║                    Jetson Orin Nano 40-Pin Header                        ║
║                                                                           ║
║  Pin 1  [3.3V] ════════════╦═══════════════════════════════════════╗    ║
║  Pin 2  [ 5V ]             ║                                       ║    ║
║  Pin 3  [SDA1] ═══════════╬╦══════════════════════════════════╗   ║    ║
║  Pin 4  [ 5V ]            ║║                                  ║   ║    ║
║  Pin 5  [SCL1] ══════════╬╬╦═════════════════════════════╗   ║   ║    ║
║  Pin 6  [ GND] ═════════╬╬╬╦════════════════════════╗   ║   ║   ║    ║
║  ...                    ║║║║                        ║   ║   ║   ║    ║
╚═════════════════════════║║║║════════════════════════║═══║═══║═══║════╝
                          ║║║║                        ║   ║   ║   ║
                    ┌─────┘│││                        │   │   │   │
                    │  ┌───┘││                        │   │   │   │
                    │  │ ┌──┘│                        │   │   │   │
                    │  │ │ ┌─┘                        │   │   │   │
                    V  V V V                          V   V   V   V
              ┌─────────────────┐              ┌─────────────────┐
              │  LCD 16x2 (I2C) │              │   INA219 Module │
              │  Address: 0x27  │              │  Address: 0x40  │
              ├─────────────────┤              ├─────────────────┤
              │ GND  [●]───────────────────────┼────[●] GND      │
              │ VCC  [●]───────────────────────┼────[●] VCC (V+) │
              │ SDA  [●]───────────────────────┼────[●] SDA      │
              │ SCL  [●]───────────────────────┼────[●] SCL      │
              └─────────────────┘              └─────────────────┘
                                                │ VIN+ [●]─→ Battery +
                                                │ VIN- [●]─→ Battery -

PARALLEL CONNECTION SCHEMATIC:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Pin 1 (3.3V) ─┬──→ LCD VCC
              └──→ INA219 VCC

Pin 3 (SDA)  ─┬──→ LCD SDA
              └──→ INA219 SDA

Pin 5 (SCL)  ─┬──→ LCD SCL
              └──→ INA219 SCL

Pin 6 (GND)  ─┬──→ LCD GND
              └──→ INA219 GND

BREADBOARD WIRING (If using):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

   Jetson                  Breadboard
   ======                  ==========
   Pin 3 ──────→ [+] Rail ──┬─→ LCD SDA
   (SDA)                     └─→ INA219 SDA

   Pin 5 ──────→ [+] Rail ──┬─→ LCD SCL
   (SCL)                     └─→ INA219 SCL

   Pin 1 ──────→ [+] Rail ──┬─→ LCD VCC
   (3.3V)                    └─→ INA219 VCC

   Pin 6 ──────→ [-] Rail ──┬─→ LCD GND
   (GND)                     └─→ INA219 GND

NOTES:
━━━━━━
• Pull-up resistors: Already on both modules (2.2kΩ - 4.7kΩ typical)
• Wire length: Keep under 30cm for reliable 100kHz operation
• Power consumption: LCD ~20mA, INA219 ~1mA = Total 21mA @ 3.3V
• Address conflict: IMPOSSIBLE (0x27 ≠ 0x40)

COLOR CODE SUGGESTION:
━━━━━━━━━━━━━━━━━━━━━━
Red    → 3.3V (VCC)
Black  → GND
Yellow → SDA (Data)
Green  → SCL (Clock)

================================================================================
TESTING THE CONNECTION
================================================================================

1. Connect both devices as shown above

2. Scan Bus 7 to verify both appear:
   $ sudo i2cdetect -y 7
   
   Expected output:
        0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
   20: -- -- -- -- -- -- -- 27 -- -- -- -- -- -- -- --
   40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
   
   ✓ 27 = LCD found!
   ✓ 40 = INA219 found!

3. Test LCD:
   $ sudo ./simple_lcd_test
   
   Should display "Hello World" on LCD

4. Test INA219:
   $ python3 test_ina219.py
   
   Should show voltage/current readings

5. Test both simultaneously in code:
   See example in: apps/drone_web_controller/

================================================================================
TROUBLESHOOTING
================================================================================

Problem: "Nothing detected on bus"
Solution:
  • Check wiring (especially SDA/SCL not swapped)
  • Verify 3.3V on VCC pins with multimeter
  • Try each device individually first

Problem: "Only one device appears"
Solution:
  • Check if both have power (VCC connected?)
  • Try different I2C address on LCD (0x3F vs 0x27)
  • Check for loose connections

Problem: "Bus hangs / lockup"
Solution:
  • Power cycle both devices
  • Check for short circuits
  • Verify pull-up resistors present (should be on modules)

Problem: "LCD works but INA219 doesn't"
Solution:
  • INA219 needs VIN+/VIN- connected to measure voltage
  • Try: sudo i2cget -y 7 0x40 0x00
  • Check INA219 is at 0x40 (no jumpers = 0x40)

================================================================================
SOFTWARE EXAMPLE
================================================================================

C++ Code (drone_web_controller style):
─────────────────────────────────────────

#include "common/hardware/i2c_lcd/lcd.h"
#include "ina219.h"  // Use appropriate library

int main() {
    // Both on Bus 7, different addresses
    I2C_LCD lcd(7, 0x27);           // LCD @ 0x27
    INA219 power_monitor(7, 0x40);  // INA219 @ 0x40
    
    // Initialize
    lcd.init();
    lcd.backlight();
    power_monitor.begin();
    power_monitor.setCalibration_16V_400mA();
    
    // Use both simultaneously - NO CONFLICT!
    while (true) {
        float voltage = power_monitor.getBusVoltage_V();
        float current = power_monitor.getCurrent_mA();
        
        lcd.setCursor(0, 0);
        lcd.print("Volt: ");
        lcd.print(voltage, 2);
        lcd.print("V");
        
        lcd.setCursor(0, 1);
        lcd.print("Curr: ");
        lcd.print(current, 1);
        lcd.print("mA");
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}

Python Code:
────────────

from smbus2 import SMBus
from ina219 import INA219
from time import sleep

# LCD I2C library (e.g., RPLCD)
from RPLCD.i2c import CharLCD

# Initialize both on Bus 7
lcd = CharLCD('PCF8574', 0x27, bus=7)
ina = INA219(shunt_ohms=0.1, busnum=7, address=0x40)
ina.configure()

# Use both
while True:
    voltage = ina.voltage()
    current = ina.current()
    
    lcd.clear()
    lcd.write_string(f'Volt: {voltage:.2f}V')
    lcd.crlf()
    lcd.write_string(f'Curr: {current:.1f}mA')
    
    sleep(1)

================================================================================
KEY TAKEAWAYS
================================================================================

✓ NO MULTIPLEXER NEEDED - I2C supports multiple devices natively!
✓ Both devices share SAME 4 pins (3.3V, GND, SDA, SCL)
✓ Different addresses (0x27 vs 0x40) prevent conflicts
✓ This is STANDARD I2C operation - used worldwide in millions of devices
✓ Production-ready solution - proven in drone_web_controller field tests

Questions? Check docs/I2C_MULTI_DEVICE_NO_MULTIPLEXER.md

================================================================================
EOF
