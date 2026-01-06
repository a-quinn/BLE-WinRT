cmake_minimum_required(VERSION 3.15)

project(test_static_link_0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_static_link_0 main.cpp)

target_link_libraries(test_static_link_0 PRIVATE BLE_WinRT)

target_include_directories(test_static_link_0 PRIVATE
    ${CMAKE_SOURCE_DIR}/BLE_WinRT
)

set_target_properties(test_static_link_0 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
