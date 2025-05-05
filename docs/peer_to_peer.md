# Peer-to-Peer Audio Synchronization

This document explains how to use the peer-to-peer (P2P) functionality of the Music262 application to synchronize audio playback between multiple clients.

## Overview

The P2P feature allows multiple clients to synchronize their audio playback. When enabled, any playback commands (play, pause, resume, stop) issued on one client will be automatically broadcast to all connected peers, ensuring that all clients stay synchronized.

## Features

- Automatic P2P server starts when the client launches
- Connect to other clients with a simple command
- Synchronized playback across all connected clients
- Robust error handling and logging
- Simple CLI interface

## Getting Started

### Starting the Client

Start the client with the following command:

```bash
./music_client [--server SERVER_ADDRESS] [--p2p-port PORT]
```

Options:
- `--server SERVER_ADDRESS`: Connect to a specific server (default: localhost:50051)
- `--p2p-port PORT`: Specify the port to use for P2P connections (default: 50052)

### Available Commands

Once the client is running, the following P2P-related commands are available:

- `peers`: List all clients connected to the server
- `connections`: List all active peer connections
- `join <ip:port>`: Connect to another client by IP address and port
- `leave <ip:port>`: Disconnect from a specific peer

### Usage Example

1. Start client on machine A:
   ```
   ./music_client
   ```

2. Start client on machine B:
   ```
   ./music_client
   ```

3. On machine A, find the IP address using the system's network tools (e.g., `ifconfig` on Unix-based systems)

4. On machine B, connect to machine A:
   ```
   > join 192.168.1.10:50052  # Use the actual IP address of machine A
   ```

5. Load and play a song on either machine:
   ```
   > playlist  # Show available songs
   > play sample_song.wav
   ```

6. Control playback on either machine, and the commands will propagate:
   ```
   > pause
   > resume
   > stop
   ```

## Technical Details

### Architecture

The P2P functionality is implemented using the following components:

1. **PeerService**: Handles incoming gRPC requests from other clients
2. **PeerNetwork**: Manages connections to other clients and broadcasts commands
3. **AudioClient Extension**: Interfaces with the audio playback and coordinates with PeerNetwork

### Communication Protocol

Clients communicate using the protocol defined in `audio_sync.proto`, which includes:

- Ping: Simple connection verification
- SendMusicCommand: Sends playback commands with timestamp and position
- GetPosition: Queries the current playback position

### Synchronization Logic

To prevent command loops, each client:
1. Marks commands received from peers to prevent re-broadcasting
2. Includes timestamps to help with synchronization
3. Validates connections before sending commands

### Logging

All P2P operations are logged using the application's logging system, which can be helpful for debugging. Logs include:
- Connection attempts and results
- Command broadcasting status
- Command reception and execution
- Errors and warnings

## Troubleshooting

### Can't Connect to Peer

- Ensure both clients are running
- Check firewall settings (port 50052 needs to be open)
- Verify the IP address is correct and accessible
- Try a different port if 50052 is blocked or in use

### Commands Not Propagating

- Check the list of connections to ensure peers are connected
- Look at logs for any errors during command broadcast
- Restart both clients if synchronization issues persist