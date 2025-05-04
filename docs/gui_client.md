# GUI Client User Guide

The GUI client provides an intuitive graphical interface for all the functionality available in the terminal client, making it easier to manage playback and peer connections.

## Overview

The GUI client is organized into tabs for different functions:

1. **Playback Tab**: View and control audio playback
2. **Peer Network Tab**: Manage peer connections for synchronized playback

A status bar at the bottom of the window shows server and P2P connection status.

## Playback Tab

![Playback Tab Screenshot](images/gui_playback_tab.png)

The Playback tab includes:

- **Song List**: Shows all available songs from the server
- **Playback Controls**:
  - **Play**: Loads and plays the selected song
  - **Pause**: Pauses the currently playing song
  - **Resume**: Resumes a paused song
  - **Stop**: Stops playback and resets position
- **Position Slider**: Shows current playback position and allows seeking
- **Time Display**: Shows current position and song duration

## Peer Network Tab

![Peer Network Tab Screenshot](images/gui_peer_tab.png)

The Peer Network tab includes:

- **Server Clients**: List of clients connected to the server
- **Peer Connection Controls**:
  - **Join Peer**: Connect to a peer by entering IP:port
  - **Leave Peer**: Disconnect from a selected peer
- **Active Connections**: List of peers currently connected for synchronized playback
- **Gossip Button**: Share your peer connections with all connected peers
- **Sync Offset**: Shows the average timing offset with connected peers

## Using the GUI Client

### Starting Playback

1. Start the server and GUI client
2. In the Playback tab, select a song from the list
3. Click the Play button
4. Use Pause, Resume, and Stop buttons to control playback
5. Track playback progress on the position slider

### Connecting to Peers

1. In the Peer Network tab, click "Refresh Peer List" to see available clients
2. Enter a peer address (IP:port) and click "Join Peer"
3. The peer will appear in the Active Connections list
4. Select a peer from the list and click "Leave Peer" to disconnect
5. Click "Gossip" to share your connections with all peers

### Synchronized Playback

Once connected to peers:

1. Any playback command you issue (play, pause, resume, stop) will be synchronized across all connected peers
2. The average timing offset shows how well synchronized your playback is with peers
3. Lower offset values indicate better synchronization

## Command Line Arguments

The GUI client accepts the same command line arguments as the terminal client:

```bash
./bin/music_client_gui --server <server_address> --p2p-port <port>
```

- `--server`: Address of the server (default: localhost:50051)
- `--p2p-port`: Port for the P2P server (default: 50052)