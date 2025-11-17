// Depth Data Viewer
// Reads .depth files and displays them as colorized depth maps or saves as PNG

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <cmath>

namespace fs = std::filesystem;

struct DepthFileHeader {
    int width;
    int height;
    int frame_number;
};

bool readDepthFile(const std::string& filepath, DepthFileHeader& header, std::vector<float>& depth_data) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    // Read header
    file.read(reinterpret_cast<char*>(&header.width), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.height), sizeof(int));
    file.read(reinterpret_cast<char*>(&header.frame_number), sizeof(int));
    
    // Read depth data
    size_t pixel_count = header.width * header.height;
    depth_data.resize(pixel_count);
    file.read(reinterpret_cast<char*>(depth_data.data()), pixel_count * sizeof(float));
    
    file.close();
    return true;
}

cv::Mat depthToColorMap(const std::vector<float>& depth_data, int width, int height, float max_depth = 10.0f) {
    cv::Mat depth_image(height, width, CV_8UC3);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            float depth = depth_data[idx];
            
            // Handle invalid depth (NaN, inf, too far)
            if (std::isnan(depth) || std::isinf(depth) || depth > max_depth || depth <= 0.0f) {
                depth_image.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0); // Black for invalid
                continue;
            }
            
            // Normalize depth to 0-255 range
            int depth_value = static_cast<int>((depth / max_depth) * 255.0f);
            depth_value = std::min(255, std::max(0, depth_value));
            
            // Apply COLORMAP_JET
            cv::Mat single_pixel(1, 1, CV_8UC1, cv::Scalar(depth_value));
            cv::Mat colored_pixel;
            cv::applyColorMap(single_pixel, colored_pixel, cv::COLORMAP_JET);
            
            depth_image.at<cv::Vec3b>(y, x) = colored_pixel.at<cv::Vec3b>(0, 0);
        }
    }
    
    return depth_image;
}

void printUsage(const char* program_name) {
    std::cout << "Depth Data Viewer" << std::endl;
    std::cout << "Usage: " << program_name << " <command> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  view <depth_file>           - Display depth file interactively" << std::endl;
    std::cout << "  convert <depth_file> <out>  - Convert depth file to PNG" << std::endl;
    std::cout << "  batch <input_dir> <out_dir> - Convert all .depth files to PNG" << std::endl;
    std::cout << "  info <depth_file>           - Show file information" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --max-depth <meters>        - Maximum depth for visualization (default: 10.0)" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    float max_depth = 10.0f;
    
    // Parse options
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--max-depth" && i + 1 < argc) {
            max_depth = std::stof(argv[++i]);
        }
    }
    
    if (command == "view" && argc >= 3) {
        std::string filepath = argv[2];
        
        DepthFileHeader header;
        std::vector<float> depth_data;
        
        if (!readDepthFile(filepath, header, depth_data)) {
            return 1;
        }
        
        std::cout << "Frame " << header.frame_number << ": " 
                  << header.width << "x" << header.height << std::endl;
        
        cv::Mat depth_image = depthToColorMap(depth_data, header.width, header.height, max_depth);
        
        cv::imshow("Depth Viewer", depth_image);
        std::cout << "Press any key to exit..." << std::endl;
        cv::waitKey(0);
        
    } else if (command == "convert" && argc >= 4) {
        std::string input_file = argv[2];
        std::string output_file = argv[3];
        
        DepthFileHeader header;
        std::vector<float> depth_data;
        
        if (!readDepthFile(input_file, header, depth_data)) {
            return 1;
        }
        
        cv::Mat depth_image = depthToColorMap(depth_data, header.width, header.height, max_depth);
        
        if (cv::imwrite(output_file, depth_image)) {
            std::cout << "Saved: " << output_file << std::endl;
        } else {
            std::cerr << "Failed to save: " << output_file << std::endl;
            return 1;
        }
        
    } else if (command == "batch" && argc >= 4) {
        std::string input_dir = argv[2];
        std::string output_dir = argv[3];
        
        fs::create_directories(output_dir);
        
        int count = 0;
        for (const auto& entry : fs::directory_iterator(input_dir)) {
            if (entry.path().extension() == ".depth") {
                DepthFileHeader header;
                std::vector<float> depth_data;
                
                if (readDepthFile(entry.path().string(), header, depth_data)) {
                    cv::Mat depth_image = depthToColorMap(depth_data, header.width, header.height, max_depth);
                    
                    std::string out_filename = "depth_" + 
                                              std::to_string(header.frame_number) + ".png";
                    std::string out_path = output_dir + "/" + out_filename;
                    
                    if (cv::imwrite(out_path, depth_image)) {
                        count++;
                        if (count % 10 == 0) {
                            std::cout << "Processed " << count << " frames..." << std::endl;
                        }
                    }
                }
            }
        }
        
        std::cout << "Batch conversion complete: " << count << " frames" << std::endl;
        
    } else if (command == "info" && argc >= 3) {
        std::string filepath = argv[2];
        
        DepthFileHeader header;
        std::vector<float> depth_data;
        
        if (!readDepthFile(filepath, header, depth_data)) {
            return 1;
        }
        
        // Calculate statistics
        float min_depth = INFINITY;
        float max_depth_val = 0.0f;
        float sum = 0.0f;
        int valid_pixels = 0;
        
        for (float depth : depth_data) {
            if (!std::isnan(depth) && !std::isinf(depth) && depth > 0.0f) {
                min_depth = std::min(min_depth, depth);
                max_depth_val = std::max(max_depth_val, depth);
                sum += depth;
                valid_pixels++;
            }
        }
        
        float avg_depth = (valid_pixels > 0) ? (sum / valid_pixels) : 0.0f;
        
        std::cout << "=== Depth File Information ===" << std::endl;
        std::cout << "Frame Number: " << header.frame_number << std::endl;
        std::cout << "Resolution: " << header.width << "x" << header.height << std::endl;
        std::cout << "Total Pixels: " << depth_data.size() << std::endl;
        std::cout << "Valid Pixels: " << valid_pixels << " (" 
                  << (100.0f * valid_pixels / depth_data.size()) << "%)" << std::endl;
        std::cout << "Depth Range: " << min_depth << "m - " << max_depth_val << "m" << std::endl;
        std::cout << "Average Depth: " << avg_depth << "m" << std::endl;
        
    } else {
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
