cmake_minimum_required(VERSION 3.22.1)
project(speechnotes)

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)

# Build the official ggml Android CPU variants as dynamically loadable modules.
# This lets the app benchmark the conservative ARMv8.0 baseline against the
# highest feature set that the device itself reports as safe to execute.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(GGML_BACKEND_DL ON CACHE BOOL "" FORCE)
set(GGML_CPU_ALL_VARIANTS ON CACHE BOOL "" FORCE)
set(GGML_CPU ON CACHE BOOL "" FORCE)

# ggml MODULE backends use CMAKE_RUNTIME_OUTPUT_DIRECTORY. Keep them next to
# the normal Android native libraries so AGP packages them into lib/arm64-v8a.
if(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/whispercpp/CMakeLists.txt")
    message(FATAL_ERROR "whisper.cpp source missing; CI must clone it into app/src/main/cpp/whispercpp")
endif()

add_subdirectory(whispercpp whisper-build EXCLUDE_FROM_ALL)

add_library(whisper_jni SHARED whisper_jni.cpp)
target_include_directories(whisper_jni PRIVATE
    whispercpp/include
    whispercpp/ggml/include)
target_link_libraries(whisper_jni PRIVATE whisper ggml android log dl)
target_compile_options(whisper_jni PRIVATE -O3)
