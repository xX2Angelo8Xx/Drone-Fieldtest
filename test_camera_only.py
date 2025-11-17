#!/usr/bin/env python3
"""
ZED Camera-Only Performance Test (No Recording)
Tests pure camera grab() performance without compression overhead
"""

import sys
import pyzed.sl as sl
from signal import signal, SIGINT
import time

cam = sl.Camera()
start_time = None
frames_captured = 0

def handler(signal_received, frame):
    global start_time, frames_captured
    
    if start_time:
        duration = time.time() - start_time
        actual_fps = frames_captured / duration
        print(f"\n\n=== TEST COMPLETE ===")
        print(f"Frames captured: {frames_captured}")
        print(f"Duration: {duration:.2f} seconds")
        print(f"Target FPS: 60")
        print(f"Actual FPS: {actual_fps:.2f}")
        print(f"FPS Achievement: {(actual_fps/60)*100:.1f}%")
        print(f"\nConclusion: Camera {'CAN' if actual_fps > 55 else 'CANNOT'} sustain 60 FPS")
    
    cam.close()
    sys.exit(0)

signal(SIGINT, handler)

def main():
    global start_time, frames_captured
    
    # Initialize camera with HD720 @ 60 FPS
    init = sl.InitParameters()
    init.camera_resolution = sl.RESOLUTION.HD720  # 1280x720
    init.camera_fps = 60  # Target 60 FPS
    init.depth_mode = sl.DEPTH_MODE.NONE  # No depth
    init.coordinate_units = sl.UNIT.METER
    init.coordinate_system = sl.COORDINATE_SYSTEM.RIGHT_HANDED_Y_UP
    
    print("Opening ZED camera with HD720 @ 60 FPS...")
    print("(Camera-only test - NO RECORDING)")
    status = cam.open(init)
    if status != sl.ERROR_CODE.SUCCESS:
        print(f"Camera Open failed: {status}")
        exit(1)
    
    runtime = sl.RuntimeParameters()
    print("\n=== TEST STARTED ===")
    print("Capturing frames WITHOUT recording")
    print("This tests pure camera performance without compression overhead")
    print("Press Ctrl+C to stop (recommend 60 seconds)\n")
    
    start_time = time.time()
    last_print = start_time
    
    while True:
        if cam.grab(runtime) == sl.ERROR_CODE.SUCCESS:
            frames_captured += 1
            
            # Print stats every second
            now = time.time()
            if now - last_print >= 1.0:
                elapsed = now - start_time
                current_fps = frames_captured / elapsed
                print(f"Time: {elapsed:.1f}s | Frames: {frames_captured} | "
                      f"Current FPS: {current_fps:.2f} | Target: 60 FPS", end="\r")
                last_print = now

if __name__ == "__main__":
    main()
