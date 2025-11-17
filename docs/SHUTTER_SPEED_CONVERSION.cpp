// ZED Camera Exposure to Shutter Speed Conversion
// 
// ZED SDK Exposure range: 0-100
// - Exposure value represents percentage of frame time used as shutter speed
// - 100 = full frame time (1/FPS)
// - 50 = half frame time (1/(FPS*2))
// - 25 = quarter frame time (1/(FPS*4))
//
// Formula:
//   Shutter Speed = 1 / (FPS × (100 / Exposure%))
//
// Examples at 60 FPS:
//   Exposure 100% → 1/60s shutter
//   Exposure 50%  → 1/120s shutter
//   Exposure 25%  → 1/240s shutter
//   Exposure 12.5% → 1/480s shutter
//
// Common Photography Shutter Speeds:
//   1/30, 1/60, 1/90, 1/120, 1/150, 1/180, 1/240, 1/480, 1/1000
//
// Conversion Table (60 FPS):
//   Shutter 1/30   → Exposure = 200% (clamped to 100, not possible)
//   Shutter 1/60   → Exposure = 100%
//   Shutter 1/90   → Exposure = 67%
//   Shutter 1/120  → Exposure = 50%
//   Shutter 1/150  → Exposure = 40%
//   Shutter 1/180  → Exposure = 33%
//   Shutter 1/240  → Exposure = 25%
//   Shutter 1/480  → Exposure = 12.5%
//   Shutter 1/1000 → Exposure = 6%

#include <cmath>

// Convert shutter speed fraction (e.g., 120 for 1/120s) to ZED exposure value (0-100)
// fps: Current camera FPS
// shutter_denominator: Bottom part of shutter fraction (e.g., 120 for 1/120s)
// Returns: Exposure value 0-100, or -1 for Auto
int shutterToExposure(int fps, int shutter_denominator) {
    if (shutter_denominator <= 0) return -1;  // Auto
    
    // Exposure% = (1/shutter_speed) / (1/fps) × 100
    // Simplified: Exposure% = fps / shutter_denominator × 100
    float exposure = (float)fps / (float)shutter_denominator * 100.0f;
    
    // Clamp to 0-100 range
    if (exposure > 100.0f) exposure = 100.0f;
    if (exposure < 0.0f) exposure = 0.0f;
    
    return (int)std::round(exposure);
}

// Convert ZED exposure value (0-100) to shutter speed denominator
// fps: Current camera FPS
// exposure: ZED exposure value 0-100
// Returns: Shutter speed denominator (e.g., 120 for 1/120s)
int exposureToShutter(int fps, int exposure) {
    if (exposure <= 0 || exposure > 100) return -1;  // Invalid or Auto
    
    // shutter_denominator = fps / (exposure / 100)
    // Simplified: shutter_denominator = fps × 100 / exposure
    float shutter = (float)fps * 100.0f / (float)exposure;
    
    return (int)std::round(shutter);
}

// Get nearest standard shutter speed from available options
// current_shutter: Current shutter denominator
// Returns: Index in standard shutter array
int getNearestShutterIndex(int current_shutter, const int* standard_shutters, int count) {
    int nearest_idx = 0;
    int min_diff = abs(standard_shutters[0] - current_shutter);
    
    for (int i = 1; i < count; i++) {
        int diff = abs(standard_shutters[i] - current_shutter);
        if (diff < min_diff) {
            min_diff = diff;
            nearest_idx = i;
        }
    }
    
    return nearest_idx;
}
