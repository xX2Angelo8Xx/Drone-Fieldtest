# 🛰️ Jetson Network Safety & Recovery Policy
**Version:** 1.0  
**Last Updated:** 2025-11-12  
**Status:** MANDATORY - All network code must comply

---

## 1. Purpose

This document defines **mandatory rules and technical guidelines** for safe network operations on Jetson systems.  
It prevents system failures caused by:
- Faulty Access Point configurations
- Incorrect DNS handling
- Uncontrolled termination of system-critical processes

**Goal:** The system must maintain stable LAN or WLAN connectivity at all times, even after network experiments (Access Point, Hotspot, P2P).

---

## 2. Root Cause of Previous Failures

Previous failures occurred because critical network services were aggressively terminated or overwritten when starting Access Point mode:

### ❌ What Went Wrong:
1. **Process Termination:** NetworkManager, wpa_supplicant, dnsmasq, systemd-resolved killed via `pkill`
2. **DNS Destruction:** `/etc/resolv.conf` deleted or overwritten with empty content
3. **Interface Hijacking:** Primary WLAN interface (`wlP1p1s0`) forced into AP mode instead of using virtual interface
4. **Autoconnect Disabled:** Home WiFi connections set to `autoconnect=no` and never restored

### 💥 Result:
- Complete loss of internet connectivity
- DNS resolution broken (no nameservers)
- NetworkManager unable to manage interfaces
- System requires manual intervention to restore network

---

## 3. Forbidden Actions

### ❌ NEVER Kill System Network Processes
```bash
# FORBIDDEN - These commands are BANNED:
pkill NetworkManager
pkill wpa_supplicant
pkill dnsmasq
pkill systemd-resolved
systemctl stop NetworkManager
systemctl stop wpa_supplicant
systemctl stop systemd-resolved
```

### ❌ NEVER Modify System DNS Configuration
```bash
# FORBIDDEN - NEVER touch these files directly:
rm /etc/resolv.conf
echo "" > /etc/resolv.conf
ln -sf /dev/null /etc/resolv.conf
```

### ❌ NEVER Disable NetworkManager
```bash
# FORBIDDEN - NetworkManager must always run:
systemctl disable NetworkManager
systemctl mask NetworkManager
```

### ❌ NEVER Force Interface Mode Without Cleanup
```bash
# FORBIDDEN - Leaves interface in broken state:
iw dev wlP1p1s0 set type __ap  # Without proper teardown
ip addr add 192.168.x.x/24 dev wlP1p1s0  # Manual IP without NetworkManager
```

---

## 4. Mandatory Safety Rules

### ✅ Rule 1: Always Use NetworkManager APIs
```bash
# CORRECT - Use NetworkManager's native hotspot:
nmcli dev wifi hotspot ifname wlP1p1s0 con-name Hotspot ssid MyAP password pass123

# CORRECT - Query connection status:
nmcli con show --active
nmcli dev status
```

### ✅ Rule 2: Preserve DNS Configuration
```bash
# CORRECT - Let NetworkManager handle DNS:
# - NetworkManager manages /etc/resolv.conf via symlink
# - systemd-resolved provides DNS when needed
# - NO manual writes to /etc/resolv.conf

# If DNS must be configured, use:
nmcli con modify <connection> ipv4.dns "8.8.8.8 8.8.4.4"
```

### ✅ Rule 3: Always Restore Autoconnect
```bash
# BEFORE disabling autoconnect, store state:
WIFI_CONNECTIONS=$(nmcli -t -f NAME,TYPE con show | grep ':802-11-wireless$' | cut -d: -f1)

# Disable autoconnect temporarily:
nmcli con modify "$CONNECTION" connection.autoconnect no

# MANDATORY: Restore after AP shutdown:
nmcli con modify "$CONNECTION" connection.autoconnect yes
```

### ✅ Rule 4: Implement Graceful Fallback
```bash
# CORRECT - Test connectivity before committing changes:
if ! nmcli dev wifi hotspot ...; then
    echo "Hotspot creation failed, restoring previous state"
    # Automatic rollback here
fi
```

### ✅ Rule 5: Use Connection Profiles, Not Raw Commands
```bash
# CORRECT - Create persistent profile:
nmcli con add type wifi ifname wlP1p1s0 con-name DroneAP mode ap ssid DroneController
nmcli con modify DroneAP 802-11-wireless-security.key-mgmt wpa-psk
nmcli con modify DroneAP 802-11-wireless-security.psk drone123
nmcli con modify DroneAP ipv4.method shared

# CORRECT - Activate/deactivate:
nmcli con up DroneAP
nmcli con down DroneAP  # Automatic cleanup!
```

---

## 5. Safe Implementation Pattern

### Complete Safe Hotspot Lifecycle:

```python
#!/usr/bin/env python3
"""
Safe WiFi Hotspot Manager for Jetson
Complies with Network Safety Policy v1.0
"""

import subprocess
import sys
import time

class SafeHotspotManager:
    def __init__(self, interface="wlP1p1s0", con_name="DroneController"):
        self.interface = interface
        self.con_name = con_name
        self.wifi_connections_state = {}
        
    def backup_wifi_state(self):
        """Backup autoconnect state of all WiFi connections"""
        result = subprocess.run(
            ["nmcli", "-t", "-f", "NAME,TYPE,AUTOCONNECT", "con", "show"],
            capture_output=True, text=True
        )
        for line in result.stdout.splitlines():
            name, conn_type, autoconnect = line.split(':')
            if conn_type == "802-11-wireless" and name != self.con_name:
                self.wifi_connections_state[name] = (autoconnect == "yes")
        print(f"[BACKUP] Saved state for {len(self.wifi_connections_state)} WiFi connections")
    
    def disable_other_wifi_autoconnect(self):
        """Temporarily disable autoconnect for other WiFi networks"""
        for conn_name in self.wifi_connections_state.keys():
            subprocess.run(
                ["nmcli", "con", "modify", conn_name, "connection.autoconnect", "no"],
                capture_output=True
            )
        print("[SAFETY] Other WiFi autoconnect disabled")
    
    def restore_wifi_state(self):
        """Restore original autoconnect state"""
        for conn_name, autoconnect in self.wifi_connections_state.items():
            state = "yes" if autoconnect else "no"
            subprocess.run(
                ["nmcli", "con", "modify", conn_name, "connection.autoconnect", state],
                capture_output=True
            )
        print(f"[RESTORE] Restored state for {len(self.wifi_connections_state)} connections")
    
    def create_hotspot(self):
        """Safely create hotspot using NetworkManager"""
        try:
            # Step 1: Backup state
            self.backup_wifi_state()
            
            # Step 2: Check if profile exists
            result = subprocess.run(
                ["nmcli", "con", "show", self.con_name],
                capture_output=True
            )
            
            if result.returncode == 0:
                print(f"[INFO] Connection '{self.con_name}' exists, activating...")
                result = subprocess.run(
                    ["nmcli", "con", "up", self.con_name],
                    capture_output=True, text=True
                )
            else:
                print(f"[INFO] Creating new connection '{self.con_name}'...")
                # Create permanent profile
                subprocess.run([
                    "nmcli", "con", "add",
                    "type", "wifi",
                    "ifname", self.interface,
                    "con-name", self.con_name,
                    "autoconnect", "no",
                    "ssid", self.con_name,
                    "mode", "ap"
                ], check=True)
                
                # Configure security
                subprocess.run([
                    "nmcli", "con", "modify", self.con_name,
                    "802-11-wireless.band", "bg",
                    "802-11-wireless.channel", "6",
                    "802-11-wireless-security.key-mgmt", "wpa-psk",
                    "802-11-wireless-security.psk", "drone123",
                    "ipv4.method", "shared"
                ], check=True)
                
                # Activate
                subprocess.run(["nmcli", "con", "up", self.con_name], check=True)
            
            # Step 3: Disable other WiFi autoconnect
            self.disable_other_wifi_autoconnect()
            
            # Step 4: Verify hotspot is active
            time.sleep(3)
            if self.verify_hotspot():
                print("[SUCCESS] Hotspot is active and verified!")
                return True
            else:
                print("[ERROR] Hotspot verification failed")
                self.teardown_hotspot()
                return False
                
        except Exception as e:
            print(f"[ERROR] Hotspot creation failed: {e}")
            self.restore_wifi_state()
            return False
    
    def verify_hotspot(self):
        """Verify hotspot is running correctly"""
        # Check connection is active
        result = subprocess.run(
            ["nmcli", "-t", "-f", "NAME,STATE", "con", "show", "--active"],
            capture_output=True, text=True
        )
        if f"{self.con_name}:activated" not in result.stdout:
            return False
        
        # Check IP address
        result = subprocess.run(
            ["ip", "addr", "show", self.interface],
            capture_output=True, text=True
        )
        if "10.42.0.1" not in result.stdout:
            return False
        
        # Check AP mode
        result = subprocess.run(
            ["iw", "dev", self.interface, "info"],
            capture_output=True, text=True
        )
        return "type AP" in result.stdout
    
    def teardown_hotspot(self):
        """Safely teardown hotspot and restore network"""
        try:
            print("[TEARDOWN] Deactivating hotspot...")
            subprocess.run(
                ["nmcli", "con", "down", self.con_name],
                capture_output=True
            )
            
            print("[TEARDOWN] Restoring WiFi autoconnect state...")
            self.restore_wifi_state()
            
            # Give NetworkManager time to reconnect
            time.sleep(2)
            
            # Verify LAN is still available
            result = subprocess.run(
                ["nmcli", "dev", "status"],
                capture_output=True, text=True
            )
            if "ethernet" in result.stdout and "connected" in result.stdout:
                print("[VERIFY] ✓ LAN connection intact")
            else:
                print("[WARNING] LAN connection may be affected")
            
            print("[SUCCESS] Hotspot teardown complete")
            
        except Exception as e:
            print(f"[ERROR] Teardown error: {e}")

def main():
    manager = SafeHotspotManager()
    
    try:
        if manager.create_hotspot():
            print("\n" + "="*50)
            print("Hotspot Active!")
            print("SSID: DroneController")
            print("Password: drone123")
            print("IP: 10.42.0.1")
            print("="*50)
            print("\nPress Ctrl+C to stop...")
            
            while True:
                time.sleep(1)
                
    except KeyboardInterrupt:
        print("\n[INTERRUPT] Shutting down...")
        
    finally:
        manager.teardown_hotspot()

if __name__ == "__main__":
    main()
```

---

## 6. Recovery Procedures

### If Internet Is Lost After Running Network Code:

#### Step 1: Check NetworkManager Status
```bash
systemctl status NetworkManager
# If not running:
sudo systemctl start NetworkManager
sudo systemctl enable NetworkManager
```

#### Step 2: Restore DNS Configuration
```bash
# Check current DNS:
cat /etc/resolv.conf

# If empty or broken, restore systemd-resolved link:
sudo rm /etc/resolv.conf
sudo ln -s /run/systemd/resolve/stub-resolv.conf /etc/resolv.conf

# Restart DNS:
sudo systemctl restart systemd-resolved
```

#### Step 3: Re-enable WiFi Autoconnect
```bash
# List all WiFi connections:
nmcli con show

# Re-enable autoconnect for your home WiFi:
nmcli con modify "YourHomeWiFi" connection.autoconnect yes

# Reconnect:
nmcli con up "YourHomeWiFi"
```

#### Step 4: Reset Interface to Managed Mode
```bash
# If interface is stuck in AP mode:
sudo nmcli dev set wlP1p1s0 managed yes
sudo ip link set wlP1p1s0 down
sudo ip link set wlP1p1s0 up
```

#### Step 5: Full NetworkManager Restart
```bash
# Nuclear option - restart everything:
sudo systemctl restart NetworkManager
sudo systemctl restart systemd-resolved
sudo rfkill unblock all
```

#### Step 6: Verify Connectivity
```bash
# Test DNS:
nslookup google.com

# Test internet:
ping -c 3 8.8.8.8
ping -c 3 google.com

# Check routing:
ip route show
```

---

## 7. Pre-Deployment Checklist

Before deploying any network code to production:

- [ ] Code uses `nmcli` for all network operations
- [ ] Code NEVER uses `pkill` on system processes
- [ ] Code NEVER modifies `/etc/resolv.conf` directly
- [ ] Code implements `backup_state()` before changes
- [ ] Code implements `restore_state()` in teardown
- [ ] Code has exception handling for all network operations
- [ ] Code verifies connectivity after making changes
- [ ] Code includes timeout mechanisms (no infinite loops)
- [ ] Code logs all network operations for debugging
- [ ] Code has been tested with `--dry-run` mode first

---

## 8. Testing Requirements

All network code must pass these tests:

### Test 1: Hotspot Creation
```bash
# Create hotspot and verify:
1. Hotspot is active
2. LAN connection still works (ping 8.8.8.8 over Ethernet)
3. DNS still resolves (nslookup google.com)
```

### Test 2: Hotspot Teardown
```bash
# Stop hotspot and verify:
1. Hotspot is deactivated
2. WiFi autoconnect is restored
3. System reconnects to home WiFi automatically
4. Internet is accessible
```

### Test 3: Crash Recovery
```bash
# Kill process mid-operation and verify:
1. System can recover without manual intervention
2. NetworkManager is still running
3. DNS configuration is intact
4. LAN or WiFi connectivity restored within 30 seconds
```

### Test 4: Reboot Persistence
```bash
# After reboot, verify:
1. NetworkManager starts automatically
2. DNS configuration is correct
3. Home WiFi autoconnect works
4. LAN connection works
```

---

## 9. Emergency Contact

If the system loses network connectivity and cannot be recovered:

1. **Physical Access:** Connect via HDMI/keyboard or serial console
2. **Restore Script:** Run `/home/angelo/Projects/Drone-Fieldtest/scripts/restore_network.sh` (to be created)
3. **Factory Reset:** `sudo nmcli con reload && sudo systemctl restart NetworkManager`
4. **Last Resort:** Manual configuration via `/etc/NetworkManager/system-connections/`

---

## 10. Version History

| Version | Date       | Changes                                      |
|---------|------------|----------------------------------------------|
| 1.0     | 2025-11-12 | Initial policy after multiple network failures |

---

## 11. Approval & Enforcement

**Status:** MANDATORY  
**Enforcement:** All network-related PRs must pass policy compliance check  
**Review:** Angelo must approve all network code changes  
**Violations:** Code that violates this policy will be rejected immediately

---

**Remember:** It's better to have NO hotspot functionality than to break the system's internet connection! 🛡️
