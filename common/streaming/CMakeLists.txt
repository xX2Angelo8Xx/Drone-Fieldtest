cmake_minimum_required(VERSION 3.10)

# ZED Streaming Library
add_library(zed_streaming STATIC
    zed_streamer.cpp
)

# Link dependencies
target_link_libraries(zed_streaming
    ${ZED_LIBRARIES}
    ${OpenCV_LIBRARIES}
    pthread
)

# Include directories
target_include_directories(zed_streaming PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ZED_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# Compiler features
target_compile_features(zed_streaming PRIVATE cxx_std_17)