cmake_minimum_required(VERSION 3.10)

project(BLE_WinRT_Context LANGUAGES CXX)

# Option to build static or shared
option(BLE_WINRT_CONTEXT_BUILD_SHARED "Build BLE_WinRT_Context as a shared library" OFF)

set(BLE_WinRT_Context_SRC
    BLE_WinRT_Context.cpp
)
set(BLE_WinRT_Context_HDR
    BLE_WinRT_Context.h
)

set(BLE_WinRT_Context_LIB_TYPE STATIC)
if(BLE_WINRT_CONTEXT_BUILD_SHARED)
    set(BLE_WinRT_Context_LIB_TYPE SHARED)
endif()

add_library(BLE_WinRT_Context ${BLE_WinRT_Context_LIB_TYPE} ${BLE_WinRT_Context_SRC} ${BLE_WinRT_Context_HDR})

if(BLE_WINRT_CONTEXT_BUILD_SHARED)
    target_compile_definitions(BLE_WinRT_Context PRIVATE BLE_WINRT_CONTEXT_DLL_EXPORTS)
endif()
# Set include directories for consumers
target_include_directories(BLE_WinRT_Context PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++ standard
set_target_properties(BLE_WinRT_Context PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
