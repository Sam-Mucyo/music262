cmake_minimum_required(VERSION 3.15)
project(music262_client)

# Core Audio frameworks (macOS only)
find_library(AUDIO_TOOLBOX AudioToolbox)
find_library(CORE_AUDIO CoreAudio)
find_library(AVFAUDIO AVFAudio)

# Library target
add_library(audioplayer
    src/audioplayer.cpp
)

target_include_directories(audioplayer PUBLIC
target_include_directories(audioplayer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(audioplayer PRIVATE
    ${AUDIO_TOOLBOX}
    ${CORE_AUDIO}
    ${AVFAUDIO}
)

# Main executable
add_executable(music262_client
    src/main.cpp
)

target_link_libraries(music262_client PRIVATE audioplayer)

# Tests
if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    add_executable(audioplayer_test
        ${CMAKE_SOURCE_DIR}/tests/unit/audioplayer.cpp
    )
    target_link_libraries(audioplayer_test PRIVATE 
        audioplayer 
        GTest::GTest 
        GTest::Main
    )
    add_test(NAME audioplayer_test COMMAND audioplayer_test)
endif()