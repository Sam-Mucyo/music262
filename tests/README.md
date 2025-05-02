# Music262 Tests

This directory contains unit tests for the Music262 application. The tests use Google Test and Google Mock to test the functionality of various components.

## Test Structure

The test directory structure mirrors the src directory structure:

```
tests/
├── client/           # Tests for client components
│   ├── audioplayer_mock_test.cpp
│   ├── audioplayer_test.cpp
│   └── audioplayer_callback_test.cpp
├── common/           # Tests for common components
└── server/           # Tests for server components
```

### Client Tests

- `audioplayer_mock_test.cpp`: Tests using mock AudioAPI and FileSystem classes
- `audioplayer_coreaudio_test.cpp`: Tests using mocked CoreAudio APIs
- `audioplayer_callback_test.cpp`: Tests simulating the RenderCallback function

## Building and Running Tests

To build and run the tests, follow these steps:

1. Create a build directory:
   ```
   mkdir -p build
   cd build
   ```

2. Configure the project with CMake:
   ```
   cmake ..
   ```

3. Build the tests:
   ```
   make
   ```

4. Run all tests:
   ```
   ctest
   ```

Or run individual test executables from the bin directory:
   ```
   ./bin/audioplayer_mock_test
   ./bin/audioplayer_coreaudio_test
   ./bin/audioplayer_callback_test
   ```

## Test Coverage

The tests cover the following functionality:

- Loading WAV files (valid and invalid)
- Playing audio
- Pausing and resuming audio
- Stopping audio
- Loading a new file while playing
- Position tracking
- RenderCallback simulation

## Mocking Strategy

The tests use different mocking strategies:

1. **Mock AudioAPI and FileSystem**: The first test file mocks the AudioAPI and FileSystem classes, which are abstractions over the CoreAudio APIs.

2. **Mock CoreAudio APIs**: The second test file directly mocks the CoreAudio APIs by replacing the real functions with mock implementations.

3. **Real WAV File**: The third test file uses the actual daydreamin.wav file from the sample_music directory.

4. **RenderCallback Simulation**: The fourth test file simulates the RenderCallback function to test position advancement and end-of-file detection.

## Centralized Mocks Implementation

All CoreAudio mocks and test fixtures are centralized in the `tests/testlib` directory:

- `tests/testlib/include/coreaudio_mocks.h`: Declarations for all CoreAudio mock functions
- `tests/testlib/src/coreaudio_mocks.cpp`: Implementations of all CoreAudio mock functions
- `tests/testlib/include/test_utils.h`: Common test fixtures and utilities

This centralized approach provides several benefits:

1. **Reduced Duplication**: All mock declarations and implementations live in one place
2. **Consistent Behavior**: All tests use the same mock implementations
3. **Easier Maintenance**: Changes to mocks only need to be made in one place
4. **Dependency Injection Ready**: The global `g_player` pointer is managed centrally

## Notes

- The tests use preprocessor macros to replace the real CoreAudio functions with mock implementations, applied only in test builds via the `TESTING` definition.
- The RenderCallback simulation test uses a global variable to access the AudioPlayer instance from the callback function.
- The base test fixture `AudioPlayerTestBase` provides common setup/teardown and utility functions for all tests.

## Adding New Tests

To add a new test for a component:

1. Create a new test file in the appropriate subdirectory (client, common, or server)
2. Add the test to the corresponding CMakeLists.txt using the `add_module_test` function

Example for adding a new client test:

```cmake
# In tests/client/CMakeLists.txt
add_module_test(
    client_test                                           # Test executable name
    ${CMAKE_CURRENT_SOURCE_DIR}/client_test.cpp           # Test source file
    ${CMAKE_SOURCE_DIR}/src/client/client.cpp             # Source file being tested
)
```

The test structure is designed to scale automatically with new components and tests without requiring manual updates to the main CMakeLists.txt file.