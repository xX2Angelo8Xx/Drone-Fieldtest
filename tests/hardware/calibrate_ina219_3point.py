#!/usr/bin/env python3
"""
INA219 3-Point Calibration Tool with Persistent Storage
Calibrates INA219 at three voltage points for accurate battery monitoring
Stores calibration coefficients for automatic loading at system startup
"""

import sys
import time
import json
import os

CALIBRATION_FILE = "/home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json"

# Calibration points for 4S LiPo battery
CALIBRATION_POINTS = [
    {"name": "Minimum (Critical)", "voltage": 14.6, "description": "3.65V per cell"},
    {"name": "Middle (Midpoint)", "voltage": 15.7, "description": "3.925V per cell"},
    {"name": "Maximum (Full)", "voltage": 16.8, "description": "4.2V per cell"}
]

def read_ina219_samples(ina, num_samples=20, duration_seconds=2.0):
    """Read multiple samples from INA219 and return average"""
    print(f"   Taking {num_samples} samples over {duration_seconds:.1f} seconds...")
    
    samples = []
    interval = duration_seconds / num_samples
    
    for i in range(num_samples):
        try:
            voltage = ina.voltage()
            samples.append(voltage)
            if (i + 1) % 5 == 0:  # Progress indicator every 5 samples
                print(f"   Progress: {i+1}/{num_samples} samples...")
            time.sleep(interval)
        except Exception as e:
            print(f"   ⚠️ Sample {i+1} failed: {e}")
            continue
    
    if not samples:
        return None
    
    avg = sum(samples) / len(samples)
    std_dev = (sum((x - avg) ** 2 for x in samples) / len(samples)) ** 0.5
    
    print(f"   ✓ Average: {avg:.3f}V (std dev: {std_dev:.4f}V)")
    return avg

def linear_calibration(raw_readings, reference_voltages):
    """
    Calculate linear calibration coefficients (slope and offset)
    using least squares fit
    
    Calibrated_Voltage = slope * Raw_Reading + offset
    """
    n = len(raw_readings)
    
    # Calculate means
    mean_raw = sum(raw_readings) / n
    mean_ref = sum(reference_voltages) / n
    
    # Calculate slope (m)
    numerator = sum((raw_readings[i] - mean_raw) * (reference_voltages[i] - mean_ref) 
                    for i in range(n))
    denominator = sum((raw_readings[i] - mean_raw) ** 2 for i in range(n))
    
    if denominator == 0:
        return 1.0, 0.0  # No calibration needed
    
    slope = numerator / denominator
    
    # Calculate offset (b)
    offset = mean_ref - slope * mean_raw
    
    return slope, offset

def piecewise_calibration(raw_readings, reference_voltages):
    """
    Calculate 2-segment piecewise linear calibration
    
    Segment 1: Low to Mid (14.6V → 15.7V)
    Segment 2: Mid to High (15.7V → 16.8V)
    
    Returns:
        (slope1, offset1, slope2, offset2, midpoint_voltage)
    """
    # Segment 1: Points 0-1 (Low to Mid)
    raw_low = raw_readings[0]
    raw_mid = raw_readings[1]
    ref_low = reference_voltages[0]
    ref_mid = reference_voltages[1]
    
    slope1 = (ref_mid - ref_low) / (raw_mid - raw_low) if (raw_mid - raw_low) != 0 else 1.0
    offset1 = ref_low - slope1 * raw_low
    
    # Segment 2: Points 1-2 (Mid to High)
    raw_high = raw_readings[2]
    ref_high = reference_voltages[2]
    
    slope2 = (ref_high - ref_mid) / (raw_high - raw_mid) if (raw_high - raw_mid) != 0 else 1.0
    offset2 = ref_mid - slope2 * raw_mid
    
    # Midpoint is the transition voltage (use calibrated midpoint as threshold)
    midpoint = ref_mid
    
    return slope1, offset1, slope2, offset2, midpoint

def main():
    print("\n" + "=" * 80)
    print("  INA219 3-POINT CALIBRATION TOOL")
    print("=" * 80)
    print()
    print("This tool performs a professional 3-point calibration of the INA219.")
    print()
    print("Requirements:")
    print("  • Adjustable power supply (14.6V - 16.8V range)")
    print("  • INA219 connected to I2C bus 7 (0x40)")
    print()
    print("Calibration points:")
    for i, point in enumerate(CALIBRATION_POINTS, 1):
        print(f"  {i}. {point['name']:20s} {point['voltage']:5.1f}V  ({point['description']})")
    print()
    print("=" * 80)
    print()
    
    # Initialize INA219
    try:
        sys.path.insert(0, '/home/angelo/Projects/Drone-Fieldtest/.venv/lib/python3.10/site-packages')
        from ina219 import INA219
        
        print("Initializing INA219 on bus 7, address 0x40...")
        ina = INA219(shunt_ohms=0.1, max_expected_amps=3.0, busnum=7, address=0x40)
        ina.configure(ina.RANGE_16V)
        print("✓ INA219 initialized successfully\n")
        
    except Exception as e:
        print(f"❌ Error initializing INA219: {e}")
        print("   Make sure the INA219 is properly connected to I2C bus 7")
        return 1
    
    # Collect calibration data
    raw_readings = []
    reference_voltages = []
    
    for i, point in enumerate(CALIBRATION_POINTS, 1):
        print(f"\n{'─' * 80}")
        print(f"CALIBRATION POINT {i}/{len(CALIBRATION_POINTS)}: {point['name']}")
        print(f"{'─' * 80}")
        print()
        print(f"Please adjust your power supply to: {point['voltage']}V")
        print(f"({point['description']})")
        print()
        
        # Wait for user confirmation
        input("Press ENTER when power supply is set to {:.1f}V...".format(point['voltage']))
        print()
        
        # Read INA219
        print(f"Reading INA219 at calibration point {i}...")
        raw_reading = read_ina219_samples(ina, num_samples=20, duration_seconds=2.0)
        
        if raw_reading is None:
            print("❌ Failed to read INA219. Aborting calibration.")
            return 1
        
        raw_readings.append(raw_reading)
        reference_voltages.append(point['voltage'])
        
        error = raw_reading - point['voltage']
        error_percent = (error / point['voltage']) * 100
        
        print()
        print(f"   Reference voltage: {point['voltage']:.3f}V")
        print(f"   INA219 reading:    {raw_reading:.3f}V")
        print(f"   Error:             {error:+.3f}V ({error_percent:+.2f}%)")
        print()
    
    # Calculate calibration coefficients
    print("\n" + "=" * 80)
    print("  CALCULATING CALIBRATION COEFFICIENTS")
    print("=" * 80)
    print()
    
    # Calculate both 1-segment (legacy) and 2-segment (new)
    slope_1seg, offset_1seg = linear_calibration(raw_readings, reference_voltages)
    slope1, offset1, slope2, offset2, midpoint = piecewise_calibration(raw_readings, reference_voltages)
    
    print("1-SEGMENT (Legacy Linear Regression):")
    print(f"  Formula: Calibrated_V = {slope_1seg:.6f} × Raw_V {offset_1seg:+.6f}")
    print()
    
    print("2-SEGMENT (Piecewise Linear - RECOMMENDED):")
    print(f"  Segment 1 (<{midpoint:.1f}V): Calibrated_V = {slope1:.6f} × Raw_V {offset1:+.6f}")
    print(f"  Segment 2 (≥{midpoint:.1f}V): Calibrated_V = {slope2:.6f} × Raw_V {offset2:+.6f}")
    print()
    
    # Verify both calibration methods
    print("=" * 80)
    print("  VERIFICATION: 1-SEGMENT vs 2-SEGMENT")
    print("=" * 80)
    print()
    print(f"{'Point':<20} {'Reference':<12} {'Raw':<12} {'1-Seg Error':<14} {'2-Seg Error':<14}")
    print("─" * 80)
    
    max_error_1seg = 0
    max_error_2seg = 0
    
    for i, (raw, ref) in enumerate(zip(raw_readings, reference_voltages)):
        # 1-segment calibration
        calibrated_1seg = slope_1seg * raw + offset_1seg
        error_1seg = calibrated_1seg - ref
        max_error_1seg = max(max_error_1seg, abs(error_1seg))
        
        # 2-segment calibration
        if ref < midpoint:
            calibrated_2seg = slope1 * raw + offset1
        else:
            calibrated_2seg = slope2 * raw + offset2
        error_2seg = calibrated_2seg - ref
        max_error_2seg = max(max_error_2seg, abs(error_2seg))
        
        print(f"{CALIBRATION_POINTS[i]['name']:<20} {ref:>10.3f}V  {raw:>10.3f}V  "
              f"{error_1seg:>+12.4f}V  {error_2seg:>+12.4f}V")
    
    print()
    print(f"Maximum error (1-segment): {max_error_1seg:.4f}V ({(max_error_1seg/16.8)*100:.3f}%)")
    print(f"Maximum error (2-segment): {max_error_2seg:.4f}V ({(max_error_2seg/16.8)*100:.3f}%)")
    print()
    
    improvement = max_error_1seg - max_error_2seg
    print(f"✓ 2-segment reduces max error by {improvement:.4f}V ({(improvement/max_error_1seg)*100:.1f}%)")
    print()
    
    # Save calibration (2-segment as primary, 1-segment as legacy fallback)
    calibration_data = {
        "slope1": slope1,
        "offset1": offset1,
        "slope2": slope2,
        "offset2": offset2,
        "midpoint": midpoint,
        "slope": slope_1seg,  # Legacy 1-segment for compatibility
        "offset": offset_1seg,  # Legacy 1-segment for compatibility
        "calibration_date": time.strftime("%Y-%m-%d %H:%M:%S"),
        "raw_readings": raw_readings,
        "reference_voltages": reference_voltages,
        "max_error_1segment": max_error_1seg,
        "max_error_2segment": max_error_2seg,
        "calibration_points": CALIBRATION_POINTS
    }
    
    print("=" * 80)
    print("  SAVING CALIBRATION")
    print("=" * 80)
    print()
    print(f"Calibration file: {CALIBRATION_FILE}")
    
    try:
        with open(CALIBRATION_FILE, 'w') as f:
            json.dump(calibration_data, f, indent=2)
        print("✓ Calibration saved successfully")
        
        # Make file readable by all (so system service can access it)
        os.chmod(CALIBRATION_FILE, 0o644)
        print("✓ File permissions set (readable by all)")
        
    except Exception as e:
        print(f"❌ Failed to save calibration: {e}")
        return 1
    
    print()
    print("=" * 80)
    print("  CALIBRATION COMPLETE!")
    print("=" * 80)
    print()
    print("Next steps:")
    print("  1. Rebuild and deploy the drone controller:")
    print("     cd /home/angelo/Projects/Drone-Fieldtest")
    print("     ./scripts/build.sh")
    print("     sudo systemctl restart drone-recorder")
    print()
    print("  2. The calibration will be automatically loaded at startup")
    print()
    print("  3. Verify calibrated readings in the web UI:")
    print("     http://192.168.4.1:8080 → Power tab")
    print()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
