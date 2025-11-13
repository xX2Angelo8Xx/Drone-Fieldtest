# Find OpenCV libraries manually (JetPack ships with OpenCV 4.5.4)
set(OpenCV_LIBS
    /usr/lib/aarch64-linux-gnu/libopencv_core.so.4.5d
    /usr/lib/aarch64-linux-gnu/libopencv_imgcodecs.so.4.5d
    /usr/lib/aarch64-linux-gnu/libopencv_imgproc.so.4.5d
)

add_library(zed_camera
    zed_recorder.cpp
    raw_frame_recorder.cpp
)

target_include_directories(zed_camera PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    /usr/include/opencv4
)

target_link_libraries(zed_camera
    sl_zed
    cuda
    stdc++fs
    ${OpenCV_LIBS}
)

