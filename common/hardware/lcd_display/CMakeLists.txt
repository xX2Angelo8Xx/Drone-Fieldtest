add_library(lcd_display
    lcd_handler.cpp
    lcd_i2c.cpp
)

target_include_directories(lcd_display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

