cmake_minimum_required(VERSION 3.10)
project(SVOExtractor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ZED SDK
find_package(ZED 4 REQUIRED)

# Include directories
include_directories(${ZED_INCLUDE_DIRS})
include_directories("/usr/include/opencv4")

# Create executable
add_executable(svo_extractor svo_extractor.cpp)

# Link libraries using the actual Ubuntu package libraries
target_link_libraries(svo_extractor 
    ${ZED_LIBRARIES}
    /usr/lib/aarch64-linux-gnu/libopencv_core.so.4.5.4d
    /usr/lib/aarch64-linux-gnu/libopencv_imgproc.so.4.5.4d
    /usr/lib/aarch64-linux-gnu/libopencv_imgcodecs.so.4.5.4d
    /usr/lib/aarch64-linux-gnu/libopencv_highgui.so.4.5.4d
)

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(svo_extractor PRIVATE -Wno-deprecated-declarations)
endif()