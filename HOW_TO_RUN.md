# How to Build and Run the Application

## Build 

### Prerequisites

- C++17 compatible compiler (GCC, Clang, MSVC, etc.)
- CMake 3.14 or higher
- Internet connection (first build will download `spdlog` library)

### Building the Application

1. Create a build directory:

```bash
mkdir build
cd build
```

2. Configure the project:

```bash
cmake ..
```

3. Build the project:

```bash
cmake --build .
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

