# Test library with shared test utilities and mocks
cmake_minimum_required(VERSION 3.10)

# Add include directories for test utilities
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Create a library for shared test utilities and mocks
add_library(test_utils STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/coreaudio_mocks.cpp
)

# Add TESTING definition to enable mocks
target_compile_definitions(test_utils PUBLIC TESTING=1)
