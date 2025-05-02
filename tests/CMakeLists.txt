# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(music262_tests)

# Find GTest package
find_package(GTest REQUIRED)

# Common include directories for all tests
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

# Define common test libraries
set(COMMON_TEST_LIBRARIES
    gtest
    gmock
    gtest_main
    spdlog
)

# Define Mac-specific frameworks
set(MAC_FRAMEWORKS
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Function to add test with proper dependencies
function(add_module_test TEST_NAME TEST_SOURCE MODULE_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${MODULE_SOURCE})
    
    # Link against common test libraries
    target_link_libraries(${TEST_NAME} PRIVATE ${COMMON_TEST_LIBRARIES})
    
    # Add Mac frameworks if needed
    if(APPLE)
        target_link_libraries(${TEST_NAME} PRIVATE ${MAC_FRAMEWORKS})
    endif()
    
    # Register with CTest
    gtest_discover_tests(${TEST_NAME})
endfunction()

# Add subdirectories for each module's tests
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(server)
