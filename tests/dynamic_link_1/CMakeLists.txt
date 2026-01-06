cmake_minimum_required(VERSION 3.15)

project(test_dynamic_link_1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_dynamic_link_1 main.cpp)

# Link dynamically to BLE_WinRT

# Ensure shared library build
set(BLE_WINRT_BUILD_SHARED ON CACHE BOOL "Build BLE_WinRT as shared library")

target_link_libraries(test_dynamic_link_1 PRIVATE BLE_WinRT)

target_include_directories(test_dynamic_link_1 PRIVATE
    ${CMAKE_SOURCE_DIR}/BLE_WinRT
)


set_target_properties(test_dynamic_link_1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy BLE_WinRT.dll to output directory after build
add_custom_command(TARGET test_dynamic_link_1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:BLE_WinRT>"
        "$<TARGET_FILE_DIR:test_dynamic_link_1>/BLE_WinRT.dll"
)
