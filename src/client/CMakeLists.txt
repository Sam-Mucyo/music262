# Client executable
add_executable(music_client
    main.cpp
)

# Include directories
target_include_directories(music_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
