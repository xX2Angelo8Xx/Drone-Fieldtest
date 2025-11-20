/*
 * SVO LEFT CAMERA IMAGE EXTRACTION GUIDE
 * =====================================
 * 
 * Multiple approaches to extract left camera images from SVO files
 * for AI training data preparation
 */

#include <sl/Camera.hpp>
#include <opencv2/opencv.hpp>

// METHOD 1: ZED SDK Direct Extraction (Recommended)
class SVOImageExtractor {
private:
    sl::Camera zed;
    sl::Mat zed_image;
    cv::Mat cv_image;
    
public:
    bool openSVO(const std::string& svo_path) {
        sl::InitParameters init_params;
        init_params.input.setFromSVOFile(svo_path.c_str());
        init_params.coordinate_units = sl::UNIT::METER;
        
        if (zed.open(init_params) != sl::ERROR_CODE::SUCCESS) {
            std::cerr << "Failed to open SVO file: " << svo_path << std::endl;
            return false;
        }
        
        std::cout << "SVO opened: " << zed.getInitParameters().input.getInputType() << std::endl;
        return true;
    }
    
    void extractLeftImages(const std::string& output_dir) {
        int frame_count = 0;
        
        // Create output directory
        std::filesystem::create_directories(output_dir);
        
        while (zed.grab() == sl::ERROR_CODE::SUCCESS) {
            // Get left camera image
            zed.retrieveImage(zed_image, sl::VIEW::LEFT);
            
            // Convert to OpenCV format
            cv_image = slMat2cvMat(zed_image);
            
            // Save image
            std::string filename = output_dir + "/frame_" + 
                                 std::to_string(frame_count) + ".jpg";
            cv::imwrite(filename, cv_image);
            
            frame_count++;
            
            if (frame_count % 100 == 0) {
                std::cout << "Extracted " << frame_count << " frames" << std::endl;
            }
        }
        
        std::cout << "Total frames extracted: " << frame_count << std::endl;
    }
    
private:
    cv::Mat slMat2cvMat(sl::Mat& input) {
        int cv_type = -1;
        switch (input.getDataType()) {
            case sl::MAT_TYPE::F32_C1: cv_type = CV_32FC1; break;
            case sl::MAT_TYPE::F32_C2: cv_type = CV_32FC2; break;
            case sl::MAT_TYPE::F32_C3: cv_type = CV_32FC3; break;
            case sl::MAT_TYPE::F32_C4: cv_type = CV_32FC4; break;
            case sl::MAT_TYPE::U8_C1: cv_type = CV_8UC1; break;
            case sl::MAT_TYPE::U8_C2: cv_type = CV_8UC2; break;
            case sl::MAT_TYPE::U8_C3: cv_type = CV_8UC3; break;
            case sl::MAT_TYPE::U8_C4: cv_type = CV_8UC4; break;
            default: break;
        }
        return cv::Mat(input.getHeight(), input.getWidth(), cv_type, input.getPtr<sl::uchar1>());
    }
};

// USAGE EXAMPLE:
int main() {
    SVOImageExtractor extractor;
    
    // Open your SVO file
    if (extractor.openSVO("/media/angelo/DRONE_DATA/flight_20251027_132504/video.svo2")) {
        // Extract all left camera images
        extractor.extractLeftImages("/home/angelo/training_images/");
    }
    
    return 0;
}