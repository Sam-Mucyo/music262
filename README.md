# Distributed Audio Synchronization System

A decentralized, peer-based distributed system where multiple machines can collaborate to play the same audio track in synchronization (e.x. surround sound system, etc.)

[![Engineering Notebook](https://img.shields.io/badge/Engineering-Notebook-blue)](https://github.com/Sam-Mucyo/music262/blob/main/docs/notebook/notebook.md)
[![C++ Tests](https://github.com/Sam-Mucyo/music262/actions/workflows/cpp-tests.yml/badge.svg)](https://github.com/Sam-Mucyo/music262/actions/workflows/cpp-tests.yml)

## System Architecture

![System Architecture](./docs/images/hld_diagram.png)

## Principles from Distributed Systems

- Client-server communication using gRPC and Protocol Buffers
- Synchronization using logical clocks (variant of Lamport's algorithm)
- Peer-to-peer consensus network protocol for clients
- Fault tolerance through distributed coordination

## Documentation

[![Client Documentation](https://img.shields.io/badge/Client-Documentation-green)](src/client/README.md)
[![Server Documentation](https://img.shields.io/badge/Server-Documentation-orange)](src/server/README.md)

This project uses Doxygen to generate API documentation from source code comments.

### Building In-code Documentation

```bash
mkdir -p build && cd build
cmake ..
make docs
```

The generated documentation will be available in `docs/doxygen/html/index.html`.

See [Documentation Guide](docs/README.md) for more information on the documentation system and coding style guides.
