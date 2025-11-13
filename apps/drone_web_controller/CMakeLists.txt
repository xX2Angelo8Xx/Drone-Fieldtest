cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(Threads REQUIRED)

# Create executable
add_executable(drone_web_controller
    main.cpp
    drone_web_controller.cpp
)

# Include directories
target_include_directories(drone_web_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/common/hardware/zed_camera
    ${CMAKE_SOURCE_DIR}/common/hardware/lcd_display
    ${CMAKE_SOURCE_DIR}/common/storage
    ${CMAKE_SOURCE_DIR}/common/utils
    ${CMAKE_SOURCE_DIR}/common/networking
)

# Link libraries
target_link_libraries(drone_web_controller PRIVATE
    zed_camera
    lcd_display
    storage
    safe_hotspot_manager
    Threads::Threads
)

# Compiler flags
target_compile_features(drone_web_controller PRIVATE cxx_std_17)