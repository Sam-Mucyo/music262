# Required packages
find_package(Protobuf REQUIRED)

# Manually locate gRPC (because find_package(gRPC) is flaky with brew)
# set(GRPC_INCLUDE_DIR "/opt/homebrew/bin")
# set(GRPC_LIB_DIR "/opt/homebrew/lib")

# Client executable
add_executable(music_client
    main.cpp
    audioplayer.cpp
    client.cpp
    # include/client_handler.pb.cc
    # include/client_handler.grpc.pb.cc
)

# Include directories
target_include_directories(music_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Protobuf_INCLUDE_DIRS}
    # ${GRPC_INCLUDE_DIR}
    /opt/homebrew/bin
)
# # Add proto-generated files
# set(PROTO_SRCS
#     include/client_handler.pb.cc
#     include/client_handler.grpc.pb.cc
# )


# Link libraries
target_link_libraries(music_client PRIVATE
    common
    "-framework CoreAudio"
    "-framework AudioToolbox"
    ${Protobuf_LIBRARIES}
    # gRPC::grpc++
)

# target_link_directories(music_client PRIVATE /opt/homebrew/lib)

# /opt/homebrew/bin/protoc
# /opt/homebrew/bin/grpc_cpp_plugin