LCD Hardware Failure — Field Test Notes

Status: Hardware failure detected; replacement ordered.

Summary
-------
During routine testing the HD44780/PCF8574 I2C LCD (expected on `/dev/i2c-7`, address `0x27`) failed to display characters. Software initialization logged success, but the display itself remained dark.

Actions performed
-----------------
- Confirmed software initialization logs: "LCD_I2C: Initialized successfully on /dev/i2c-7 at address 0x27".
- Ran `i2c_scanner` and a custom scanner: device detected at bus 7, address 0x27.
- Used `i2cget` to directly read bytes from the address; bus responded (non-fatal reads).
- Ran `tools/lcd_display_tool "Autostart" "Started!"` — utility logged success but no visible characters.
- Executed `i2c_lcd_tester` interactive tests: backlight toggles, init sequences completed according to logs.
- Verified power and wiring; voltage present but display still dark.

Diagnosis
----------
- Likely hardware fault in the LCD module (contrast pot or character module failed), or loose internal connection on the display.
- Software driver and I2C bus are working (device responds to low-level probes and initializations).

Next steps / Replacement procedure
----------------------------------
1. Replace the LCD module with the same form-factor PCF8574-backed HD44780 I2C display.
2. Verify wiring: SDA/SCL to the Jetson's I2C pins for bus 7, and 5V (or 3.3V if using level-shifting module) and GND.
3. Adjust contrast potentiometer slowly while running `tools/lcd_display_tool` to check for visible characters.
4. If characters appear, run full `i2c_lcd_tester` and re-enable LCD in autostart (if desired).
5. If still failing, check I2C pull-ups and the PCF8574 module's power.

Useful commands
---------------
# Check the i2c bus for devices (replace 7 with other bus numbers as needed)
i2cdetect -y 7

# Run the interactive tester (after building the tools)
./build/tools/i2c_lcd_tester

# Quick two-line write from the tools
./tools/lcd_display_tool "Autostart" "Started!"

Notes
-----
- The repository already contains robust software support for the LCD: low-level timing fixes, backlight control, and test utilities. This file documents the hardware failure and the steps to validate a replacement.
- Keep this file updated with results after replacement verification.
