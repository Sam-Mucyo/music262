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
- Qt5 (optional, only required for the GUI client)

### Installing Dependencies (macOS)

You can install the required dependencies using Homebrew:

```bash
# Core dependencies
brew install cmake protobuf grpc spdlog

# Optional: Qt5 (only needed for GUI client)
brew install qt@5
```

### Building the Application

From root directory, run:
```bash
# Build everything (including GUI client if Qt5 is available)
cmake -B build -S . && cmake --build build

# Build without the GUI client
cmake -B build -S . -DBUILD_GUI=OFF && cmake --build build
```

The build process will automatically format the code according to style guidelines.

After successful build, the executables will be in the `build/bin` directory. If you built with `-DBUILD_GUI=OFF`, the `music_client_gui` executable will not be available.

### Developer setup (optional)

If you use clangd/VS Code/Neovim for C++ language features and hate those red-underlines, give your editor the exact compile flags:
```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json .   # makes clangd pick it up from the repo root
```

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

### Terminal Client

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
- `play <song_num>` - Load and play a song by number
- `pause` - Pause the currently playing song
- `resume` - Resume the paused song
- `stop` - Stop playback and reset position
- `peers` - Show other clients connected to the server
- `join <ip:port>` - Join a peer for synchronized playback
- `leave <ip:port>` - Leave a connected peer
- `connections` - List all active peer connections
- `gossip` - Share all active peer connections with all peers
- `help` - Show help
- `exit` or `quit` - Exit the client

### GUI Client

From the build directory:

```bash
./bin/music_client_gui
```

You can specify the server address and P2P port:

```bash
./bin/music_client_gui --server localhost:50051 --p2p-port 50052
```

The GUI client provides the same functionality as the terminal client but with an intuitive graphical interface:

#### Playback Tab
- View and select from available songs
- Play, pause, resume, and stop buttons
- Position slider with time display
- Automatic updates of playback position

#### Peer Network Tab
- View clients connected to the server
- Connect to peers with a simple interface
- Manage active peer connections
- Gossip peer connections to all peers
- View synchronization offsets


### Unit Tests

After following build instructions:
```bash
cd build
ctest --output-on-failure
```

