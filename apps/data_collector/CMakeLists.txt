add_executable(data_collector
    main.cpp
)

target_link_libraries(data_collector
    lcd_display
    zed_camera
    storage
    utils
    sl_zed
    cuda
    nvbufsurftransform
    nvargus_socketclient
    /usr/lib/aarch64-linux-gnu/libopencv_core.so.4.5.4d
    /usr/lib/aarch64-linux-gnu/libopencv_imgproc.so.4.5.4d
    /usr/lib/aarch64-linux-gnu/libopencv_highgui.so.4.5.4d
)

# Installationsziel
install(TARGETS data_collector DESTINATION bin)

