# AudioPlayer Tests

This directory contains unit tests for the AudioPlayer class. The tests use Google Test and Google Mock to test the functionality of the AudioPlayer class.

## Test Files

- `audioplayer_mock_test.cpp`: Tests using mock AudioAPI and FileSystem classes
- `audioplayer_coreaudio_test.cpp`: Tests using mocked CoreAudio APIs
- `audioplayer_real_file_test.cpp`: Tests using the actual daydreamin.wav file
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

4. Run the tests:
   ```
   ctest
   ```

Or run individual test executables:
   ```
   ./audioplayer_mock_test
   ./audioplayer_coreaudio_test
   ./audioplayer_real_file_test
   ./audioplayer_callback_test
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

## Notes

- The tests require the daydreamin.wav file to be present in the sample_music directory.
- The tests use preprocessor macros to replace the real CoreAudio functions with mock implementations.
- The RenderCallback simulation test uses a global variable to access the AudioPlayer instance from the callback function. 