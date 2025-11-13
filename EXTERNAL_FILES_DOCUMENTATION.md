# External Files Documentation

This document provides complete information about all files that exist outside the main project directory but are essential for the drone system operation.

## System Files Overview

The drone system creates and depends on several files outside the main project directory:

```
/home/angelo/Desktop/Autostart           # Visual autostart control
/etc/sudoers.d/drone-controller          # Passwordless sudo permissions
/home/angelo/.bashrc                     # Command aliases (appended)
/etc/systemd/system/drone-recorder.service  # SystemD service file
```

## File Details

### 1. Desktop Autostart Control File

**Location**: `/home/angelo/Desktop/Autostart`

**Purpose**: Visual desktop file that controls whether the drone system starts automatically on boot.

**Current Content**:
```
=== DRONE AUTOSTART CONTROL ===

SYSTEM STATUS: AUTOSTART ENABLED ✅

The presence of this file enables automatic drone startup.
To DISABLE autostart: rename this file to "Autostart_DISABLED"
To ENABLE autostart: rename back to "Autostart"

CURRENT BEHAVIOR:
- Boot → SystemD starts drone-recorder.service
- Service checks for this file's existence
- If present: Starts WiFi hotspot + web interface
- If missing: System boots normally without drone functionality

WEB ACCESS:
1. Connect to WiFi: "DroneController" (password: drone123)
2. Open browser: http://192.168.4.1:8080
3. Use web interface for recording control

MANUAL CONTROL:
- Start: sudo systemctl start drone-recorder
- Stop: sudo systemctl stop drone-recorder  
- Status: sudo systemctl status drone-recorder
- Logs: sudo journalctl -u drone-recorder -f

TERMINAL SHORTCUTS:
- drone      # Quick start command
- drone-status  # Check system status

STOP COMMANDS (if needed):
sudo systemctl stop drone-recorder
sudo pkill -f drone_web_controller
sudo pkill -f hostapd
sudo pkill -f dnsmasq
sudo systemctl restart NetworkManager
```

**How It Works**:
- SystemD service checks if this file exists
- File present = Autostart enabled
- File missing/renamed = Autostart disabled
- Provides visual feedback on desktop
- Contains all necessary command documentation

### 2. Sudo Permissions File

**Location**: `/etc/sudoers.d/drone-controller`

**Purpose**: Allows the drone system to execute WiFi management commands without password prompts.

**Content**:
```
# Drone controller WiFi management permissions
angelo ALL=(ALL) NOPASSWD: /usr/sbin/hostapd
angelo ALL=(ALL) NOPASSWD: /usr/sbin/dnsmasq
angelo ALL=(ALL) NOPASSWD: /sbin/ip
angelo ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart NetworkManager
angelo ALL=(ALL) NOPASSWD: /usr/bin/systemctl stop NetworkManager
angelo ALL=(ALL) NOPASSWD: /usr/bin/systemctl start NetworkManager
angelo ALL=(ALL) NOPASSWD: /usr/bin/pkill -f hostapd
angelo ALL=(ALL) NOPASSWD: /usr/bin/pkill -f dnsmasq
```

**Critical for**:
- WiFi hotspot creation (hostapd)
- DHCP server functionality (dnsmasq)
- Network interface management (ip commands)
- NetworkManager conflict resolution
- SystemD service operation without interactive passwords

### 3. Bash Aliases

**Location**: `/home/angelo/.bashrc` (appended lines)

**Purpose**: Provides convenient terminal commands for drone system control.

**Appended Content**:
```bash
# Drone system quick commands
alias drone='cd ~/Projects/Drone-Fieldtest && ./drone_start.sh'
alias drone-status='~/Projects/Drone-Fieldtest/scripts/autostart_control.sh'
```

**Commands Provided**:
- `drone`: Quick start command from any directory
- `drone-status`: Check current autostart status and system state

### 4. SystemD Service File

**Location**: `/etc/systemd/system/drone-recorder.service`

**Purpose**: Enables automatic startup of drone system on boot.

**Content**:
```ini
[Unit]
Description=Drone Web Controller Service
After=network.target
Wants=network.target

[Service]
Type=forking
User=angelo
Group=angelo
WorkingDirectory=/home/angelo/Projects/Drone-Fieldtest
ExecStart=/home/angelo/Projects/Drone-Fieldtest/apps/drone_web_controller/autostart.sh
Environment=DISPLAY=:0
Environment=HOME=/home/angelo
Environment=USER=angelo
TimeoutStartSec=60
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**Key Features**:
- Runs as user `angelo` (not root)
- Sets proper environment variables
- Automatic restart on failure
- 60-second startup timeout for hardware initialization
- Uses forking type for proper daemon behavior

## File Dependencies

### Boot Sequence
1. **SystemD** loads `/etc/systemd/system/drone-recorder.service`
2. **Service** executes `~/Projects/Drone-Fieldtest/apps/drone_web_controller/autostart.sh`
3. **Script** checks for `/home/angelo/Desktop/Autostart` file existence
4. **If present**: Starts WiFi hotspot using permissions from `/etc/sudoers.d/drone-controller`
5. **Terminal** commands use aliases from `~/.bashrc`

### Critical Dependencies
- **WiFi Operations**: Require `/etc/sudoers.d/drone-controller` for passwordless execution
- **Autostart Control**: Depends on `/home/angelo/Desktop/Autostart` file presence
- **Service Management**: Uses `/etc/systemd/system/drone-recorder.service` for boot integration
- **User Convenience**: Terminal aliases in `~/.bashrc` for quick access

## Maintenance

### Creating Files
If these files are missing, they can be recreated:

```bash
# Autostart control file
touch ~/Desktop/Autostart

# Sudo permissions (requires manual creation)
sudo visudo -f /etc/sudoers.d/drone-controller

# SystemD service
sudo ./scripts/install_service.sh

# Bash aliases (automatic via scripts)
# Will be added when using drone system commands
```

### Backup Important Files
```bash
# Create backup of critical system files
sudo cp /etc/sudoers.d/drone-controller ~/Projects/Drone-Fieldtest/backup/
sudo cp /etc/systemd/system/drone-recorder.service ~/Projects/Drone-Fieldtest/backup/
cp ~/Desktop/Autostart ~/Projects/Drone-Fieldtest/backup/
```

### Verification Commands
```bash
# Check if all external files exist
ls -la ~/Desktop/Autostart
sudo ls -la /etc/sudoers.d/drone-controller
sudo ls -la /etc/systemd/system/drone-recorder.service
grep "drone" ~/.bashrc

# Test sudo permissions
sudo -n hostapd --help >/dev/null 2>&1 && echo "✅ hostapd sudo OK" || echo "❌ hostapd sudo FAIL"
sudo -n dnsmasq --help >/dev/null 2>&1 && echo "✅ dnsmasq sudo OK" || echo "❌ dnsmasq sudo FAIL"

# Check service status
sudo systemctl status drone-recorder
```

## Troubleshooting

### Common Issues

**Autostart Not Working**
- Check if `/home/angelo/Desktop/Autostart` file exists
- Verify service is enabled: `sudo systemctl is-enabled drone-recorder`
- Check logs: `sudo journalctl -u drone-recorder -f`

**Permission Denied Errors**
- Verify `/etc/sudoers.d/drone-controller` exists and has correct content
- Test with: `sudo -n hostapd --help`

**Commands Not Found**
- Check if aliases exist in `~/.bashrc`
- Reload bash: `source ~/.bashrc`

**Service Won't Start**
- Verify service file: `sudo systemctl cat drone-recorder`
- Check working directory exists
- Ensure user permissions are correct

## Integration with Project

These external files are tightly integrated with the main project:

- **autostart.sh** script checks Desktop file
- **WiFi management** requires sudo permissions
- **SystemD service** enables boot integration
- **Command aliases** provide user convenience

All external files are documented here to ensure complete system restoration if needed.

## LCD Hardware Note

The project expects a PCF8574-backed HD44780 I2C LCD on `/dev/i2c-7` (address `0x27`). During the most recent field test the physical LCD failed (dark display despite successful software initialization). The repository contains software and test utilities to validate a replacement LCD once it's installed:

- `tools/lcd_display_tool` — CLI to write two-line messages from scripts
- `build/tools/i2c_lcd_tester` / `i2c_lcd_tester` — interactive tester (backlight, init sequences)
- `build/tools/simple_lcd_test` — quick initialization test

Replacement steps:
1. Install the replacement LCD and verify correct wiring and contrast potentiometer.
2. Run the `i2c_scanner` to confirm the device shows up on the expected bus and address.
3. Run `i2c_lcd_tester` and `simple_lcd_test` to validate backlight and display initialization.
4. Use `tools/lcd_display_tool "Autostart" "Started!"` to verify messages from autostart scripts.

If the LCD still does not show text after these steps, check wiring/power, the display contrast pot, and run `dmesg` for any I2C errors.