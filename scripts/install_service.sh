#!/bin/bash
# Installation script for the drone recorder systemd service

set -e

echo "Installing Drone Field Testing Service..."

# Kopiere Service-Datei nach systemd
sudo cp /home/angelo/Projects/Drone-Fieldtest/systemd/drone-recorder.service /etc/systemd/system/

# Lade systemd neu
sudo systemctl daemon-reload

# Aktiviere Service für Autostart
sudo systemctl enable drone-recorder.service

echo "Service installed successfully!"
echo "Commands:"
echo "  Start service:  sudo systemctl start drone-recorder"
echo "  Stop service:   sudo systemctl stop drone-recorder"
echo "  Check status:   sudo systemctl status drone-recorder"
echo "  View logs:      sudo journalctl -u drone-recorder -f"