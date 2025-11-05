add_executable(smart_recorder
    main.cpp
)

target_link_libraries(smart_recorder
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
install(TARGETS smart_recorder DESTINATION bin)