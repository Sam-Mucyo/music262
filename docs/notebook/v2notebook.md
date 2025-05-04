# CS262 Final Project Engineering Notebook
#  Part 0: Introduction

## Motivation and Project Overview

Many of us regularly use sophisticated wireless audio systems like Apple SharePlay/AirPlay 2, Sonos, Spotify Connect/Jam, or Google Cast. These platforms offer the compelling experience of playing music seamlessly across multiple speakers or devices, creating immersive listening environments. While the user experience is often smooth, the underlying technology involves complex proprietary protocols and intricate streaming and control logic. We wanted to figure out how these kinds of systems work.

Instead of relying on centralized control or proprietary solutions, we set out to build a decentralized, peer-based distributed system. In our model, multiple machines—initially, our laptops—collaborate to play the same audio track in synchronization. The core architecture involves a simple server responsible for hosting audio files. Client devices connect to this server to prefetch and buffer the audio content locally. The important aspect, however, is in the distributed coordination and synchronization among the client peers. The goal is to achieve near-simultaneous playback across all participating clients, even though they operate with independent clocks. Furthermore, the system is designed with fault tolerance in mind, ensuring that if one client node crashes or disconnects, the remaining clients can continue playback without interruption.

## Goals and Scope

The primary goals for this project as outlined in our initial proposal are:

*   **Server Functionality:** Implement a server capable of hosting audio files that clients can discover, load, and play.
*   **Client Synchronization:** Enable clients to achieve and maintain synchronized playback. This involves handling control actions (like play/pause) initiated by any client and propagating them effectively across the network. Our approach evolved from simple delays to an NTP-inspired mechanism (detailed in Part 5).
*   **Fault Tolerance:** Design the client network to be resilient to failures. The system must be able to distinguish between intentional control actions and unexpected node failures. In the event of a crash, the remaining active clients should continue their synchronized playback seamlessly.

Our stretch goals included support for multiple audio tracks, enhanced fault tolerance, stereo channel distribution, and client rejoin capabilities.

## Distributed Systems Principles

Concepts we apply from class:

*   **Client-Server Communication:** Utilizing gRPC and protocol buffers (protobuf) for structured communication between the clients and the central server.
*   **Peer-to-Peer Communication:** Implementing direct communication between clients using gRPC for synchronization commands.
*   **Synchronization:** Addressing the challenge of coordinating actions across distributed nodes with independent clocks, culminating in an NTP-inspired approach.
*   **Fault Tolerance:** Designing mechanisms to detect and handle peer failures gracefully.

## Table of Contents

- [Part 0: Introduction](#part-0-introduction) – Overview, motivation, goals, and distributed systems principles  
- [Part 1: Project Setup and Initial Architecture](#part-1-project-setup-and-initial-architecture) – Tech stack, high-level design, system roles  
- [Part 2: Audio Playback Implementation](#part-2-audio-playback-implementation) – CoreAudio integration, buffering, rendering logic  
- [Part 3: Server-Client Communication](#part-3-server-client-communication) – gRPC protocol, server responsibilities, peer discovery  
- [Part 4: Choosing and Implementing the Peer-to-Peer Architecture](#part-4-choosing-and-implementing-the-peer-to-peer-architecture) – Topology evaluation, client dual roles, peer services  
- [Part 5: Simple Delays to NTP](#part-5-simple-delays-to-ntp) – Synchronization algorithm evolution, asynchronous broadcasting, clock offset handling  
- [Part 6: Refactoring for Testability and Modularity](#part-6-refactoring-for-testability-and-modularity) – Interfaces, dependency injection, factory pattern  
- [Part 7: Testing the Distributed System](#part-7-testing-the-distributed-system) – Unit/integration testing evolution, GMock usage, test strategy  
- [Part 8: Finalization, GUI, and Future Work](#part-8-finalization-gui-and-future-work) – GUI, limitations, future improvements  

# Part 1: Project Setup and Initial Architecture

This section covers initial setup, technology choices, and architectural design.

## Technology Stack Selection

Our goal was to build a robust, reasonably performant system while using CS262 concepts. We settled on the following stack:

*   **Language: C++:** Performance characteristics and the availability of low-level system APIs, which seemed potentially necessary for fine-grained audio control and synchronization.
*   **Communication Protocol: gRPC / Protocol Buffers (protobuf):** For communication between the server and clients, and potentially between clients themselves, we opted for gRPC. Its use of HTTP/2 for transport, protobuf for interface definition and serialization, and support for bidirectional streaming made it a good candidate for efficient and structured network communication in a distributed environment. It also made sense in the context of what we learned from the course.
*   **Audio Playback (Initial): CoreAudio (macOS):** Handling audio playback reliably and with low latency is nontrivial. Given the initial project scope and the team's development environment, we decided to start with Apple's CoreAudio framework for macOS. This provided a native, powerful, albeit complex, API for audio processing. We acknowledged this introduced platform dependency (initially macOS-only) but prioritized getting a functional playback system working first. Alternatives or cross-platform libraries were considered but deferred.
*   **Synchronization Protocol: NTP-Inspired Custom Protocol:** Synchronization was identified early as the core challenge. While initial thoughts included logical clocks, we ultimately developed a custom synchronization mechanism built on top of gRPC, inspired by Network Time Protocol (NTP), to estimate clock offsets and network latency between peers. This is detailed in Part 5.

## Initial System Architecture


![Initial System Architecture Diagram](docs/images/hld_diagram.png)

We were thinking:

1.  **Server:** A central entity responsible for:
    *   Hosting audio files (e.g., WAV).
    *   Allowing clients to connect and discover available tracks.
    *   Serving audio data chunks to clients upon request.
    *   Acting as a discovery service, maintaining a list of active clients to facilitate peer connections.
2.  **Clients (Peers):** Individual devices (laptops) participating in playback:
    *   Connect to the server to fetch song lists and download/buffer audio.
    *   Implement audio playback using CoreAudio.
    *   Establish direct connections with other clients (peers) using information from the server.
    *   Communicate directly with peers to achieve and maintain playback synchronization.
    *   Handle user commands (play, pause) and coordinate these actions with peers.
    *   Implement fault detection for peer failures.

While the high-level roles are the same , the specifics of the peer-to-peer interaction and synchronization mechanism changed a lot, as detailed in later parts (Part 4 and Part 5).

# Part 2: Audio Playback Implementation

One of the first major technical hurdles was implementing reliable audio playback on the client side. As decided in Part 1, we targeted macOS using the CoreAudio framework. This section delves into our exploration of CoreAudio, the implementation challenges faced, and the resulting audio player component, based on work we did around April 5th and April 12th, as well as later refinements.

## Dealing with CoreAudio

CoreAudio is Apple's low-level framework for handling audio on macOS and iOS. Its power comes at the cost of complexity. Our initial task was simply to understand its fundamental concepts sufficiently to play an audio file loaded into memory or read from disk.

Much of early April was spent deciphering CoreAudio's architecture. A key realization was the role of the `AudioUnit`. It's not a unit of data, but rather the core processing engine – the "musician" in our analogy. The actual audio data serves as the "sheet music".

Central to CoreAudio's operation is the `RenderCallback` function. The `AudioUnit` invokes this callback whenever it needs more audio data to feed the output device (e.g., speakers). Our callback's responsibility is to act as the "page turner," fetching the next chunk of audio data from our source (like a buffer holding the downloaded WAV file) and copying it into the buffer provided by the `AudioUnit`. This callback-driven model allows for continuous playback without loading massive files entirely into memory and enables real-time processing if needed.

We found Apple's documentation helpful, particularly the [Audio Unit Programming Guide](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnit/TheAudioUnit.html) (though navigating Apple's developer docs can sometimes be a journey in itself).

## Implementation Details and Refinements

Based on this understanding, the implementation of the `AudioPlayer` class began, primarily located in `client/src/audioplayer.cpp`, with instantiation and basic usage demonstrated in `client/src/main.cpp`. The necessary Apple frameworks included `AudioToolbox`, `CoreAudio`, and `CoreFoundation`.

*   **File Handling:** Using `AudioFile` APIs (part of the `AudioToolbox` framework) to open and read audio data from various file formats supported by macOS. This abstracted away the complexities of different codecs.
*   **Buffering:** A robust buffering strategy is crucial for glitch-free playback. We implemented a buffering mechanism (likely double or triple buffering) where the `RenderCallback` fills buffers ahead of time, ensuring the `AudioUnit` always has data ready to play.
*   **Callback Logic:** The `RenderCallback` was implemented to read the correct segment of audio data from our loaded file buffer (tracking the current playback position) and copy it into the `AudioUnit`'s input buffer during each callback.
*   **Build System Integration:** Linking against the necessary CoreAudio frameworks (`AudioToolbox`, `CoreAudio`, `CoreFoundation`) required careful configuration in our CMake build system (`client/src/CMakeLists.txt`, root `CMakeLists.txt`).
*   **Bug Fix:** Early versions of the `AudioPlayer` had a bug that could lead to multiple audio streams being created unintentionally, causing playback issues or crashes. This was identified and fixed in around May 2nd, ensuring only a single playback stream was active.

## Platform Dependency

Our reliance on CoreAudio inherently tied the initial client implementation to macOS. This was a conscious trade-off made for expediency and access to a powerful native framework. Around April 12th and 13th we discussed potentially replacing CoreAudio with a cross-platform library later, but the immediate focus remained on getting the macOS version functional.

## Development Workflow

On April 12th we also detailed the typical development workflow established for building and testing the client, including the audio player component:

Our established workflow involved building with CMake (`cmake .. && make` in the `build` directory), running the client (`./bin/music262_client`), and testing using CTest (`ctest --output-on-failure`). This structured approach was vital for managing the C++ codebase and ensuring the audio player functioned correctly before integrating it with the networking layers.


# Part 3: Server-Client Communication

With a basic audio playback mechanism established on the client (Part 2), the next logical step was enabling clients to fetch audio content from the central server. This involved defining and implementing the communication protocol between the server and clients using gRPC, a core task leading up to our internal Milestone 2 (around April 24th).

## Server Role and Responsibilities

As outlined in the initial architecture (Part 1), the server acts as the central coordinator and content source. Its primary responsibilities included:

*   **Hosting Audio Files:** Storing the music tracks (WAV files) that the clients would play.
*   **Client Registration/Discovery:** Allowing clients to connect and register their presence. Crucially, the server also provides connected clients with the list of other currently active peers (including their IP addresses and P2P listening ports), facilitating the peer discovery necessary for the P2P synchronization layer. This was fined around May 4th, to ensure clients correctly received peer information.
*   **Serving Audio Data:** Providing lists of available songs and streaming the actual audio data to clients upon request.

## Defining the Interface: gRPC and Protobuf

We used gRPC with protobuf for server-client communication. The service definitions (`src/proto/audio_server.proto` or similar) outlined RPCs for actions like:

*   `RegisterClient`: Allows a client to announce its presence and P2P listening address.
*   `GetPeerList`: Allows a client to retrieve the list of other active clients.
*   `GetSongList`: Retrieves the list of available audio tracks.
*   `GetSong`: Streams the requested audio track data to the client.

## Implementation Efforts

Leading up to the April 24th milestone, our focus was on implementing this server-client interaction:

*   **Server-Side (`src/server/`):** Involved setting up the gRPC server, implementing the service methods (handling registration, maintaining the peer list, reading audio files, streaming data), and managing client connections.
*   **Client-Side (`src/client/`):** Required implementing the gRPC client stub, connecting to the server, calling RPC methods (registering, getting peer/song lists, requesting and receiving audio streams), and feeding the received audio data into the `AudioPlayer` component.
*   **Integration:** Ensuring the client could successfully connect, fetch data, and potentially play it back, verifying the end-to-end flow.

## Challenges and Refinements

We ran into quite a few issues around April 24th in our implementation phase:

*   **gRPC Build/Compile Issues:** Standard integration challenges with complex libraries like gRPC.
*   **Connection Feedback:** Ensuring clients received clear confirmation of successful server connection.
*   **Peer Discovery Reliability:** Initial implementations had issues correctly providing peer IP addresses to connecting clients. We specifically addressed and fixed this on May 3rd, ensuring the peer discovery mechanism functioned reliably.
*   **Song Selection:** The interface evolved to use song IDs/numbers for selection instead of filenames for simplicity.

Successfully implementing robust server-client communication was critical. It provided the foundation for clients to obtain audio content and, importantly, discover each other, paving the way for the P2P synchronization efforts detailed next.


# Part 4: Choosing and Implementing the Peer-to-Peer Architecture

With the foundational elements of server-client communication (Part 3) and basic audio playback (Part 2) in place, we had to deal with the project's central challenge: synchronizing playback across multiple, independent clients. Our initial proposal (Part 1) leaned towards a peer-to-peer (P2P) approach, but we formally evaluated different things before committing.

## Evaluating Synchronization Topologies

We considered three main architectural patterns for handling the real-time synchronization commands (play, pause, seek):

![Synchronization Topology Comparison](docs/images/topologies.png)

1.  **Central Coordinator (Star Topology):** In this model, the existing server would act as the central timing authority. It would maintain a connection (e.g., a bidirectional gRPC stream) to every client and issue precise commands like "start playing track X at position Y exactly at time T".
    *   *Pros:* Clients remain relatively simple ("pure clients"), as they only need to listen for commands from the server and don't require their own listening ports for peer communication.
    *   *Cons:* This introduces a single point of failure and a potential bottleneck. All timing-critical traffic must pass through the server, increasing latency and making the system entirely dependent on the server's availability and performance.

2.  **Peer-to-Peer (Mesh Topology):** Each client acts as both a gRPC client and a gRPC server. Upon startup, it registers its listening address (`host:port`) with a discovery service (our existing audio server fulfills this role). Clients then use this information to establish direct connections with all other known peers. Playback commands initiated on one peer are broadcast directly to all other peers.
    *   *Pros:* Eliminates the central bottleneck for synchronization traffic once peers discover each other. The system can potentially tolerate server unavailability for synchronization purposes after initial discovery. It also offers a natural way to handle peer failures – if a peer becomes unreachable, others can continue synchronizing among themselves (though adding new peers or handling partitions requires the server).
    *   *Cons:* Requires each client to open a listening port, which might pose firewall challenges. The implementation complexity increases slightly due to clients having dual roles (client and server) and needing logic for peer connection management, service definition, and potentially reconnection.

3.  **Multicast/Broadcast:** Clients could use UDP multicast or a publish/subscribe message bus to share timing packets directly on the local network. gRPC would remain for control-plane operations (like fetching audio from the server) but not for real-time synchronization.
    *   *Pros:* Potentially very low latency and scales well on a single Local Area Network (LAN) segment.
    *   *Cons:* Requires a multicast-enabled network, which is often not available or configured, especially across different subnets or Wi-Fi networks. It adds more moving parts and dependencies outside the gRPC framework.

**Decision:** We opted for the **Peer-to-Peer** architecture. While slightly more complex than the central coordinator model, it offered better resilience against server bottlenecks and aligned well with our goal of building a more decentralized system. The reliance on standard gRPC for peer communication felt more manageable and less environment-dependent than multicast.

## Implementing the P2P Layer

Based on the chosen architecture, the implementation involved several key components and steps, primarily documented in `peer_to_peer.md` and reflected in PRs like #43 ("Peer2peer - Add basic audio syncing") and #42 ("Add bidirectionality + for loop synchronization") around May 2nd:

*   **Dual Role Clients:** Each client executable was enhanced to run both a gRPC client (for talking to the server and other peers) and a gRPC server (using `PeerService` to listen for incoming commands from peers). The P2P listening port defaults to 50052 but can be configured (`--p2p-port`).
*   **Discovery via Server:** Clients register their P2P `host:port` with the main audio server upon connection. Clients can query the server (`peers` command) to get a list of other registered clients.
*   **Peer Connection Management (`PeerNetwork`):** A `PeerNetwork` class was introduced (`src/client/peer_network.cpp`) responsible for:
    *   Establishing and maintaining gRPC connections (stubs) to other peers using addresses obtained from the server or via the `join <ip:port>` command.
    *   Tracking active connections (`connections` command).
    *   Broadcasting playback commands (`MusicCommand`) to all connected peers.
    *   Handling disconnection (`leave <ip:port>` command) and potentially detecting dead peers.
*   **P2P Service Definition (`audio_sync.proto`):** The `PeerService` interface was defined in `src/proto/audio_sync.proto`, including RPCs like:
    *   `Ping`: For basic connectivity checks and latency measurement.
    *   `SendMusicCommand`: To broadcast playback actions (play, pause, etc.) along with timing information.
    *   `GetPosition`: To query the current playback position of a peer (though its use in the final sync logic might be limited).
*   **Client Integration (`AudioClient`):** The main `AudioClient` class (`src/client/client.cpp`) was extended to interact with `PeerNetwork`. When a user issues a playback command locally, the `AudioClient` now also instructs the `PeerNetwork` to broadcast that command to other peers.

This setup established the communication pathways for P2P synchronization. The next critical step was developing and refining the logic to actually *use* these pathways to achieve accurate, synchronized playback.

# Part 5: Simple Delays to NTP

In Part 4, we laid out our decision to use a peer-to-peer architecture for synchronization. Now came the hard part: making it actually work. Our journey involved several iterations, starting with naive approaches and progressively adding more complex stuff as we hit limitations.

## Attempt 1: The Simplest Thing - Sequential Broadcast

Our very first attempt, reflected in early P2P implementations around May 1st was based on a simple idea: when a client (let's call it the initiator) wants to start playback, it tells all its peers to play *now*, and then starts playing itself. We hoped the low latency of C++ and the local network might be enough for basic synchronization.

```cpp
// Conceptual code for initial sequential broadcast
void PeerNetwork::BroadcastPlayCommand(int64_t position) {
    MusicCommand command;
    command.set_action("play");
    command.set_position_nanos(position);
    // No sophisticated timing yet...

    for (const auto& peer_addr : connected_peers_) {
        // Send command synchronously to each peer
        Status status = SendCommandToPeer(peer_addr, command);
        if (!status.ok()) {
            // Log error
        }
    }
    // Initiator starts playing after telling everyone else
    audio_client_->HandleMusicCommand(command); 
}
```

**Result:** This barely worked. Synchronization was noticeably poor even with just 3 clients, exhibiting clear lag. The initiator started playing before confirming peers even received the command, let alone processed it.

## Attempt 2: Introducing Delays

Realizing the flaws, we introduced artificial delays around May 1/2. The initiator would still broadcast sequentially, but it would calculate a small `wait_time` for each peer, increasing slightly for each subsequent peer in the broadcast loop. The idea was to stagger the start times, hoping to compensate for the sequential broadcast delay.

```cpp
// Conceptual code with simple delay (based on pasted_content_2.txt)
void PeerNetwork::BroadcastPlayCommand(int64_t position) {
    MusicCommand command;
    // ... set action, position ...
    int success_count = 0;
    for (const auto& peer_addr : connected_peers_) {
        // Calculate a small, increasing delay
        // !!! THIS WAS FLAWED LOGIC !!!
        int64_t wait_ns = (connected_peers_.size() - success_count) * 10; 
        command.set_wait_time_ns(wait_ns); 

        Status status = SendCommandToPeer(peer_addr, command);
        if (status.ok()) {
            success_count++;
        }
    }
    // Initiator also waits based on total peers?
    // audio_client_->HandleMusicCommand(command, self_wait_time);
}
```

**Result:** Still problematic. The `wait_time` calculation (`(peer_list.size() - success_count)*10` ns) was arbitrary and didn't scale. Synchronization quality degraded rapidly as more peers joined. Furthermore, the sequential nature of the broadcast itself became a bottleneck.

## Problem: gRPC Deadlines and Sequential Bottlenecks

As we tested with more peers (around 4+), we started hitting gRPC `deadline exceeded` errors (`pasted_content_3.txt`). The root causes were:

1.  **Sequential Broadcasting:** Sending commands one by one in a loop meant the total time taken scaled linearly with the number of peers. If each RPC call took even a fraction of a second (including network latency and processing), broadcasting to many peers could easily exceed the default gRPC deadline (often 1 second).
2.  **Expensive Pre-computation (Initially):** At one point, we considered recalculating network timing (latency) before *every* broadcast, involving multiple pings per peer. This added significant overhead before the time-sensitive play command could even be sent.

## Attempt 3: Asynchronous Broadcasting + NTP-Style Sync

This led to a major refinement around May 1st-4th, which you can see in our closed github PRs as well:

1.  **Asynchronous Broadcasting:** We abandoned the sequential `for` loop for broadcasting. Instead, we moved to sending commands to all peers concurrently using asynchronous gRPC calls. The initiator no longer waits for a response from one peer before sending to the next. This dramatically reduced the total time required to *initiate* the broadcast.

    ```cpp
    // Conceptual async broadcast
    void PeerNetwork::BroadcastCommandAsync(const MusicCommand& command) {
        for (const auto& peer_addr : connected_peers_) {
            // Initiate async RPC call to each peer
            // (Details involve completion queues, callbacks etc.)
            StartAsyncSendCommand(peer_addr, command);
        }
        // Initiator proceeds without waiting for individual peer responses
    }
    ```

2.  **NTP-Inspired Clock Synchronization:** We implemented a more robust mechanism inspired by the Network Time Protocol (NTP) to estimate clock offsets and network latency (Round Trip Time - RTT) between peers.
    *   **Periodic Pinging:** Clients periodically ping each other in the background (or upon connection) multiple times (e.g., 5 trials) to get more stable RTT and offset estimates.
    *   **Offset Calculation:** Standard NTP formulas were used to estimate the clock offset between pairs of peers based on timestamps exchanged during the ping process.
    *   **Target Execution Time:** Instead of arbitrary delays, the initiator calculates a target execution time in the near future (e.g., `now + safety_margin + max_estimated_RTT`). This target time, adjusted for the estimated clock offset for each specific peer, is included in the `MusicCommand`.
    *   **Receiver Logic:** Upon receiving a command, a peer uses the target time and its estimated offset relative to the sender to schedule the action locally using its own clock.

    ```protobuf
    // Revised MusicCommand (conceptual)
    message MusicCommand {
      string action = 1;
      int64 position_nanos = 2;
      int64 target_execution_time_unix_ns = 3; // Target time in sender's clock domain
      // No longer sending arbitrary wait_time
    }
    ```

    ```cpp
    // Conceptual receiver logic
    void PeerService::HandleMusicCommand(const MusicCommand& command, const std::string& sender_addr) {
        int64_t estimated_offset_ns = GetEstimatedOffset(sender_addr);
        int64_t local_target_time_ns = command.target_execution_time_unix_ns() + estimated_offset_ns;
        int64_t current_local_time_ns = GetCurrentTimeNs();
        int64_t delay_ns = local_target_time_ns - current_local_time_ns;

        if (delay_ns > 0) {
            // Schedule the action (play, pause) to run after delay_ns
            ScheduleAction(command.action(), command.position_nanos(), delay_ns);
        } else {
            // Target time already passed, execute immediately (or log warning)
            ExecuteAction(command.action(), command.position_nanos());
        }
    }
    ```

**Result:** Yayyy this asynchronous, NTP-based approach significantly improved synchronization quality and scalability compared to the earlier attempts. It directly addressed the sequential broadcast bottleneck and replaced arbitrary delays with calculated, offset-adjusted target times.

We still acknowledge remaining limitations: the system doesn't have mechanisms for continuous drift correction during playback or robust handling of packet loss. However, for event-based synchronization (play, pause), this refined P2P logic proved much more effective.

# Part 6: Refactoring for Testability and Modularity

As the complexity of the client application grew, particularly with the integration of server communication, audio playback, and peer-to-peer networking, we encountered significant challenges in testing and maintenance. The initial codebase, while functional, suffered from tight coupling and mixed responsibilities, making isolated testing nearly impossible. This led to a refactoring effort for testability and modularity starting around May 4th.

##  Problems with the original structure

1.  **Tight Coupling:** The main `AudioClient` class was directly intertwined with gRPC implementation details for both server and peer communication. Methods within `AudioClient` often made direct calls to gRPC stubs.
2.  **Mixed Responsibilities:** Business logic (like handling user commands, managing playback state) was mixed with networking code (RPC calls, connection management) within the same classes.
3.  **Difficult Mocking:** The direct use of concrete gRPC classes made it extremely hard to substitute mock objects for unit testing. We couldn't easily test the client's logic without setting up real gRPC servers and network connections.
4.  **Limited Testability:**  Because of that, most testing had to be integration testing, which is slower, more brittle (prone to network failures), and doesn't provide fine-grained feedback on specific units of code.

## Interfaces and Injection

To address these problems, we did a few things:

1.  We introduced abstract interfaces to define the *contracts* for external services the `AudioClient` depends on, rather than having `AudioClient` depend directly on concrete implementations.
    *   `AudioServiceInterface`: Defines operations related to the main audio server (e.g., `GetSongList`, `GetSongStream`).
    *   `PeerServiceInterface`: Defines operations for peer-to-peer communication (e.g., `BroadcastCommand`, `PingPeer`).

2.  We did concrete implementations of these interfaces for the specific communication mechanisms (just gRPC here).
    *   `GrpcAudioService`: Implements `AudioServiceInterface` using gRPC calls to the main server.
    *   `GrpcPeerService`: Implements `PeerServiceInterface` using gRPC calls for P2P interactions (likely integrated within `PeerNetwork`).

3.  Instead of `AudioClient` creating its own service communication objects, these dependencies (implementations of the service interfaces) are now *injected* into the `AudioClient` upon its creation.

4. To manage the creation of the `AudioClient` and its dependencies, we introduced a factory function (`client_factory.cpp`). This centralizes the object graph construction, making it clear how components are wired together.

    ```cpp
    // Conceptual Factory Function (based on pasted_content.txt)
    std::unique_ptr<AudioClient> CreateAudioClient(const std::string& server_address, /* other params */) {
        // Create concrete service implementations (using gRPC)
        auto audio_service = std::make_unique<GrpcAudioService>(server_address);
        // Peer service might be part of PeerNetwork or created separately
        auto peer_service = std::make_unique<GrpcPeerService>(/*...*/);
        
        // Create PeerNetwork (which might use PeerService)
        auto peer_network = std::make_shared<PeerNetwork>(peer_service.get());

        // Inject dependencies into AudioClient
        auto client = std::make_unique<AudioClient>(std::move(audio_service), peer_network);
        
        // Potentially set back-references if needed
        // peer_network->SetClient(client.get()); 

        return client;
    }
    ```

5.  We tried to separate responsibilities:
    *   `AudioClient`: High-level orchestration, user command handling, playback state.
    *   `AudioPlayer`: Low-level audio rendering (CoreAudio interaction).
    *   `GrpcAudioService`: Communication with the main server.
    *   `PeerNetwork` / `GrpcPeerService`: P2P connection management and communication logic.

## Lessons Learned

Even though this took a while, we think these were some of the useful things that came out of it:

*   **Improved Testability:** We can now write unit tests for `AudioClient` by injecting *mock* implementations of `AudioServiceInterface` and `PeerServiceInterface`. These mocks simulate server/peer behavior without actual network calls, making tests fast, reliable, and focused on the client's logic.
*   **Increased Modularity:** Components are more self-contained and interact through well-defined interfaces.
*   **Enhanced Maintainability:** Changes to the communication mechanism (e.g., tweaking gRPC settings) are localized within the concrete service implementations, minimizing impact on the core client logic.

# Part 7: Testing the Distributed System

## Initial Testing Infrastructure

We established a testing framework early on using:

*   **Google Test (GTest):** For writing C++ unit tests.
*   **CMake/CTest:** For building and running the test suite (`ctest --output-on-failure`).
*   **Code Coverage (lcov):** To measure test effectiveness (`make coverage`).

## Challenges and the Drive for Refactoring

Before the refactoring (Part 6), testing the client logic was difficult. Unit testing `AudioClient` was nearly impossible because its core logic was tightly coupled to live gRPC calls. We couldn't easily simulate peer or server responses. This meant relying heavily on:

*   **Integration Tests:** These involved spinning up actual server and client processes, making them slower and susceptible to network flakiness. While valuable, they didn't allow for fine-grained testing of specific client logic branches.
*   **Manual Testing:** Running multiple client instances and visually/audibly checking synchronization. This was essential for assessing the end-user experience but tedious, hard to automate, and poor at catching edge cases.

The difficulties in writing effective unit tests was a big reason for our refactoring effort described in Part 6.

## Post-Refactoring Testing Strategy

The refactoring, which introduced interfaces (`AudioServiceInterface`, `PeerServiceInterface`) and dependency injection, unlocked the ability to write proper unit tests for the `AudioClient` and `PeerNetwork` logic:

*   **Mocking Dependencies:** We could now create mock implementations of the service interfaces using GTest's mocking framework (GMock). These mocks simulate responses from the server or peers without any actual network communication.
*   **Focused Unit Tests:** This allowed us to write tests verifying specific scenarios within `AudioClient` or `PeerNetwork`, such as:
    *   Does the client correctly parse a `SongListResponse`?
    *   Does `PeerNetwork` attempt to broadcast a command when requested?
    *   How does the client handle an error status returned from a (mocked) service call?

## Integration and System Testing

*   **Integration Tests:** Verifying the interaction between the refactored components (e.g., `AudioClient` using `GrpcAudioService`) and ensuring the gRPC communication worked end-to-end.
*   **Manual Testing:** Running multiple clients, often on different machines, remained the ultimate test of synchronization quality and usability. This helped identify subtle timing issues or bugs not caught by automated tests.

# Part 8: Finalization, GUI, and Future Work

At the end, our focus shifted towards finalizing the core functionality, improving usability, documenting our work, and reflecting on potential future directions.

## GUI Development

We developed a graphical user interface (GUI) for the client. The command-line interface, while functional for development and testing, is less intuitive for end-users. You can check out the 'client-gui' branch for our initial implementation of this GUI. 

## Future Work

While we achieved our core goals of building a functional P2P synchronized audio playback system, several areas offer potential for future improvement:

*   **Complete GUI:** Finalize and test the client GUI more substantially.
*   **Continuous Drift Correction:** Implement mechanisms to actively monitor and correct clock drift between clients *during* playback, not just at the start of commands.
*   **Robustness:** Add resilience against packet loss for synchronization commands (e.g., using sequence numbers or acknowledgments).
*   **Client Rejoin:** Allow clients that crash or disconnect to rejoin an ongoing session and resynchronize.
*   **Cross-Platform Audio:** Replace the macOS-specific CoreAudio implementation with a cross-platform audio library (e.g., PortAudio, SDL_mixer) to enable wider compatibility.
*   **Stereo/Channel Distribution:** Implement the stretch goal of assigning different audio channels (e.g., left/right) to different clients for a surround sound effect.
*   **Enhanced Testing:** Continue expanding unit and integration test coverage, particularly for the synchronization logic under various network conditions and the GUI.
*   **Performance Optimization:** Profile the system to identify and address any performance bottlenecks in audio processing or network communication.

