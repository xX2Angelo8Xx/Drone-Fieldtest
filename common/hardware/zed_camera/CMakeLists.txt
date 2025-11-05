add_library(zed_camera
    zed_recorder.cpp
)

target_include_directories(zed_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zed_camera
    sl_zed
    cuda
    stdc++fs
)

