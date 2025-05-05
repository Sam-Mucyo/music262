cmake_minimum_required(VERSION 3.10)
project(music_client_gui)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt packages
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Tell CMake to run moc when necessary
set(CMAKE_AUTOMOC ON)
# Handle resources
set(CMAKE_AUTORCC ON)
# Handle UI files
set(CMAKE_AUTOUIC ON)

# Add include directories
include_directories(
    include
    ${CMAKE_SOURCE_DIR}/src/client/include
    ${CMAKE_SOURCE_DIR}/src/common/include
    ${CMAKE_SOURCE_DIR}/src/proto
)

# Explicitly include Qt dirs
include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})

# Collect header files for Qt MOC
set(HEADERS
    include/mainwindow.h
)

# Collect source files
set(SOURCES
    main.cpp
    mainwindow.cpp
    ${HEADERS}
)

# Create executable with explicit MOC setup
qt5_wrap_cpp(MOC_SOURCES ${HEADERS})

# Create executable
add_executable(music_client_gui ${SOURCES} ${MOC_SOURCES})

# Link libraries
target_link_libraries(music_client_gui
    PUBLIC
    Qt5::Core
    Qt5::Widgets
    music_client_lib
    ${COREAUDIO_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
)

# Set output directory
set_target_properties(music_client_gui
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")