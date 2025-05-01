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

# Create test executable for PeerNetwork
add_executable(peer_network_test
    test_peer_network.cpp
    ${CMAKE_SOURCE_DIR}/src/client/peer_network.cpp
    ${CMAKE_SOURCE_DIR}/src/client/client.cpp
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

target_link_libraries(peer_network_test
    PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    spdlog::spdlog
    proto_lib
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Register tests with CTest
gtest_discover_tests(audioplayer_test)
gtest_discover_tests(peer_network_test)
