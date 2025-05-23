# Music262 Server

The server component acts as a central hub for music streaming and client coordination. It provides audio files to clients, maintains a registry of connected clients, and facilitates peer discovery.

## Architecture

#### AudioServer (`audio_server.h/audio_server.cpp`)

- Core server logic for the music streaming service
- Manages a directory of audio files and builds a playlist
- Handles client registration and tracking
- Provides methods to get audio file paths and playlist information
- Maintains a list of connected clients

#### Main (`main.cpp`)

- Initializes the server application
- Sets up the gRPC server to listen for client connections
- Configures the audio directory and other server parameters

## Communication Protocol

The server uses gRPC for communication with clients:

- Defined in `audio_service.proto`
- Provides methods for clients to:
  - Get playlist information
  - Load audio data
  - Register with the server
  - Discover other connected clients

## Server Configuration

The server can be configured with command-line arguments:

- `--port`: The port to listen on (default: 50051)
- `--audio-dir`: Directory containing audio files (default: "../sample_music")
