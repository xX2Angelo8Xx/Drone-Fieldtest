#!/usr/bin/env python3
"""
ZED Camera FPS Test - NO RECORDING
Tests maximum camera capture rate without recording overhead
"""

import sys
import pyzed.sl as sl
from signal import signal, SIGINT
import time

cam = sl.Camera()
start_time = None
frames_grabbed = 0

def handler(signal_received, frame):
    global start_time, frames_grabbed
    
    if start_time:
        duration = time.time() - start_time
        actual_fps = frames_grabbed / duration
        print(f"\n\n=== TEST COMPLETE ===")
        print(f"Frames grabbed: {frames_grabbed}")
        print(f"Duration: {duration:.2f} seconds")
        print(f"Target FPS: 60")
        print(f"Actual FPS: {actual_fps:.2f}")
        print(f"FPS Achievement: {(actual_fps/60)*100:.1f}%")
    
    cam.close()
    sys.exit(0)

signal(SIGINT, handler)

def main():
    global start_time, frames_grabbed
    
    # Initialize camera with HD720 @ 60 FPS
    init = sl.InitParameters()
    init.camera_resolution = sl.RESOLUTION.HD720
    init.camera_fps = 60
    init.depth_mode = sl.DEPTH_MODE.NONE
    
    print("Opening ZED camera with HD720 @ 60 FPS...")
    status = cam.open(init)
    if status != sl.ERROR_CODE.SUCCESS:
        print(f"Camera Open failed: {status}")
        exit(1)
    
    runtime = sl.RuntimeParameters()
    print("\n=== CAMERA CAPTURE TEST (NO RECORDING) ===")
    print("Testing maximum grab() rate without recording overhead")
    print("Press Ctrl+C to stop (recommend 60 seconds)\n")
    
    start_time = time.time()
    last_print = start_time
    
    while True:
        if cam.grab(runtime) == sl.ERROR_CODE.SUCCESS:
            frames_grabbed += 1
            
            # Print stats every second
            now = time.time()
            if now - last_print >= 1.0:
                elapsed = now - start_time
                current_fps = frames_grabbed / elapsed
                print(f"Time: {elapsed:.1f}s | Frames: {frames_grabbed} | "
                      f"Current FPS: {current_fps:.2f} | Target: 60 FPS", end="\r")
                last_print = now

if __name__ == "__main__":
    main()
