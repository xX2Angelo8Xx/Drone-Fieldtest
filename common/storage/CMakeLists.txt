add_library(storage
    storage.cpp
)

target_include_directories(storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link filesystem library for older compilers
target_link_libraries(storage stdc++fs)

