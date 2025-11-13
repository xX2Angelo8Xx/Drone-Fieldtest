#!/bin/bash
# DRONE FIELDTEST - BASH ALIASES
# Füge diese Zeilen zu ~/.bashrc hinzu für schnelle Befehle

# ZED Tools Shortcuts
alias zed-explorer='/usr/local/zed/tools/ZED_Explorer'
alias zed-diag='/usr/local/zed/tools/ZED_Diagnostic'
alias zed-depth='/usr/local/zed/tools/ZED_Depth_Viewer'
alias zed-sensor='/usr/local/zed/tools/ZED_Sensor_Viewer'

# Drone System Shortcuts
alias drone='sudo /home/angelo/Projects/Drone-Fieldtest/scripts/start_drone.sh'
alias drone-stop='sudo /home/angelo/Projects/Drone-Fieldtest/scripts/stop_drone.sh'
alias drone-log='tail -f /tmp/drone_controller.log'
alias drone-netlog='sudo tail -f /var/log/drone_controller_network.log'
alias drone-build='cd /home/angelo/Projects/Drone-Fieldtest && ./scripts/build.sh'
alias drone-service-start='sudo systemctl start drone-recorder'
alias drone-service-stop='sudo systemctl stop drone-recorder'
alias drone-service-status='sudo systemctl status drone-recorder'
alias drone-logs='sudo journalctl -u drone-recorder -f'

# Recording Shortcuts
alias drone-record-ai='cd /home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder && sudo ./smart_recorder realtime_light'
alias drone-record-training='cd /home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder && sudo ./smart_recorder training'
alias drone-record-test='cd /home/angelo/Projects/Drone-Fieldtest/build/apps/smart_recorder && sudo ./smart_recorder development'

# Data Management 
alias drone-data='ls -lt /media/angelo/DRONE_DATA/flight_* | head -10'
alias drone-latest='ls -lt /media/angelo/DRONE_DATA/flight_* | head -1'
alias drone-space='df -h /media/angelo/DRONE_DATA'

# Quick Functions
drone-open-latest() {
    LATEST=$(ls -t /media/angelo/DRONE_DATA/flight_*/video.svo* | head -1)
    if [ -f "$LATEST" ]; then
        echo "Opening: $LATEST"
        ZED_Explorer "$LATEST" &
    else
        echo "No SVO2 files found"
    fi
}

drone-open-file() {
    if [ -z "$1" ]; then
        echo "Usage: drone-open-file [filename or flight folder]"
        echo "Available flights:"
        ls /media/angelo/DRONE_DATA/flight_* 2>/dev/null | head -5
        return 1
    fi
    
    # Check if it's a direct file path
    if [[ "$1" == *.svo* ]]; then
        ZED_Explorer "$1" &
        return 0
    fi
    
    # Check if it's a flight folder name
    if [[ -d "/media/angelo/DRONE_DATA/$1" ]]; then
        SVO_FILE="/media/angelo/DRONE_DATA/$1/video.svo2"
        if [[ ! -f "$SVO_FILE" ]]; then
            SVO_FILE="/media/angelo/DRONE_DATA/$1/video.svo"
        fi
        
        if [[ -f "$SVO_FILE" ]]; then
            echo "Opening: $SVO_FILE"
            ZED_Explorer "$SVO_FILE" &
        else
            echo "No SVO file found in $1"
        fi
    else
        echo "File or folder not found: $1"
    fi
}

drone-profile-change() {
    if [ -z "$1" ]; then
        echo "Usage: drone-profile-change [realtime_light|training|realtime_heavy|development]"
        echo "Current profile:"
        grep "EXECUTABLE_PATH" /home/angelo/Projects/Drone-Fieldtest/apps/data_collector/autostart.sh
        return 1
    fi
    
    echo "Changing autostart profile to: $1"
    sed -i "s/realtime_light/$1/g" /home/angelo/Projects/Drone-Fieldtest/apps/data_collector/autostart.sh
    sed -i "s/training/$1/g" /home/angelo/Projects/Drone-Fieldtest/apps/data_collector/autostart.sh
    sed -i "s/realtime_heavy/$1/g" /home/angelo/Projects/Drone-Fieldtest/apps/data_collector/autostart.sh
    sed -i "s/development/$1/g" /home/angelo/Projects/Drone-Fieldtest/apps/data_collector/autostart.sh
    
    echo "Building and restarting service..."
    drone-build
    sudo systemctl restart drone-recorder
    echo "Profile changed to: $1"
}

echo "🚁 Drone Fieldtest Aliases loaded!"
echo "Available commands:"
echo "  zed-explorer, zed-diag, zed-depth, zed-sensor"  
echo "  drone-build, drone-service-*, drone-logs"
echo "  drone-record-*, drone-data, drone-space"
echo "  drone-open-latest, drone-profile-change [profile]"