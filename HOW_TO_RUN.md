# How to Build and Run the Application

The first version of the application has support for macOS only.

## Build

### Prerequisites

- C++17 compatible compiler (GCC, Clang, MSVC, etc.)
- Homebrew (recommended for installing dependencies on macOS)
- CMake 3.14 or higher
- Protobuf library
- spdlog library

### Installing Dependencies (macOS)

You can install the required dependencies using Homebrew:

```bash
brew install cmake protobuf spdlog
```

### Building the Application

From root directory, run:
```bash
cmake -B build -S . && cmake --build build
```

After successful build, the executables will be in the `build/bin` directory.

## Running the Application

### Server

From the build directory:

```bash
./bin/music_server
```

Server commands:
- `status` - Show server status (IP Address and port, active clients, etc.)
- `help` - Show help
- `exit` - Exit the server

You can also specify a port:

```bash
./bin/music_server --port 9000
```

### Client

From the build directory:

```bash
./bin/music_client
```

Client commands:
- `play`, `pause`, `restart`  - for music controls
- `sync  <peer_id>` - allow syncing with currently known peer on the network
- `help` - Show help
- `exit` - Exit the client

