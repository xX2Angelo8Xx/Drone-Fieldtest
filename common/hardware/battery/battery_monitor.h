#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <cstdint>

/**
 * BatteryMonitor - Thread-safe INA219 battery monitoring
 * 
 * Monitors 4S LiPo battery via INA219 on I2C bus 7 (0x40)
 * Provides voltage, current, power, energy consumption tracking
 * 
 * Critical voltages for 4S LiPo:
 * - Nominal: 14.8V (3.7V/cell)
 * - Warning: 14.8V (3.7V/cell) - low battery warning
 * - Critical: 14.6V (3.65V/cell) - auto-shutdown threshold (voltage jumps cause counter resets at lower values)
 * - Full charge: 16.8V (4.2V/cell)
 * 
 * Note: 14.6V chosen over 14.4V for safety - rapid voltage fluctuations under load
 * prevent reliable debounce counter operation at lower thresholds.
 */

struct BatteryStatus {
    // Voltage measurements
    float voltage;              // Total battery voltage (V)
    float cell_voltage;         // Per-cell voltage (V) = voltage / 4
    
    // Current and power
    float current;              // Current draw (A)
    float power;                // Instantaneous power (W)
    
    // Energy consumption tracking
    float energy_consumed_wh;   // Total energy consumed (Wh)
    float energy_consumed_mah;  // Total charge consumed (mAh)
    
    // Battery state estimation
    int battery_percentage;     // Estimated remaining capacity (0-100%)
    float estimated_runtime_minutes; // Estimated remaining runtime at current load
    
    // Status flags
    bool is_critical;           // Voltage below critical threshold (14.6V - accounts for voltage fluctuations)
    bool is_warning;            // Voltage below warning threshold (14.8V)
    bool is_healthy;            // Battery in good operating range
    bool hardware_error;        // I2C communication error
    
    // Session statistics
    uint64_t sample_count;      // Number of samples collected
    float uptime_seconds;       // Monitoring session duration
    
    BatteryStatus() : 
        voltage(0), cell_voltage(0), current(0), power(0),
        energy_consumed_wh(0), energy_consumed_mah(0),
        battery_percentage(0), estimated_runtime_minutes(0),
        is_critical(false), is_warning(false), is_healthy(true),
        hardware_error(false), sample_count(0), uptime_seconds(0) {}
};

class BatteryMonitor {
public:
    /**
     * Constructor
     * @param i2c_bus I2C bus number (default: 7 for Jetson Orin Nano shared with LCD)
     * @param i2c_address I2C address (default: 0x40)
     * @param shunt_ohms Shunt resistor value in ohms (default: 0.1)
     * @param battery_capacity_mah Battery capacity in mAh (default: 5000 for typical 4S LiPo)
     */
    BatteryMonitor(int i2c_bus = 7, 
                   uint8_t i2c_address = 0x40,
                   float shunt_ohms = 0.1,
                   int battery_capacity_mah = 5000);
    ~BatteryMonitor();
    
    // Control methods
    bool initialize();
    void shutdown();
    
    // Status methods
    BatteryStatus getStatus() const;
    bool isHealthy() const;
    bool isCritical() const;
    bool isWarning() const;
    
    // Configuration
    void setBatteryCapacity(int capacity_mah);
    void setVoltageThresholds(float critical_v, float warning_v);
    void setVoltageCalibration(float slope, float offset);  // Set linear calibration
    bool loadCalibrationFromFile(const std::string& filepath);  // Load from JSON
    
    // Manual reading (for testing)
    bool readSensors(float& voltage, float& current, float& power);
    
private:
    // I2C configuration
    int i2c_bus_;
    uint8_t i2c_address_;
    int i2c_fd_;  // File descriptor for I2C device
    float shunt_ohms_;
    
    // Battery configuration
    int battery_capacity_mah_;
    const int num_cells_ = 4;  // 4S battery
    float critical_voltage_;    // 14.4V default (3.6V/cell, compensates for load drop)
    float warning_voltage_;     // 14.8V default
    float full_voltage_;        // 16.8V default (4.2V/cell)
    float empty_voltage_;       // 14.4V default (0% at critical threshold)
    
    // Calibration coefficients (2-segment piecewise linear)
    // Segment 1: 14.4V-15.7V (low to mid)
    float calibration_slope1_;    // Default: 1.0 (no scaling)
    float calibration_offset1_;   // Default: 0.0 (no offset)
    // Segment 2: 15.7V-16.8V (mid to high)
    float calibration_slope2_;    // Default: 1.0 (no scaling)
    float calibration_offset2_;   // Default: 0.0 (no offset)
    float calibration_midpoint_;  // Calibrated voltage threshold between segments (default: 15.7V)
    float calibration_raw_midpoint_;  // RAW voltage at midpoint for segment selection
    
    // Moving average filter for battery percentage display (smooths erratic jumps)
    static const int PERCENTAGE_FILTER_SIZE = 10;  // 10-second window (1 sample/sec)
    int percentage_filter_buffer_[PERCENTAGE_FILTER_SIZE];
    int percentage_filter_index_;
    int percentage_filter_count_;  // Number of valid samples in buffer
    bool percentage_filter_initialized_;
    
    // Moving average filter for runtime estimate (smooths calculation variations)
    static const int RUNTIME_FILTER_SIZE = 5;  // 5-second window (smaller for responsiveness)
    float runtime_filter_buffer_[RUNTIME_FILTER_SIZE];
    int runtime_filter_index_;
    int runtime_filter_count_;  // Number of valid samples in buffer
    bool runtime_filter_initialized_;
    
    // Monitoring thread
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> running_;
    
    // Thread-safe status
    mutable std::mutex status_mutex_;
    BatteryStatus current_status_;
    
    // Energy tracking
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_sample_time_;
    
    // Monitoring loop
    void monitorLoop();
    
    // INA219 low-level I2C operations
    bool openI2C();
    void closeI2C();
    bool writeRegister(uint8_t reg, uint16_t value);
    bool readRegister(uint8_t reg, uint16_t& value);
    bool configureINA219();
    
    // Data processing
    void updateEnergyConsumption(float current_a, float duration_seconds);
    int calculateBatteryPercentage(float voltage);
    float estimateRuntimeMinutes(float current_a);
    int applyPercentageFilter(int raw_percentage);  // Apply moving average filter to percentage
    float applyRuntimeFilter(float raw_runtime);    // Apply moving average filter to runtime estimate
};
