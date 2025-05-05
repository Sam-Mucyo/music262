add_subdirectory(proto)
add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

# Only build GUI client if enabled
if(BUILD_GUI)
    add_subdirectory(client_gui)
endif()
