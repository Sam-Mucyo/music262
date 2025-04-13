# Client executable
add_executable(music_client
    main.cpp
    audioplayer.cpp
)

target_include_directories(audioplayer PUBLIC
target_include_directories(audioplayer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(music_client PRIVATE
    common
    "-framework CoreAudio"
    "-framework AudioToolbox"
)
