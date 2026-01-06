cmake_minimum_required(VERSION 3.10)

project(BLE_WinRT LANGUAGES CXX)

# Option to build static or shared
option(BLE_WINRT_BUILD_SHARED "Build BLE_WinRT as a shared library" OFF)

set(BLE_WINRT_SRC
    BLE_WinRT.cpp
)
set(BLE_WINRT_HDR
    BLE_WinRT.h
)

set(BLE_WINRT_LIB_TYPE STATIC)
if(BLE_WINRT_BUILD_SHARED)
    set(BLE_WINRT_LIB_TYPE SHARED)
endif()

add_library(BLE_WinRT ${BLE_WINRT_LIB_TYPE} ${BLE_WINRT_SRC} ${BLE_WINRT_HDR})

# Set include directories for consumers
target_include_directories(BLE_WinRT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++ standard
set_target_properties(BLE_WinRT PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
