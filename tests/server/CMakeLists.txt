# Server module tests
cmake_minimum_required(VERSION 3.10)

# Add test for AudioServer class
add_module_test(
    audio_server_test
    ${CMAKE_CURRENT_SOURCE_DIR}/audio_server_test.cpp
    ${CMAKE_SOURCE_DIR}/src/server/audio_server.cpp
)

# Link against additional libraries needed for the test
target_link_libraries(audio_server_test PRIVATE
    common
)
