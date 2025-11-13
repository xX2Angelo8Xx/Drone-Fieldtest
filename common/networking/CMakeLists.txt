cmake_minimum_required(VERSION 3.10)

# SafeHotspotManager library
add_library(safe_hotspot_manager
    safe_hotspot_manager.cpp
)

target_include_directories(safe_hotspot_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required libraries (none for now, only standard library)
target_link_libraries(safe_hotspot_manager
    stdc++fs  # For filesystem operations if needed
)
