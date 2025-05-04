# Create client library for use by both CLI and GUI clients
add_library(music_client_lib
    client.cpp
    audioplayer.cpp
    peer_network.cpp
    sync_clock.cpp
)

# Include directories for library
target_include_directories(music_client_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/proto
)

# Link libraries for library
target_link_libraries(music_client_lib PUBLIC
    common
    proto_lib
    "-framework CoreAudio"
    "-framework AudioToolbox"
)

# Client executable
add_executable(music_client 
    main.cpp
)

# Link the client to the library
target_link_libraries(music_client PRIVATE
    music_client_lib
)
