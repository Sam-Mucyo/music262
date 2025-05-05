# Proto library

# Get the absolute path to the protoc compiler
set(PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
set(GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Set the output directory for generated files
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Get all proto files
file(GLOB PROTO_FILES "${PROTO_SRC_DIR}/*.proto")

# Generate C++ code from proto files
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_SRCS "${PROTO_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDRS "${PROTO_BINARY_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_SRCS "${PROTO_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDRS "${PROTO_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
        COMMAND ${PROTOC_EXECUTABLE}
        ARGS --grpc_out=${PROTO_BINARY_DIR}
             --cpp_out=${PROTO_BINARY_DIR}
             -I${PROTO_SRC_DIR}
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Running C++ protocol buffer compiler on ${PROTO_FILE}"
        VERBATIM
    )
    
    list(APPEND PROTO_GENERATED_SRCS ${PROTO_SRCS} ${GRPC_SRCS})
    list(APPEND PROTO_GENERATED_HDRS ${PROTO_HDRS} ${GRPC_HDRS})
endforeach()

# Create a library with the generated source files
add_library(proto_lib STATIC ${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS})
target_link_libraries(proto_lib PUBLIC
    ${_PROTOBUF_LIBPROTOBUF}
    gRPC::grpc++
    gRPC::grpc++_reflection
)
target_include_directories(proto_lib PUBLIC
    ${PROTO_BINARY_DIR}
)

# Make sure the generated headers are included
set_target_properties(proto_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
