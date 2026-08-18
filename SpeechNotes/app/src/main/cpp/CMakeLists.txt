cmake_minimum_required(VERSION 3.22.1)
project(speechnotes)

set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)

# Build the official ggml Android CPU variants as dynamically loadable modules.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(GGML_BACKEND_DL ON CACHE BOOL "" FORCE)
set(GGML_CPU_ALL_VARIANTS ON CACHE BOOL "" FORCE)
set(GGML_CPU ON CACHE BOOL "" FORCE)

# Experimental Vulkan backend for the reproducible ARM-Best vs Vulkan suite.
# CI stages Khronos Vulkan + SPIR-V headers under this source tree so the
# Android cross-compiler can resolve both <vulkan/vulkan.hpp> and
# <spirv/unified1/spirv.hpp> without adding the host /usr/include tree.
set(GGML_VULKAN ON CACHE BOOL "" FORCE)
set(SN_KHRONOS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan-headers/include")
if(EXISTS "${SN_KHRONOS_HEADERS}/vulkan/vulkan.hpp")
    set(Vulkan_INCLUDE_DIR "${SN_KHRONOS_HEADERS}" CACHE PATH "" FORCE)
endif()
if(EXISTS "/usr/bin/glslc")
    set(Vulkan_GLSLC_EXECUTABLE "/usr/bin/glslc" CACHE FILEPATH "" FORCE)
endif()
if(EXISTS "/usr/share/cmake/SPIRV-Headers")
    set(SPIRV-Headers_DIR "/usr/share/cmake/SPIRV-Headers" CACHE PATH "" FORCE)
endif()

# ggml MODULE backends use CMAKE_RUNTIME_OUTPUT_DIRECTORY. Keep them next to
# Android native libraries so AGP packages them into lib/arm64-v8a.
if(CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/whispercpp/CMakeLists.txt")
    message(FATAL_ERROR "whisper.cpp source missing; CI must clone it into app/src/main/cpp/whispercpp")
endif()

add_subdirectory(whispercpp whisper-build EXCLUDE_FROM_ALL)

# ggml-vulkan is created inside whisper.cpp. Attach only the staged Khronos
# header root to the Android target; it contains vulkan/ and spirv/.
if(TARGET ggml-vulkan
   AND EXISTS "${SN_KHRONOS_HEADERS}/vulkan/vulkan.hpp"
   AND EXISTS "${SN_KHRONOS_HEADERS}/spirv/unified1/spirv.hpp")
    target_include_directories(ggml-vulkan SYSTEM PRIVATE "${SN_KHRONOS_HEADERS}")
endif()

add_library(whisper_jni SHARED whisper_jni.cpp)
target_include_directories(whisper_jni PRIVATE
    whispercpp/include
    whispercpp/ggml/include)
target_link_libraries(whisper_jni PRIVATE whisper ggml android log dl)
target_compile_options(whisper_jni PRIVATE -O3)
