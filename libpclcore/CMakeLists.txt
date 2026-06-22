cmake_minimum_required(VERSION 3.16)

# ── libpclcore — 纯 C++ 核心库，零 GTK/GLib 依赖 ─────────────────────────

add_library(pclcore_objs OBJECT
    src/core/Log.cpp
    src/local/Instance.cpp
    src/local/DownloadTask.cpp
    src/network/Dispatcher.cpp
)

target_include_directories(pclcore_objs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(MSVC)
    target_compile_options(pclcore_objs PRIVATE /W4)
else()
    target_compile_options(pclcore_objs PRIVATE -Wall -Wextra -Wpedantic)
endif()
