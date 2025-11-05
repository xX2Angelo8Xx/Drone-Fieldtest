cmake_minimum_required(VERSION 3.10)

# Live Streamer Application
add_executable(live_streamer
    main.cpp
)

# Link with common libraries
target_link_libraries(live_streamer
    zed_camera
    storage
    ${ZED_LIBRARIES}
    ${OpenCV_LIBRARIES}
    pthread
)

# Include directories
target_include_directories(live_streamer PRIVATE
    ${CMAKE_SOURCE_DIR}/common/hardware/zed_camera
    ${CMAKE_SOURCE_DIR}/common/streaming
    ${CMAKE_SOURCE_DIR}/common/storage
    ${ZED_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

# Compiler-specific options
target_compile_features(live_streamer PRIVATE cxx_std_17)

# Copy to build directory for easy access
set_target_properties(live_streamer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps/live_streamer/
)