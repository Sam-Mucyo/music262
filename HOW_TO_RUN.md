# How to Build and Run the Application

This updated version of the application has support for macOS only, with streaming audio from server to client.

## Build

### Prerequisites

- C++17 compatible compiler (GCC, Clang, MSVC, etc.)
- Homebrew (recommended for installing dependencies on macOS)
- CMake 3.14 or higher
- Protobuf library
- gRPC library
- spdlog library

### Installing Dependencies (macOS)

You can install the required dependencies using Homebrew:

```bash
brew install cmake protobuf grpc spdlog
```

### Building the Application

From root directory, run:
```bash
cmake -B build -S . && cmake --build build
```

The build process will automatically format the code according to style guidelines.

After successful build, the executables will be in the `build/bin` directory.

### Developer setup (optional)

If you use clangd/VS Code/Neovim for C++ language features and hate those red-underlines, give your editor the exact compile flags:
```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json .   # makes clangd pick it up from the repo root


## Running the Application

### Server

From the build directory:

```bash
./bin/music_server
```

Server commands:
- `status` - Show server status (available songs, active clients, etc.)
- `help` - Show help
- `exit` - Exit the server

You can also specify port and audio directory:

```bash
./bin/music_server --port 50051 --audio_dir ../sample_music
```

### Client

From the build directory:

```bash
./bin/music_client
```

You can specify the server address:

```bash
./bin/music_client --server localhost:50051
```

Client commands:
- `playlist` - Get the list of songs available on the server
- `play <song_name>` - Load a song from the server and play it
- `pause` - Pause the currently playing song
- `resume` - Resume the paused song
- `stop` - Stop playback and reset position
- `peers` - Show other clients connected to the server
- `help` - Show help
- `exit` or `quit` - Exit the client

## Sample Workflow

1. Start the server
2. Start one or more clients
3. Use the `playlist` command to see available songs
4. Use `play song_name.wav` to play a song
5. Use playback controls (pause/resume/stop)

The server streams the audio file to the client, which stores it in memory and plays it locally.

### Unit Tests

After following build instructions:
```bash
   cd build
    ctest --output-on-failure
```

