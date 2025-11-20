#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

// INA219 Register addresses
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE  0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

// INA219 Configuration bits
#define INA219_CONFIG_RESET             0x8000
#define INA219_CONFIG_BVOLTAGERANGE_32V 0x2000
#define INA219_CONFIG_GAIN_8_320MV      0x1800
#define INA219_CONFIG_BADCRES_12BIT     0x0400
#define INA219_CONFIG_SADCRES_12BIT     0x0008
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

class INA219 {
private:
    int i2c_fd_;
    int i2c_bus_;
    uint8_t i2c_address_;
    
public:
    INA219(int bus = 7, uint8_t address = 0x40) 
        : i2c_fd_(-1), i2c_bus_(bus), i2c_address_(address) {}
    
    ~INA219() {
        close();
    }
    
    bool open() {
        char device_path[20];
        snprintf(device_path, sizeof(device_path), "/dev/i2c-%d", i2c_bus_);
        
        i2c_fd_ = ::open(device_path, O_RDWR);
        if (i2c_fd_ < 0) {
            std::cerr << "Failed to open " << device_path << ": " << strerror(errno) << std::endl;
            return false;
        }
        
        if (ioctl(i2c_fd_, I2C_SLAVE, i2c_address_) < 0) {
            std::cerr << "Failed to set I2C slave address: " << strerror(errno) << std::endl;
            ::close(i2c_fd_);
            i2c_fd_ = -1;
            return false;
        }
        
        return configure();
    }
    
    void close() {
        if (i2c_fd_ >= 0) {
            ::close(i2c_fd_);
            i2c_fd_ = -1;
        }
    }
    
    bool configure() {
        // Reset
        if (!writeRegister(INA219_REG_CONFIG, INA219_CONFIG_RESET)) {
            return false;
        }
        usleep(1000);
        
        // Configure
        uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                          INA219_CONFIG_GAIN_8_320MV |
                          INA219_CONFIG_BADCRES_12BIT |
                          INA219_CONFIG_SADCRES_12BIT |
                          INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
        
        if (!writeRegister(INA219_REG_CONFIG, config)) {
            return false;
        }
        
        uint16_t calibration = 4096;
        return writeRegister(INA219_REG_CALIBRATION, calibration);
    }
    
    float readVoltage() {
        uint16_t bus_voltage_raw;
        if (!readRegister(INA219_REG_BUS_VOLTAGE, bus_voltage_raw)) {
            return -1.0f;
        }
        return ((bus_voltage_raw >> 3) * 4) / 1000.0f;
    }
    
private:
    bool writeRegister(uint8_t reg, uint16_t value) {
        uint8_t buf[3];
        buf[0] = reg;
        buf[1] = (value >> 8) & 0xFF;
        buf[2] = value & 0xFF;
        return write(i2c_fd_, buf, 3) == 3;
    }
    
    bool readRegister(uint8_t reg, uint16_t& value) {
        if (write(i2c_fd_, &reg, 1) != 1) {
            return false;
        }
        uint8_t buf[2];
        if (read(i2c_fd_, buf, 2) != 2) {
            return false;
        }
        value = (buf[0] << 8) | buf[1];
        return true;
    }
};

// Linear regression for piecewise calibration
struct CalibrationSegment {
    float slope;
    float offset;
};

CalibrationSegment fitLine(const std::vector<float>& raw, const std::vector<float>& reference) {
    int n = raw.size();
    float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    
    for (int i = 0; i < n; i++) {
        sum_x += raw[i];
        sum_y += reference[i];
        sum_xy += raw[i] * reference[i];
        sum_xx += raw[i] * raw[i];
    }
    
    CalibrationSegment seg;
    seg.slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
    seg.offset = (sum_y - seg.slope * sum_x) / n;
    
    return seg;
}

float calculateMaxError(const std::vector<float>& raw, const std::vector<float>& reference, 
                        CalibrationSegment seg) {
    float max_error = 0;
    for (size_t i = 0; i < raw.size(); i++) {
        float predicted = seg.slope * raw[i] + seg.offset;
        float error = std::abs(predicted - reference[i]);
        if (error > max_error) {
            max_error = error;
        }
    }
    return max_error;
}

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  INA219 Battery Monitor 2-Segment Calibration Tool      ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    INA219 sensor(7, 0x40);
    
    std::cout << "Initializing INA219 on I2C bus 7, address 0x40..." << std::endl;
    if (!sensor.open()) {
        std::cerr << "ERROR: Failed to initialize INA219" << std::endl;
        return 1;
    }
    std::cout << "✓ INA219 initialized successfully\n" << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "IMPORTANT CALIBRATION INSTRUCTIONS:" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "1. Connect adjustable power supply (14.6V - 16.8V)" << std::endl;
    std::cout << "2. Ensure NO RECORDING is active (idle state only!)" << std::endl;
    std::cout << "3. Use precise voltmeter to verify supply voltage" << std::endl;
    std::cout << "4. Wait 5 seconds after adjusting voltage before measuring" << std::endl;
    std::cout << "5. Critical points:" << std::endl;
    std::cout << "   - 14.6V (3.65V/cell) - Critical battery threshold" << std::endl;
    std::cout << "   - 15.7V (3.925V/cell) - Midpoint between segments" << std::endl;
    std::cout << "   - 16.8V (4.20V/cell) - Full charge" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    std::vector<float> reference_voltages = {14.6f, 15.7f, 16.8f};
    std::vector<float> raw_readings(3);
    
    for (int i = 0; i < 3; i++) {
        std::cout << "\n--- Calibration Point " << (i+1) << "/3 ---" << std::endl;
        std::cout << "Set power supply to EXACTLY " << std::fixed << std::setprecision(2) 
                  << reference_voltages[i] << "V" << std::endl;
        std::cout << "Verify with voltmeter, then press ENTER..." << std::endl;
        std::cin.get();
        
        // Take 10 samples and average
        std::cout << "Measuring (10 samples)..." << std::endl;
        float sum = 0;
        for (int j = 0; j < 10; j++) {
            float v = sensor.readVoltage();
            sum += v;
            std::cout << "  Sample " << (j+1) << ": " << std::setprecision(4) << v << "V" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        raw_readings[i] = sum / 10.0f;
        
        std::cout << "✓ Average RAW reading: " << std::setprecision(4) << raw_readings[i] << "V" << std::endl;
        std::cout << "✓ Reference voltage: " << std::setprecision(2) << reference_voltages[i] << "V" << std::endl;
        std::cout << "✓ Error: " << std::setprecision(3) << (raw_readings[i] - reference_voltages[i]) << "V" << std::endl;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "CALCULATING 2-SEGMENT CALIBRATION..." << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Segment 1: Points 0 and 1 (14.6V - 15.7V)
    std::vector<float> raw_low = {raw_readings[0], raw_readings[1]};
    std::vector<float> ref_low = {reference_voltages[0], reference_voltages[1]};
    CalibrationSegment seg1 = fitLine(raw_low, ref_low);
    
    // Segment 2: Points 1 and 2 (15.7V - 16.8V)
    std::vector<float> raw_high = {raw_readings[1], raw_readings[2]};
    std::vector<float> ref_high = {reference_voltages[1], reference_voltages[2]};
    CalibrationSegment seg2 = fitLine(raw_high, ref_high);
    
    // Calculate errors
    float error1 = calculateMaxError(raw_low, ref_low, seg1);
    float error2 = calculateMaxError(raw_high, ref_high, seg2);
    float max_error_2seg = std::max(error1, error2);
    
    // Also calculate 1-segment for comparison
    CalibrationSegment seg_1seg = fitLine(raw_readings, reference_voltages);
    float max_error_1seg = calculateMaxError(raw_readings, reference_voltages, seg_1seg);
    
    std::cout << "Segment 1 (14.6V - 15.7V):" << std::endl;
    std::cout << "  Slope:  " << std::setprecision(6) << seg1.slope << std::endl;
    std::cout << "  Offset: " << std::showpos << std::setprecision(6) << seg1.offset << std::noshowpos << std::endl;
    std::cout << "  Max Error: " << std::setprecision(3) << error1 << "V" << std::endl;
    
    std::cout << "\nSegment 2 (15.7V - 16.8V):" << std::endl;
    std::cout << "  Slope:  " << std::setprecision(6) << seg2.slope << std::endl;
    std::cout << "  Offset: " << std::showpos << std::setprecision(6) << seg2.offset << std::noshowpos << std::endl;
    std::cout << "  Max Error: " << std::setprecision(3) << error2 << "V" << std::endl;
    
    std::cout << "\nMidpoint (segment threshold): " << std::setprecision(2) << reference_voltages[1] << "V" << std::endl;
    std::cout << "Raw midpoint (for selection): " << std::setprecision(4) << raw_readings[1] << "V" << std::endl;
    
    std::cout << "\n--- Accuracy Comparison ---" << std::endl;
    std::cout << "1-Segment max error: " << std::setprecision(3) << max_error_1seg << "V" << std::endl;
    std::cout << "2-Segment max error: " << std::setprecision(3) << max_error_2seg << "V" << std::endl;
    std::cout << "Improvement: " << std::setprecision(3) << (max_error_1seg - max_error_2seg) << "V better!" << std::endl;
    
    // Save to JSON
    std::string filename = "/home/angelo/Projects/Drone-Fieldtest/ina219_calibration.json";
    std::ofstream file(filename);
    
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream date_ss;
    date_ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
    
    file << "{\n";
    file << "  \"slope1\": " << std::setprecision(16) << seg1.slope << ",\n";
    file << "  \"offset1\": " << std::setprecision(16) << seg1.offset << ",\n";
    file << "  \"slope2\": " << std::setprecision(16) << seg2.slope << ",\n";
    file << "  \"offset2\": " << std::setprecision(16) << seg2.offset << ",\n";
    file << "  \"midpoint\": " << std::setprecision(2) << reference_voltages[1] << ",\n";
    file << "  \"slope\": " << std::setprecision(16) << seg_1seg.slope << ",\n";
    file << "  \"offset\": " << std::setprecision(16) << seg_1seg.offset << ",\n";
    file << "  \"calibration_date\": \"" << date_ss.str() << " (2-segment)\",\n";
    file << "  \"raw_readings\": [" << std::setprecision(16) 
         << raw_readings[0] << ", " << raw_readings[1] << ", " << raw_readings[2] << "],\n";
    file << "  \"reference_voltages\": [" << std::fixed << std::setprecision(1) 
         << reference_voltages[0] << ", " << reference_voltages[1] << ", " << reference_voltages[2] << "],\n";
    file << std::defaultfloat;  // Reset to default format
    file << "  \"max_error_1segment\": " << std::setprecision(16) << max_error_1seg << ",\n";
    file << "  \"max_error_2segment\": " << std::setprecision(16) << max_error_2seg << ",\n";
    file << "  \"calibration_points\": [\n";
    file << "    {\"name\": \"Minimum (Critical)\", \"voltage\": 14.6, \"description\": \"3.65V per cell\"},\n";
    file << "    {\"name\": \"Middle (Midpoint)\", \"voltage\": 15.7, \"description\": \"3.925V per cell\"},\n";
    file << "    {\"name\": \"Maximum (Full)\", \"voltage\": 16.8, \"description\": \"4.2V per cell\"}\n";
    file << "  ]\n";
    file << "}\n";
    file.close();
    
    std::cout << "\n✓ Calibration saved to: " << filename << std::endl;
    std::cout << "\nRestart drone-recorder to apply new calibration:" << std::endl;
    std::cout << "  sudo systemctl restart drone-recorder\n" << std::endl;
    
    return 0;
}
