# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.14)

# Include directories for tests
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/common
)

# Create test executable for AudioPlayer
add_executable(audioplayer_test
    audioplayer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# Link against Google Test and required libraries
target_link_libraries(audioplayer_test
    PRIVATE
    GTest::gtest_main
    spdlog::spdlog
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register test with CTest
gtest_discover_tests(audioplayer_test)
