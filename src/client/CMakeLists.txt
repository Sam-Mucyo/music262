# Client executable
add_executable(music_client 
    main.cpp
    client.cpp
    audioplayer.cpp
    peer_network.cpp
    sync_clock.cpp
    audio_service_grpc.cpp
    peer_service_grpc.cpp
    client_factory.cpp
)

# Include directories
target_include_directories(music_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/proto
)

# Link libraries
target_link_libraries(music_client PRIVATE
    common
    proto_lib
    "-framework CoreAudio"
    "-framework AudioToolbox"
)
