# Server executable
add_executable(music_server
    main.cpp
)

# Include directories
target_include_directories(music_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
