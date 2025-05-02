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

# Add more client tests here as needed
# Example:
# add_module_test(
#     client_test
#     ${CMAKE_CURRENT_SOURCE_DIR}/client_test.cpp
#     ${CMAKE_SOURCE_DIR}/src/client/client.cpp
# )
