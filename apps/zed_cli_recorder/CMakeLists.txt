cmake_minimum_required(VERSION 3.10)

# ZED CLI Recorder - Uses ZED Explorer backend for corruption-free recording
add_executable(zed_cli_recorder main.cpp)

# Link mit Hardware-Abstraktion (Storage und LCD)
target_link_libraries(zed_cli_recorder
    storage
    lcd_display
)

# Standard C++17
set_target_properties(zed_cli_recorder PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)