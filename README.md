# Distributed Audio Synchronization System

A decentralized, peer-based distributed system where multiple machines can collaborate to play the same audio track in synchronization (e.x. surround sound system, etc.)

[![Engineering Notebook](https://img.shields.io/badge/Engineering-Notebook-blue)](https://github.com/Sam-Mucyo/music262/blob/main/docs/notebook/notebook.md)
[![C++ Tests](https://github.com/Sam-Mucyo/music262/actions/workflows/cpp-tests.yml/badge.svg)](https://github.com/Sam-Mucyo/music262/actions/workflows/cpp-tests.yml)
[![Client Documentation](https://img.shields.io/badge/Client-Documentation-green)](src/client/README.md)
[![Server Documentation](https://img.shields.io/badge/Server-Documentation-orange)](src/server/README.md)

## System Architecture

![System Architecture](./docs/images/hld_diagram.png)

## Features

- **Audio Streaming**: Server streams audio files to clients
- **Terminal Client**: Command-line interface for audio playback and peer control
- **GUI Client**: Graphical interface for intuitive control of audio playback and peer management
- **Peer Synchronization**: Clients can connect directly to each other to synchronize audio playback
- **Peer Discovery**: Server facilitates client discovery in the network

## Principles from Distributed Systems

- Client-server communication using gRPC and Protocol Buffers
- Synchronization using logical clocks (variant of Lamport's algorithm)
- Peer-to-peer consensus network protocol for clients
- Fault tolerance through distributed coordination

## Building In-code Documentation

```bash
mkdir -p build && cd build
cmake ..
make docs
```

The generated documentation will be available in `docs/doxygen/html/index.html`.

See [Documentation Guide](docs/README.md) for more information on the documentation system and coding style guides.
