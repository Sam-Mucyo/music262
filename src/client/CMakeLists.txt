# Client executable
add_executable(music_client 
    main.cpp
    audioplayer.cpp
    client.cpp
)

# Include directories
target_include_directories(music_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link libraries
target_link_libraries(music_client PRIVATE
    common
    "-framework CoreAudio"
    "-framework AudioToolbox"
)