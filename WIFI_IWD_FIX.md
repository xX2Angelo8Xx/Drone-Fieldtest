# WiFi iwd → wpa_supplicant Fix - Quick Reference

## Problem
WiFi interface stuck as "unavailable" after reboot.

## Root Cause
NetworkManager configured to use `iwd` backend, but `iwd` service not installed/running.

## Solution (One-Time Fix)
```bash
# Disable iwd configuration
sudo mv /etc/NetworkManager/conf.d/10-use-iwd.conf \
        /etc/NetworkManager/conf.d/10-use-iwd.conf.disabled

# Restart NetworkManager to use wpa_supplicant instead
sudo systemctl restart NetworkManager

# Wait a few seconds
sleep 5

# Verify WiFi is now available
nmcli device status
```

## Expected Result
```
DEVICE    TYPE      STATE      CONNECTION         
wlP1p1s0  wifi      connected  Connecto Patronum
```

## To Re-enable iwd (if needed in future)
```bash
# Install iwd first
sudo apt install iwd

# Enable iwd service
sudo systemctl enable --now iwd

# Restore configuration
sudo mv /etc/NetworkManager/conf.d/10-use-iwd.conf.disabled \
        /etc/NetworkManager/conf.d/10-use-iwd.conf

# Restart NetworkManager
sudo systemctl restart NetworkManager
```

## Verification Commands
```bash
# Check which backend NetworkManager is using
grep "wifi.backend" /etc/NetworkManager/conf.d/*.conf

# Check if iwd is running
systemctl status iwd

# Check if wpa_supplicant is running
ps aux | grep wpa_supplicant

# Test WiFi scanning capability
sudo iw dev wlP1p1s0 scan | head -20
```

## Related Files
- `/etc/NetworkManager/conf.d/10-use-iwd.conf` - iwd backend config (now disabled)
- `/etc/NetworkManager/NetworkManager.conf` - Main NetworkManager config
- See: `NETWORK_SAFETY_SOLUTION.md` for full documentation
