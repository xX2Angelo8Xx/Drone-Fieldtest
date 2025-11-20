#include "battery_monitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

// JSON parsing (simple implementation)
#include <regex>

// INA219 Register addresses
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE  0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// INA219 Configuration bits
#define INA219_CONFIG_RESET             0x8000  // Reset bit
#define INA219_CONFIG_BVOLTAGERANGE_32V 0x2000  // 0-32V range
#define INA219_CONFIG_GAIN_8_320MV      0x1800  // Gain 8, 320mV range
#define INA219_CONFIG_BADCRES_12BIT     0x0400  // 12-bit bus ADC resolution
#define INA219_CONFIG_SADCRES_12BIT     0x0008  // 12-bit shunt ADC resolution
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007  // Continuous shunt and bus voltage

BatteryMonitor::BatteryMonitor(int i2c_bus, uint8_t i2c_address, 
                               float shunt_ohms, int battery_capacity_mah)
    : i2c_bus_(i2c_bus),
      i2c_address_(i2c_address),
      i2c_fd_(-1),
      shunt_ohms_(shunt_ohms),
      battery_capacity_mah_(battery_capacity_mah),
      critical_voltage_(14.6f),  // 3.65V per cell (safety margin - voltage jumps cause counter resets at 14.4V)
      warning_voltage_(14.8f),   // 3.7V per cell
      full_voltage_(16.8f),      // 4.2V per cell
      empty_voltage_(14.6f),     // 3.65V per cell (0% = critical shutdown threshold)
      calibration_slope1_(0.994957f),       // Recalibrated 2025-11-19 18:19 (segment 1: 14.6-15.7V, max error: 0.000V)
      calibration_offset1_(0.292509f),      // Recalibrated 2025-11-19 18:19 (improved lower range accuracy)
      calibration_slope2_(0.977262f),       // Recalibrated 2025-11-19 18:19 (segment 2: 15.7-16.8V, max error: 0.000V)
      calibration_offset2_(0.566513f),      // Recalibrated 2025-11-19 18:19 (improved upper range accuracy)
      calibration_midpoint_(15.7f),         // Calibrated midpoint (threshold between segments)
      calibration_raw_midpoint_(15.4856f),  // Raw midpoint from actual measurements (adjustable PSU + multimeter)
      percentage_filter_index_(0),
      percentage_filter_count_(0),
      percentage_filter_initialized_(false),
      runtime_filter_index_(0),
      runtime_filter_count_(0),
      runtime_filter_initialized_(false),
      running_(false) {
    
    std::cout << "[BatteryMonitor] Initialized for I2C bus " << i2c_bus 
              << ", address 0x" << std::hex << (int)i2c_address << std::dec << std::endl;
}

BatteryMonitor::~BatteryMonitor() {
    shutdown();
}

bool BatteryMonitor::initialize() {
    std::cout << "[BatteryMonitor] Initializing..." << std::endl;
    
    // Open I2C device
    if (!openI2C()) {
        std::cerr << "[BatteryMonitor] Failed to open I2C device" << std::endl;
        return false;
    }
    
    // Configure INA219
    if (!configureINA219()) {
        std::cerr << "[BatteryMonitor] Failed to configure INA219" << std::endl;
        closeI2C();
        return false;
    }
    
    // Initialize timing
    start_time_ = std::chrono::steady_clock::now();
    last_sample_time_ = start_time_;
    
    // Try to load calibration from file
    std::string calibration_file = "/home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json";
    if (loadCalibrationFromFile(calibration_file)) {
        std::cout << "[BatteryMonitor] ✓ Calibration loaded from file" << std::endl;
    } else {
        std::cout << "[BatteryMonitor] ⚠️ No calibration file found, using raw readings" << std::endl;
    }
    
    // Start monitoring thread
    running_ = true;
    monitor_thread_ = std::make_unique<std::thread>(&BatteryMonitor::monitorLoop, this);
    
    std::cout << "[BatteryMonitor] ✓ Initialized successfully" << std::endl;
    return true;
}

void BatteryMonitor::shutdown() {
    if (running_) {
        std::cout << "[BatteryMonitor] Shutting down..." << std::endl;
        running_ = false;
        
        if (monitor_thread_ && monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        
        closeI2C();
        std::cout << "[BatteryMonitor] Shutdown complete" << std::endl;
    }
}

bool BatteryMonitor::openI2C() {
    char device_path[20];
    snprintf(device_path, sizeof(device_path), "/dev/i2c-%d", i2c_bus_);
    
    i2c_fd_ = open(device_path, O_RDWR);
    if (i2c_fd_ < 0) {
        std::cerr << "[BatteryMonitor] Failed to open " << device_path 
                  << ": " << strerror(errno) << std::endl;
        return false;
    }
    
    if (ioctl(i2c_fd_, I2C_SLAVE, i2c_address_) < 0) {
        std::cerr << "[BatteryMonitor] Failed to set I2C slave address: " 
                  << strerror(errno) << std::endl;
        close(i2c_fd_);
        i2c_fd_ = -1;
        return false;
    }
    
    return true;
}

void BatteryMonitor::closeI2C() {
    if (i2c_fd_ >= 0) {
        close(i2c_fd_);
        i2c_fd_ = -1;
    }
}

bool BatteryMonitor::writeRegister(uint8_t reg, uint16_t value) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (value >> 8) & 0xFF;  // MSB
    buf[2] = value & 0xFF;          // LSB
    
    if (write(i2c_fd_, buf, 3) != 3) {
        std::cerr << "[BatteryMonitor] I2C write failed: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool BatteryMonitor::readRegister(uint8_t reg, uint16_t& value) {
    // Write register address
    if (write(i2c_fd_, &reg, 1) != 1) {
        std::cerr << "[BatteryMonitor] I2C write register failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Read 2 bytes
    uint8_t buf[2];
    if (read(i2c_fd_, buf, 2) != 2) {
        std::cerr << "[BatteryMonitor] I2C read failed: " << strerror(errno) << std::endl;
        return false;
    }
    
    value = (buf[0] << 8) | buf[1];
    return true;
}

bool BatteryMonitor::configureINA219() {
    // Reset INA219
    if (!writeRegister(INA219_REG_CONFIG, INA219_CONFIG_RESET)) {
        return false;
    }
    usleep(1000);  // Wait for reset
    
    // Configure for 32V range, 320mV shunt range, 12-bit resolution, continuous mode
    uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                      INA219_CONFIG_GAIN_8_320MV |
                      INA219_CONFIG_BADCRES_12BIT |
                      INA219_CONFIG_SADCRES_12BIT |
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
    
    if (!writeRegister(INA219_REG_CONFIG, config)) {
        return false;
    }
    
    // Calculate and write calibration value
    // Current_LSB = Maximum Expected Current / 32768
    // For 3A max: Current_LSB = 3 / 32768 = 0.0000916 A = 0.0916 mA
    // Cal = 0.04096 / (Current_LSB * Shunt_Resistance)
    // Cal = 0.04096 / (0.0000916 * 0.1) = 4472
    uint16_t calibration = 4096;  // Typical value for 0.1 ohm shunt, ~3A range
    
    if (!writeRegister(INA219_REG_CALIBRATION, calibration)) {
        return false;
    }
    
    return true;
}

bool BatteryMonitor::readSensors(float& voltage, float& current, float& power) {
    if (i2c_fd_ < 0) {
        return false;
    }
    
    uint16_t bus_voltage_raw, shunt_voltage_raw, power_raw, current_raw;
    
    // Read bus voltage (battery voltage)
    if (!readRegister(INA219_REG_BUS_VOLTAGE, bus_voltage_raw)) {
        return false;
    }
    
    // Read shunt voltage (voltage across shunt resistor)
    if (!readRegister(INA219_REG_SHUNT_VOLTAGE, shunt_voltage_raw)) {
        return false;
    }
    
    // Read current register
    if (!readRegister(INA219_REG_CURRENT, current_raw)) {
        return false;
    }
    
    // Read power register
    if (!readRegister(INA219_REG_POWER, power_raw)) {
        return false;
    }
    
    // Convert to actual values
    // Bus voltage: LSB = 4mV, shift right 3 bits (CNVR, OVF, and two reserved bits)
    float voltage_raw = ((bus_voltage_raw >> 3) * 4) / 1000.0f;  // RAW reading (uncalibrated)
    
    // Apply 2-segment piecewise linear calibration
    // CRITICAL: Use RAW voltage to select segment, not calibrated voltage!
    // Segment 1: raw < calibration_raw_midpoint_ → use slope1/offset1
    // Segment 2: raw >= calibration_raw_midpoint_ → use slope2/offset2
    if (voltage_raw < calibration_raw_midpoint_) {
        voltage = calibration_slope1_ * voltage_raw + calibration_offset1_;
    } else {
        voltage = calibration_slope2_ * voltage_raw + calibration_offset2_;
    }
    
    // Current: LSB depends on calibration (for cal=4096, LSB ≈ 0.1mA)
    // Treat as signed 16-bit value
    int16_t current_signed = (int16_t)current_raw;
    current = current_signed * 0.0001f;  // Convert to Amps (0.1mA LSB)
    
    // Power: LSB is 20 * Current_LSB = 20 * 0.1mA = 2mW
    power = power_raw * 0.002f;  // Convert to Watts (2mW LSB)
    
    return true;
}

void BatteryMonitor::monitorLoop() {
    std::cout << "[BatteryMonitor] Monitoring thread started" << std::endl;
    
    const int sample_interval_ms = 1000;  // 1 second interval
    
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        
        float voltage, current, power;
        bool read_success = readSensors(voltage, current, power);
        
        if (read_success) {
            // Calculate duration since last sample
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                now - last_sample_time_).count() / 1000000.0f;
            last_sample_time_ = now;
            
            // Update energy consumption
            updateEnergyConsumption(current, duration);
            
            // Calculate battery state
            float cell_voltage = voltage / num_cells_;
            int percentage_raw = calculateBatteryPercentage(voltage);  // Raw calculation
            int percentage_filtered = applyPercentageFilter(percentage_raw);  // Smoothed for display
            float runtime = estimateRuntimeMinutes(current);
            
            // Update status (thread-safe)
            {
                std::lock_guard<std::mutex> lock(status_mutex_);
                current_status_.voltage = voltage;
                current_status_.cell_voltage = cell_voltage;
                current_status_.current = current;
                current_status_.power = power;
                current_status_.battery_percentage = percentage_filtered;  // Use filtered value for display
                current_status_.estimated_runtime_minutes = runtime;
                
                // Status flags
                current_status_.is_critical = (voltage < critical_voltage_);
                current_status_.is_warning = (voltage < warning_voltage_);
                current_status_.is_healthy = (voltage >= warning_voltage_);
                current_status_.hardware_error = false;
                
                // Statistics
                current_status_.sample_count++;
                auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    now - start_time_).count();
                current_status_.uptime_seconds = uptime;
            }
            
            // Log critical conditions
            if (voltage < critical_voltage_) {
                std::cerr << "[BatteryMonitor] 🚨 CRITICAL VOLTAGE: " << voltage 
                          << "V (" << cell_voltage << "V/cell)" << std::endl;
            }
            
        } else {
            std::lock_guard<std::mutex> lock(status_mutex_);
            current_status_.hardware_error = true;
        }
        
        // Sleep for remainder of interval
        std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));
    }
    
    std::cout << "[BatteryMonitor] Monitoring thread stopped" << std::endl;
}

void BatteryMonitor::updateEnergyConsumption(float current_a, float duration_seconds) {
    // Energy in Wh = Power * Time = (Voltage * Current) * (Hours)
    float energy_wh = current_status_.voltage * current_a * (duration_seconds / 3600.0f);
    
    // Charge in mAh = Current * Time = Current(A) * 1000 * (Hours)
    float charge_mah = current_a * 1000.0f * (duration_seconds / 3600.0f);
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_status_.energy_consumed_wh += energy_wh;
    current_status_.energy_consumed_mah += charge_mah;
}

int BatteryMonitor::calculateBatteryPercentage(float voltage) {
    // Linear interpolation between empty and full voltage
    // Empty: 14.6V (3.65V/cell) = 0% (critical shutdown threshold)
    // Full: 16.8V (4.2V/cell) = 100%
    
    if (voltage >= full_voltage_) {
        return 100;
    }
    if (voltage <= empty_voltage_) {
        return 0;
    }
    
    float percentage = ((voltage - empty_voltage_) / (full_voltage_ - empty_voltage_)) * 100.0f;
    return static_cast<int>(std::round(percentage));
}

float BatteryMonitor::estimateRuntimeMinutes(float current_a) {
    // Use mAh-based calculation for more stable estimates
    // Battery capacity: 930mAh for 4S LiPo
    
    // Calculate remaining capacity based on voltage range
    // Full: 16.8V (930mAh), Empty: 14.6V (0mAh)
    float voltage = current_status_.voltage;
    float voltage_range = full_voltage_ - empty_voltage_;  // 16.8 - 14.6 = 2.2V
    float voltage_remaining = voltage - empty_voltage_;    // e.g., 15.8 - 14.6 = 1.2V
    
    // Calculate remaining mAh from voltage (linear approximation)
    float remaining_mah = 930.0f * (voltage_remaining / voltage_range);
    
    if (remaining_mah <= 0) {
        return 0.0f;
    }
    
    // Estimate current consumption based on system state
    // These are empirically determined averages:
    // - IDLE: ~0.5A (display, WiFi, sensors)
    // - RECORDING: ~3.0A (camera, encoding, writing)
    float estimated_current_ma;
    if (current_a >= 2.0f) {
        // High current - likely recording
        estimated_current_ma = 3000.0f;  // 3.0A typical during recording
    } else if (current_a >= 1.0f) {
        // Medium current - transitional state
        estimated_current_ma = current_a * 1000.0f;  // Use actual current
    } else {
        // Low current - idle state
        estimated_current_ma = 500.0f;   // 0.5A typical idle
    }
    
    // Runtime = Remaining capacity (mAh) / Current draw (mA) = hours
    float runtime_hours = remaining_mah / estimated_current_ma;
    float runtime_minutes = runtime_hours * 60.0f;
    
    // Clamp to reasonable range
    if (runtime_minutes > 999.9f) {
        runtime_minutes = 999.9f;
    }
    
    // Apply moving average filter to smooth runtime jumps (5-sample window)
    float filtered_runtime = applyRuntimeFilter(runtime_minutes);
    
    return filtered_runtime;
}

BatteryStatus BatteryMonitor::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_;
}

bool BatteryMonitor::isHealthy() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_.is_healthy && !current_status_.hardware_error;
}

bool BatteryMonitor::isCritical() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_.is_critical;
}

bool BatteryMonitor::isWarning() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return current_status_.is_warning;
}

void BatteryMonitor::setBatteryCapacity(int capacity_mah) {
    battery_capacity_mah_ = capacity_mah;
    std::cout << "[BatteryMonitor] Battery capacity set to " << capacity_mah << " mAh" << std::endl;
}

void BatteryMonitor::setVoltageThresholds(float critical_v, float warning_v) {
    critical_voltage_ = critical_v;
    warning_voltage_ = warning_v;
    std::cout << "[BatteryMonitor] Voltage thresholds set: critical=" << critical_v 
              << "V, warning=" << warning_v << "V" << std::endl;
}

void BatteryMonitor::setVoltageCalibration(float slope, float offset) {
    // Legacy 1-segment calibration (use same values for both segments)
    calibration_slope1_ = slope;
    calibration_offset1_ = offset;
    calibration_slope2_ = slope;
    calibration_offset2_ = offset;
    std::cout << "[BatteryMonitor] Voltage calibration set (1-segment mode):" << std::endl;
    std::cout << "[BatteryMonitor]   Calibrated_V = " << slope << " × Raw_V " 
              << std::showpos << offset << std::noshowpos << std::endl;
}

bool BatteryMonitor::loadCalibrationFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Read entire file
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Try 2-segment calibration first (slope1, offset1, slope2, offset2, midpoint, raw_midpoint)
    std::regex slope1_regex(R"("slope1"\s*:\s*([0-9.]+))");
    std::regex offset1_regex(R"("offset1"\s*:\s*(-?[0-9.]+))");
    std::regex slope2_regex(R"("slope2"\s*:\s*([0-9.]+))");
    std::regex offset2_regex(R"("offset2"\s*:\s*(-?[0-9.]+))");
    std::regex midpoint_regex(R"("midpoint"\s*:\s*([0-9.]+))");
    std::regex raw_readings_regex(R"("raw_readings"\s*:\s*\[\s*[0-9.]+\s*,\s*([0-9.]+))");
    
    std::smatch match;
    float slope1, offset1, slope2, offset2, midpoint, raw_midpoint = 15.515f;
    
    bool has_2segment = true;
    if (std::regex_search(content, match, slope1_regex)) {
        slope1 = std::stof(match[1].str());
    } else {
        has_2segment = false;
    }
    
    if (std::regex_search(content, match, offset1_regex)) {
        offset1 = std::stof(match[1].str());
    } else {
        has_2segment = false;
    }
    
    if (std::regex_search(content, match, slope2_regex)) {
        slope2 = std::stof(match[1].str());
    } else {
        has_2segment = false;
    }
    
    if (std::regex_search(content, match, offset2_regex)) {
        offset2 = std::stof(match[1].str());
    } else {
        has_2segment = false;
    }
    
    if (std::regex_search(content, match, midpoint_regex)) {
        midpoint = std::stof(match[1].str());
    } else {
        has_2segment = false;
    }
    
    if (has_2segment) {
        // Apply 2-segment calibration
        calibration_slope1_ = slope1;
        calibration_offset1_ = offset1;
        calibration_slope2_ = slope2;
        calibration_offset2_ = offset2;
        calibration_midpoint_ = midpoint;
        // Try to parse raw_readings[] from JSON to determine raw midpoint for segment selection
        std::regex raw_readings_regex_full(R"("raw_readings"\s*:\s*\[([^\]]+)\])");
        if (std::regex_search(content, match, raw_readings_regex_full)) {
            std::string arr = match[1].str();
            // Split by comma and parse second element (mid raw)
            std::regex num_regex(R"((-?[0-9]+\.?[0-9]*))");
            std::sregex_iterator it(arr.begin(), arr.end(), num_regex);
            std::sregex_iterator end;
            std::vector<float> raw_vals;
            for (; it != end; ++it) {
                raw_vals.push_back(std::stof((*it)[1].str()));
            }
            if (raw_vals.size() >= 2) {
                calibration_raw_midpoint_ = raw_vals[1];
            } else {
                calibration_raw_midpoint_ = midpoint; // fallback: use calibrated midpoint
            }
        } else {
            calibration_raw_midpoint_ = midpoint; // fallback
        }
        
        std::cout << "[BatteryMonitor] ✓ Loaded 2-segment calibration from: " << filepath << std::endl;
        std::cout << "[BatteryMonitor]   Segment 1 (raw <" << calibration_raw_midpoint_ << "V): Calibrated_V = " 
                  << slope1 << " × Raw_V " << std::showpos << offset1 << std::noshowpos << std::endl;
        std::cout << "[BatteryMonitor]   Segment 2 (raw >=" << calibration_raw_midpoint_ << "V): Calibrated_V = " 
                  << slope2 << " × Raw_V " << std::showpos << offset2 << std::noshowpos << std::endl;
        std::cout << "[BatteryMonitor]   Calibrated midpoint: " << midpoint << "V" << std::endl;
        return true;
    }
    
    // Fallback: Try legacy 1-segment calibration (slope, offset)
    std::regex slope_regex(R"("slope"\s*:\s*([0-9.]+))");
    std::regex offset_regex(R"("offset"\s*:\s*(-?[0-9.]+))");
    
    float slope = 1.0f;
    float offset = 0.0f;
    
    if (std::regex_search(content, match, slope_regex)) {
        slope = std::stof(match[1].str());
    } else {
        std::cerr << "[BatteryMonitor] Failed to parse calibration from file" << std::endl;
        return false;
    }
    
    if (std::regex_search(content, match, offset_regex)) {
        offset = std::stof(match[1].str());
    } else {
        std::cerr << "[BatteryMonitor] Failed to parse calibration from file" << std::endl;
        return false;
    }
    
    // Apply legacy 1-segment calibration (use same values for both segments)
    calibration_slope1_ = slope;
    calibration_offset1_ = offset;
    calibration_slope2_ = slope;
    calibration_offset2_ = offset;
    
    std::cout << "[BatteryMonitor] ⚠️ Loaded legacy 1-segment calibration from: " << filepath << std::endl;
    std::cout << "[BatteryMonitor]   Formula: Calibrated_V = " << slope << " × Raw_V " 
              << std::showpos << offset << std::noshowpos << std::endl;
    std::cout << "[BatteryMonitor]   → Run calibrate_ina219_3point.py to upgrade to 2-segment!" << std::endl;
    
    return true;
}

int BatteryMonitor::applyPercentageFilter(int raw_percentage) {
    // Initialize filter on first call - fill buffer with first value
    if (!percentage_filter_initialized_) {
        for (int i = 0; i < PERCENTAGE_FILTER_SIZE; i++) {
            percentage_filter_buffer_[i] = raw_percentage;
        }
        percentage_filter_count_ = PERCENTAGE_FILTER_SIZE;
        percentage_filter_initialized_ = true;
        return raw_percentage;
    }
    
    // Add new value to circular buffer
    percentage_filter_buffer_[percentage_filter_index_] = raw_percentage;
    percentage_filter_index_ = (percentage_filter_index_ + 1) % PERCENTAGE_FILTER_SIZE;
    
    // Update count if still filling buffer
    if (percentage_filter_count_ < PERCENTAGE_FILTER_SIZE) {
        percentage_filter_count_++;
    }
    
    // Calculate moving average
    int sum = 0;
    for (int i = 0; i < percentage_filter_count_; i++) {
        sum += percentage_filter_buffer_[i];
    }
    
    return sum / percentage_filter_count_;
}

float BatteryMonitor::applyRuntimeFilter(float raw_runtime) {
    // Initialize filter on first call - fill buffer with first value
    if (!runtime_filter_initialized_) {
        for (int i = 0; i < RUNTIME_FILTER_SIZE; i++) {
            runtime_filter_buffer_[i] = raw_runtime;
        }
        runtime_filter_count_ = RUNTIME_FILTER_SIZE;
        runtime_filter_initialized_ = true;
        return raw_runtime;
    }
    
    // Add new value to circular buffer
    runtime_filter_buffer_[runtime_filter_index_] = raw_runtime;
    runtime_filter_index_ = (runtime_filter_index_ + 1) % RUNTIME_FILTER_SIZE;
    
    // Update count if still filling buffer
    if (runtime_filter_count_ < RUNTIME_FILTER_SIZE) {
        runtime_filter_count_++;
    }
    
    // Calculate moving average
    float sum = 0.0f;
    for (int i = 0; i < runtime_filter_count_; i++) {
        sum += runtime_filter_buffer_[i];
    }
    
    return sum / runtime_filter_count_;
}
