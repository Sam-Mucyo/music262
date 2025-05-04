# Client executable
add_executable(music_client 
    main.cpp
    client.cpp
    audioplayer.cpp
    peer_network.cpp
    peer_service.cpp
)

# Include directories
target_include_directories(music_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/proto
    ${CMAKE_CURRENT_BINARY_DIR}/../proto 
)

# Link libraries
target_link_libraries(music_client PRIVATE
    common
    proto_lib
    "-framework CoreAudio"
    "-framework AudioToolbox"
)
