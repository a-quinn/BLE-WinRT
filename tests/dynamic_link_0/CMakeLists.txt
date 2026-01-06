cmake_minimum_required(VERSION 3.15)

project(test_dynamic_link_0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_dynamic_link_0 main.cpp)

set_target_properties(test_dynamic_link_0 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy BLE_WinRT.dll to output directory after build
add_custom_command(TARGET test_dynamic_link_0 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:BLE_WinRT>"
        "$<TARGET_FILE_DIR:test_dynamic_link_0>/BLE_WinRT.dll"
)
