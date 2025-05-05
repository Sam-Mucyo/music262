# Music262 Client

The client component provides a music player that can stream audio from the central server and synchronize playback with other clients in a peer-to-peer network.

## Architecture

### Core Components

#### AudioClient (`client.h/client.cpp`)

- Main client interface that coordinates all components
- Handles communication with the server via gRPC
- Controls the audio player for local playback
- Manages peer synchronization

#### AudioPlayer (`audioplayer.h/audioplayer.cpp`)

- Handles audio playback using Core Audio and Audio Toolbox frameworks on macOS
- Provides functions for loading, playing, pausing, resuming, and stopping audio
- Tracks playback position and state
- Uses a callback-based audio rendering system

#### PeerNetwork (`peer_network.h/peer_network.cpp`)

- Manages peer-to-peer connections between clients
- Implements a gRPC server to accept connections from other peers
- Provides methods to connect to and disconnect from peers
- Broadcasts commands to synchronize playback across peers
- Implements a "gossip" protocol to share peer connection information

#### SyncClock (`sync_clock.h/sync_clock.cpp`)

- Handles time synchronization between peers
- Calculates time offsets between clients to enable synchronized playback
- Uses network time protocol (NTP) principles for clock synchronization

### Service Implementations

#### AudioServiceGRPC (`audio_service_grpc.cpp`)

- Implements the AudioServiceInterface for communication with the server
- Handles requests for playlist information and audio data

#### PeerServiceGRPC (`peer_service_grpc.cpp`)

- Implements the PeerServiceInterface for communication with other peers
- Handles peer-to-peer commands and synchronization

### Entry Point

#### Main (`main.cpp`)

- Initializes the client application
- Provides a command-line interface for user interaction
- Processes user commands and delegates to appropriate components

## Communication Protocols

The client uses two types of communication:

1. **Client-Server Communication**: Uses gRPC to communicate with the central server

2. **Peer-to-Peer Communication**: Uses gRPC for direct communication between clients
   - Synchronizes playback commands (play, pause, resume, stop)
   - Shares connection information through gossip protocol
   - Performs time synchronization for coordinated playback

## User Interface

The client provides a command-line interface with commands such as:

- `playlist`: Get a list of available songs from the server
- `play <song_num>`: Load and play a specific song
- `pause`, `resume`, `stop`: Control playback
- `peers`: Get a list of other clients connected to the server
- `join <ip:port>`: Connect to another peer for synchronized playback
- `leave <ip:port>`: Disconnect from a peer
- `connections`: List active peer connections
- `gossip`: Share connection information with all peers
