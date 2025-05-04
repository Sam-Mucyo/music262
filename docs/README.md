# Music262 Documentation

This directory contains documentation for the Music262 project.

## User Guides

- [How To Run](../HOW_TO_RUN.md) - Instructions for building and running the application
- [GUI Client User Guide](gui_client.md) - Guide for the graphical user interface client
- [Peer-to-Peer Network](peer_to_peer.md) - Documentation on the peer network architecture
- [Peer Broadcast Fix](peer_broadcast_fix.md) - Information on peer broadcasting improvements

## Doxygen Documentation

The project uses Doxygen to automatically generate API documentation from source code. The generated documentation will be available in the `docs/doxygen/html` directory.

### How to Build Documentation

To build the documentation:

1. Make sure you have Doxygen installed
   ```
   which doxygen
   ```

2. Build the documentation using CMake
   ```
   mkdir -p build && cd build
   cmake ..
   make docs
   ```

3. Open the generated documentation
   ```
   open ../docs/doxygen/html/index.html
   ```

### Documentation Style Guide

When adding documentation comments to code, please follow these guidelines:

- Use `/**` style comments for Doxygen documentation
- Document all public classes, methods, and functions
- Use @brief for short descriptions
- Use @param to document parameters
- Use @return to document return values
- Use @see to reference related items
- Document code examples with @code and @endcode

Example:

```cpp
/**
 * @brief A brief description of the function
 *
 * A more detailed description if needed.
 *
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 *
 * @see RelatedClass
 */
return_type function_name(param_type param1, param_type param2);
```

For more information on Doxygen comment syntax, see [Doxygen documentation](https://www.doxygen.nl/manual/docblocks.html).