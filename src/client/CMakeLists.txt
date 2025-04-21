# # Required packages
# find_package(Protobuf REQUIRED)

# # Client executable
# add_executable(music_client
#     main.cpp
#     audioplayer.cpp
#     client.cpp
# )

# # Include directories
# target_include_directories(music_client PRIVATE 
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${Protobuf_INCLUDE_DIRS}
#     /opt/homebrew/bin
# )

# # Link libraries
# target_link_libraries(music_client PRIVATE
#     common
#     "-framework CoreAudio"
#     "-framework AudioToolbox"
#     ${Protobuf_LIBRARIES}
# )

cmake_minimum_required(VERSION 3.10)
project(music_client)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find curl for HTTP requests
find_package(CURL REQUIRED)

# Add homebrew paths for macOS
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# Define library paths directly
set(PROTOBUF_LIBRARY "/opt/homebrew/lib/libprotobuf.dylib")
set(FLATBUFFERS_LIBRARY "/opt/homebrew/lib/libgrpc++.dylib")

# Generate C++ code from .proto file
# get_filename_component(proto_file "src/client/include/client_handler.proto" ABSOLUTE)
get_filename_component(proto_file "include/client_handler.proto" ABSOLUTE)
get_filename_component(proto_dir "${proto_file}" PATH)

# Generate protobuf and gRPC C++ code
set(proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/client_handler.pb.cc")
set(proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/client_handler.pb.h")
set(grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/client_handler.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/client_handler.grpc.pb.h")

add_custom_command(
    OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
    COMMAND protoc
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${proto_dir}" 
        --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
        "${proto_file}"
    DEPENDS "${proto_file}"
)

# Include generated *.pb.h files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Client executable
add_executable(music_client 
    main.cpp
    audioplayer.cpp
    client.cpp
    # src/peer_node/logical_clock.cpp
    # src/peer_node/grpc_service.cpp
    ${proto_srcs} ${grpc_srcs}
)

# Link libraries to the peer node
target_link_libraries(music_client
    common
    ${PROTOBUF_LIBRARY}
    ${GRPCPP_LIBRARY}
    ${CURL_LIBRARIES}
    grpc
    gpr
    address_sorting
    upb
    # Abseil libraries with version numbers
    /opt/homebrew/lib/libabsl_base.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_cord.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_cord_internal.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_cordz_info.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_cordz_functions.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_cordz_handle.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_strings.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_synchronization.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_time.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_raw_logging_internal.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_throw_delegate.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_time_zone.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_civil_time.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_check_op.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_message.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_conditions.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_format.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_globals.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_proto.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_nullguard.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_internal_log_sink_set.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_globals.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_entry.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_severity.2407.0.0.dylib
    /opt/homebrew/lib/libabsl_log_sink.2407.0.0.dylib
)

# Link audio libraries based on platform
if(APPLE)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    target_link_libraries(music_client
        ${COREAUDIO_LIBRARY}
        ${AUDIOUNIT_LIBRARY}
        ${AUDIOTOOLBOX_LIBRARY}
    )
endif()

# Include directories
target_include_directories(music_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)