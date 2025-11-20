cmake_minimum_required(VERSION 3.10)

add_library(battery_monitor STATIC
    battery_monitor.cpp
)

target_include_directories(battery_monitor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(battery_monitor
    pthread
    i2c
)
