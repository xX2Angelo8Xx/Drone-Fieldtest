#!/usr/bin/env python3
"""
ZED Performance Test - HD720 @ 60 FPS
Tests if ZED SDK can achieve 60 FPS with official Python API
"""

import sys
import pyzed.sl as sl
from signal import signal, SIGINT
import time

cam = sl.Camera()
start_time = None
frames_recorded = 0

def handler(signal_received, frame):
    global start_time, frames_recorded
    cam.disable_recording()
    
    if start_time:
        duration = time.time() - start_time
        actual_fps = frames_recorded / duration
        print(f"\n\n=== RECORDING COMPLETE ===")
        print(f"Frames recorded: {frames_recorded}")
        print(f"Duration: {duration:.2f} seconds")
        print(f"Target FPS: 60")
        print(f"Actual FPS: {actual_fps:.2f}")
        print(f"FPS Achievement: {(actual_fps/60)*100:.1f}%")
    
    cam.close()
    sys.exit(0)

signal(SIGINT, handler)

def main():
    global start_time, frames_recorded
    
    # Initialize camera with HD720 @ 60 FPS
    init = sl.InitParameters()
    init.camera_resolution = sl.RESOLUTION.HD720  # 1280x720
    init.camera_fps = 60  # Target 60 FPS
    init.depth_mode = sl.DEPTH_MODE.NONE  # No depth for pure SVO performance test
    init.coordinate_units = sl.UNIT.METER
    init.coordinate_system = sl.COORDINATE_SYSTEM.RIGHT_HANDED_Y_UP
    
    print("Opening ZED camera with HD720 @ 60 FPS...")
    status = cam.open(init)
    if status != sl.ERROR_CODE.SUCCESS:
        print(f"Camera Open failed: {status}")
        exit(1)
    
    # Enable recording with LOSSLESS compression (same as our system)
    output_file = "/media/angelo/DRONE_DATA1/zed_test_60fps.svo2"
    recording_param = sl.RecordingParameters(output_file, sl.SVO_COMPRESSION_MODE.LOSSLESS)
    recording_param.target_framerate = 60
    
    print(f"Enabling recording to: {output_file}")
    err = cam.enable_recording(recording_param)
    if err != sl.ERROR_CODE.SUCCESS:
        print(f"Recording enable failed: {err}")
        exit(1)
    
    runtime = sl.RuntimeParameters()
    print("\n=== RECORDING STARTED ===")
    print("Recording HD720 @ 60 FPS with LOSSLESS compression")
    print("Press Ctrl+C to stop (recommend 60 seconds for 1-minute test)\n")
    
    start_time = time.time()
    last_print = start_time
    
    while True:
        if cam.grab(runtime) == sl.ERROR_CODE.SUCCESS:
            frames_recorded += 1
            
            # Print stats every second
            now = time.time()
            if now - last_print >= 1.0:
                elapsed = now - start_time
                current_fps = frames_recorded / elapsed
                print(f"Time: {elapsed:.1f}s | Frames: {frames_recorded} | "
                      f"Current FPS: {current_fps:.2f} | Target: 60 FPS", end="\r")
                last_print = now

if __name__ == "__main__":
    main()
