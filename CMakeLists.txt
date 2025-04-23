cmake_minimum_required(VERSION 3.14)
project(music262 VERSION 0.1.0)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_DOCS "Build documentation" ON)
option(ENABLE_FORMATTING "Enable code formatting" ON)

# Add FetchContent for dependency management
include(FetchContent)

# Fetch spdlog for logging
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)

# Fetch GoogleTest for unit testing
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    include(GoogleTest)
endif()

FetchContent_MakeAvailable(spdlog)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Add subdirectories
add_subdirectory(src)

# Add tests directory if tests are enabled
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Add documentation target if docs are enabled
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        message(STATUS "Doxygen found, documentation generation enabled")
        
        # Make sure Doxyfile exists
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile)
            message(WARNING "Doxyfile not found at ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile")
            message(STATUS "Run 'doxygen -g docs/Doxyfile' to generate a default configuration")
        else()
            # Add a custom target to run Doxygen
            add_custom_target(docs
                COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM
            )
            
            message(STATUS "Documentation will be generated in ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen/html")
        endif()
    else()
        message(WARNING "Doxygen not found, documentation generation disabled")
    endif()
endif()

# Add formatting targets if formatting is enabled
if(ENABLE_FORMATTING)
    find_program(CLANG_FORMAT "clang-format")
    if(CLANG_FORMAT)
        message(STATUS "clang-format found, formatting enabled")
        
        # Format check target - validates that all code is properly formatted
        add_custom_target(format-check
            COMMAND ${CMAKE_COMMAND} -E echo "Checking formatting..."
            COMMAND find ${CMAKE_CURRENT_SOURCE_DIR}/src -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs ${CLANG_FORMAT} --dry-run --Werror
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Checking code formatting compliance"
            VERBATIM
        )
        
        # Format target - actually formats all code according to style
        add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -E echo "Formatting code..."
            COMMAND find ${CMAKE_CURRENT_SOURCE_DIR}/src -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs ${CLANG_FORMAT} -i
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Formatting code according to style guide"
            VERBATIM
        )
        
        # Add pre-build hook to ensure code is properly formatted
        add_custom_target(format-all ALL
            DEPENDS format
            COMMENT "Ensuring all code is properly formatted"
        )
    else()
        message(WARNING "clang-format not found, formatting disabled")
    endif()
endif()
