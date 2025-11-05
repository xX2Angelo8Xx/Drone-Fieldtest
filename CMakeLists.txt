cmake_minimum_required(VERSION 3.10)
project(DroneFieldtest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Globale Includes
include_directories("/usr/include/opencv4" "/usr/local/cuda-12.6/include" "/usr/local/zed/include")
link_directories("/usr/lib/aarch64-linux-gnu" "/usr/lib/aarch64-linux-gnu/nvidia" "/usr/local/zed/lib")

# Projektweite Include-Pfade für einfachere Header-Einbindung
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/common/hardware/lcd_display
    ${CMAKE_CURRENT_SOURCE_DIR}/common/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/common/hardware/zed_camera
    ${CMAKE_CURRENT_SOURCE_DIR}/common/utils
)

# Gemeinsame Bibliotheken
add_subdirectory(common/hardware/lcd_display)
add_subdirectory(common/hardware/zed_camera)
add_subdirectory(common/storage)
add_subdirectory(common/utils)

# Apps
add_subdirectory(apps/data_collector)
add_subdirectory(apps/performance_test)
add_subdirectory(apps/smart_recorder)
add_subdirectory(apps/zed_cli_recorder)
add_subdirectory(apps/drone_web_controller)

# Tools
add_subdirectory(tools)

