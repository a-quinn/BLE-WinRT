cmake_minimum_required(VERSION 3.15)

project(test_static_link_4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    main.cpp
    bluetooth.cpp
)

set(HEADERS
    bluetooth.h
)

add_executable(test_static_link_4 ${SOURCES} ${HEADERS})

# Link with BLE_WinRT_Context library
target_link_libraries(test_static_link_4 PRIVATE BLE_WinRT_Context)

# Include BLE_WinRT_Context headers
target_include_directories(test_static_link_4 PRIVATE
    ${CMAKE_SOURCE_DIR}/BLE_WinRT_Context
)

# Windows specific settings
if (WIN32)
    target_compile_definitions(test_static_link_4 PRIVATE UNICODE _UNICODE)
    target_link_libraries(test_static_link_4 PRIVATE windowsapp)
endif()

set_target_properties(test_static_link_4 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
