#include <sl/Camera.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <svo_file> [skip_frames]" << std::endl;
        std::cout << "Example: " << argv[0] << " video.svo2 5" << std::endl;
        std::cout << "Output: Creates organized folders in /home/angelo/Projects/Drone-Fieldtest/extracted_images/" << std::endl;
        return 1;
    }

    std::string svo_path = argv[1];
    int skip_frames = (argc > 2) ? std::stoi(argv[2]) : 10;
    
    // Extract SVO filename without extension for folder naming
    std::filesystem::path svo_file_path(svo_path);
    std::string svo_basename = svo_file_path.stem().string();
    
    // Create organized output directory structure
    std::string base_output_dir = "/home/angelo/Projects/Drone-Fieldtest/extracted_images/";
    std::string output_dir = base_output_dir + svo_basename + "/";
    
    // If SVO path contains flight directory info, extract that too
    std::string parent_dir = svo_file_path.parent_path().filename().string();
    if (parent_dir.find("flight_") == 0) {
        output_dir = base_output_dir + parent_dir + "_" + svo_basename + "/";
    }

    std::cout << "🖼️  SVO LEFT CAMERA EXTRACTOR" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << "SVO File: " << svo_path << std::endl;
    std::cout << "Output: " << output_dir << std::endl;
    std::cout << "Skip: Every " << skip_frames << " frame(s)" << std::endl << std::endl;

    // Create output directory
    std::filesystem::create_directories(output_dir);

    // Initialize ZED camera with SVO file
    sl::Camera zed;
    sl::InitParameters init_params;
    init_params.input.setFromSVOFile(svo_path.c_str());
    init_params.coordinate_units = sl::UNIT::METER;
    init_params.depth_mode = sl::DEPTH_MODE::NONE; // Don't need depth for image extraction

    if (zed.open(init_params) != sl::ERROR_CODE::SUCCESS) {
        std::cerr << "❌ Failed to open SVO file: " << svo_path << std::endl;
        return -1;
    }

    std::cout << "✅ SVO file opened successfully" << std::endl;

    // Get SVO information
    auto camera_info = zed.getCameraInformation();
    std::cout << "📊 Resolution: " << camera_info.camera_configuration.resolution.width 
              << "x" << camera_info.camera_configuration.resolution.height << std::endl;
    std::cout << "📊 FPS: " << camera_info.camera_configuration.fps << std::endl;

    // Prepare image containers
    sl::Mat zed_image;
    cv::Mat cv_image;
    
    int frame_count = 0;
    int extracted_count = 0;

    std::cout << std::endl << "🎬 Starting extraction..." << std::endl;

    while (zed.grab() == sl::ERROR_CODE::SUCCESS) {
        // Skip frames if needed
        if (frame_count % skip_frames == 0) {
            // Retrieve left camera image
            if (zed.retrieveImage(zed_image, sl::VIEW::LEFT) == sl::ERROR_CODE::SUCCESS) {
                // Convert ZED Mat to OpenCV Mat
                int cv_type = CV_8UC4; // BGRA format
                cv_image = cv::Mat(zed_image.getHeight(), zed_image.getWidth(), cv_type, 
                                 zed_image.getPtr<sl::uchar1>());

                // Convert BGRA to BGR for standard image format
                cv::Mat bgr_image;
                cv::cvtColor(cv_image, bgr_image, cv::COLOR_BGRA2BGR);

                // Save image
                std::string filename = output_dir + "frame_" + 
                                     std::string(6 - std::to_string(extracted_count).length(), '0') + 
                                     std::to_string(extracted_count) + ".jpg";
                
                if (cv::imwrite(filename, bgr_image)) {
                    extracted_count++;
                    if (extracted_count % 100 == 0) {
                        std::cout << "📸 Extracted " << extracted_count << " frames..." << std::endl;
                    }
                } else {
                    std::cerr << "❌ Failed to save: " << filename << std::endl;
                }
            }
        }
        frame_count++;
    }

    zed.close();

    std::cout << std::endl << "✅ EXTRACTION COMPLETE!" << std::endl;
    std::cout << "📊 Total frames processed: " << frame_count << std::endl;
    std::cout << "📊 Images extracted: " << extracted_count << std::endl;
    std::cout << "📁 Output directory: " << output_dir << std::endl;

    return 0;
}