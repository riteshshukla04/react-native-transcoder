project(NitroTranscoder)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME NitroTranscoder)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE shared_files RELATIVE ${CMAKE_SOURCE_DIR}
     "../cpp/**.cpp"
)
file(GLOB_RECURSE android_files RELATIVE ${CMAKE_SOURCE_DIR}
     "src/main/cpp/**.cpp"
)

add_library(${PACKAGE_NAME} SHARED
            ${shared_files}
            ${android_files}
)

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/NitroTranscoder+autolinking.cmake)

set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/../dependencies/prebuilt/android/${ANDROID_ABI}")
if (NOT EXISTS "${FFMPEG_DIR}/lib/libavcodec.so")
  message(FATAL_ERROR "No FFmpeg build for ${ANDROID_ABI}. Run scripts/build-ffmpeg-android.sh first.")
endif()

foreach(ffmpeg_lib avcodec avformat avutil swresample)
  add_library(${ffmpeg_lib} SHARED IMPORTED)
  set_target_properties(${ffmpeg_lib} PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_DIR}/lib/lib${ffmpeg_lib}.so")
endforeach()

include_directories(
        "src/main/cpp"
        "../cpp"
        "../cpp/hybrid"
        "../cpp/engine/include"
        "${FFMPEG_DIR}/include"
)

if (DEFINED TRANSCODER_VERSION)
  target_compile_definitions(${PACKAGE_NAME} PRIVATE TRANSCODER_VERSION="${TRANSCODER_VERSION}")
endif()

find_library(LOG_LIB log)

target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android
        avformat
        avcodec
        swresample
        avutil
)
