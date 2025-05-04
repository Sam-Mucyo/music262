# Client module tests
cmake_minimum_required(VERSION 3.10)

# AudioPlayer tests
add_module_test(
    audioplayer_mock_test
    ${CMAKE_CURRENT_SOURCE_DIR}/audioplayer_mock_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

add_module_test(
    audioplayer_coreaudio_test
    ${CMAKE_CURRENT_SOURCE_DIR}/audioplayer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

add_module_test(
    audioplayer_callback_test
    ${CMAKE_CURRENT_SOURCE_DIR}/audioplayer_callback_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp
)

# SyncClock tests
add_module_test(
    sync_clock_test
    ${CMAKE_CURRENT_SOURCE_DIR}/sync_clock_test.cpp
    ${CMAKE_SOURCE_DIR}/src/client/sync_clock.cpp
)

# Add include paths for the SyncClock test
target_include_directories(sync_clock_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/client/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_BINARY_DIR}/src/proto
)

# Link against needed libraries
target_link_libraries(sync_clock_test PRIVATE
    common
    proto_lib
)

# Client tests
add_module_test(
    client_test
    ${CMAKE_CURRENT_SOURCE_DIR}/client_test.cpp
    "${CMAKE_SOURCE_DIR}/src/client/client.cpp;${CMAKE_SOURCE_DIR}/src/client/audioplayer.cpp;${CMAKE_SOURCE_DIR}/src/client/peer_network.cpp;${CMAKE_SOURCE_DIR}/src/client/sync_clock.cpp;${CMAKE_SOURCE_DIR}/src/client/peer_service_grpc.cpp"
)

# Add include paths for the Client test
target_include_directories(client_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/client/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_BINARY_DIR}/src/proto
)

# Link against needed libraries
target_link_libraries(client_test PRIVATE
    common
    proto_lib
)

# If on Mac, link against CoreAudio frameworks
if(APPLE)
    target_link_libraries(client_test PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()
