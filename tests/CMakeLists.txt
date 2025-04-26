# Tests CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(audioplayer_tests)

# Find GTest package
find_package(GTest REQUIRED)

# Include directories for tests
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/common
)

# Create test executable for AudioPlayer with mock tests
add_executable(audioplayer_mock_test
    audioplayer_mock_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# Link against Google Test and required libraries
target_link_libraries(audioplayer_mock_test
    PRIVATE
    gtest
    gmock
    gtest_main
    spdlog
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register test with CTest
gtest_discover_tests(audioplayer_mock_test)

# Create test executable for AudioPlayer with CoreAudio mocks
add_executable(audioplayer_coreaudio_test
    audioplayer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# Link against Google Test and required libraries
target_link_libraries(audioplayer_coreaudio_test
    PRIVATE
    gtest
    gmock
    gtest_main
    spdlog
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register test with CTest
gtest_discover_tests(audioplayer_coreaudio_test)

# Create test executable for AudioPlayer with real WAV file
add_executable(audioplayer_real_file_test
    audioplayer_real_file_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# Link against Google Test and required libraries
target_link_libraries(audioplayer_real_file_test
    PRIVATE
    gtest
    gmock
    gtest_main
    spdlog
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register test with CTest
gtest_discover_tests(audioplayer_real_file_test)

# Create test executable for AudioPlayer with callback simulation
add_executable(audioplayer_callback_test
    audioplayer_callback_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# Link against Google Test and required libraries
target_link_libraries(audioplayer_callback_test
    PRIVATE
    gtest
    gmock
    gtest_main
    spdlog
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register test with CTest
gtest_discover_tests(audioplayer_callback_test)
