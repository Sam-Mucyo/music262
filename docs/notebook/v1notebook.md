# CS262 Final Project Engineering Notebook
#  Part 0: Introduction

## Motivation and Project Overview

Many of us regularly use sophisticated wireless audio systems like Apple SharePlay/AirPlay 2, Sonos, Spotify Connect/Jam, or Google Cast. These platforms offer the compelling experience of playing music seamlessly across multiple speakers or devices, creating immersive listening environments. While the user experience is often smooth, the underlying technology involves complex proprietary protocols and intricate streaming and control logic. We wanted to figure out how these kinds of systems work.

Instead of relying on centralized control or proprietary solutions, we set out to build a decentralized, peer-based distributed system. In our model, multiple machines—initially, our laptops—collaborate to play the same audio track in near-perfect synchronization. The core architecture involves a simple server responsible for hosting audio files. Client devices connect to this server to prefetch and buffer the audio content locally. The crucial aspect, however, lies in the distributed coordination and synchronization among the client peers. The goal is to achieve near-simultaneous playback across all participating clients, even though they operate with independent clocks. Furthermore, the system is designed with fault tolerance in mind, ensuring that if one client node crashes or disconnects, the remaining clients can continue playback without interruption.

## Goals and Scope

The primary goals for this project as outlined in our initial proposal are:

*   **Server Functionality:** Implement a server capable of hosting audio files that clients can discover, load, and play.
*   **Client Synchronization:** Enable clients to achieve and maintain synchronized playback. This involves handling control actions (like play/pause) initiated by any client and propagating them effectively across the network. We planned to explore techniques based on logical clocks or vector clock-inspired methods to estimate relative time offsets and align playback accurately.
*   **Fault Tolerance:** Design the client network to be resilient to failures. The system must be able to distinguish between intentional control actions (like a user pausing playback) and unexpected node failures (like a client crashing). In the event of a crash, the remaining active clients should continue their synchronized playback seamlessly.

Our stretch goals include support for multiple audio tracks, enhanced fault tolerance (tolerating more than one failure), stereo channel distribution for a surround sound effect, and the ability for a crashed node to rejoin and resynchronize with the ongoing playback.

## Distributed Systems Principles

Concepts we apply from class:

*   **Client-Server Communication:** Utilizing gRPC and protocol buffers (protobuf) for structured communication between the clients and the central server.
*   **Synchronization:** Addressing the challenge of coordinating actions across distributed nodes with independent clocks. We anticipated exploring algorithms inspired by logical clocks (like Lamport timestamps) or network time protocols (NTP) adapted for audio synchronization.
*   **Replication and Consensus:** Implementing a peer-to-peer network among clients where consensus mechanisms might be needed for maintaining a consistent playback state and handling control commands reliably.
*   **Fault Tolerance:** Designing mechanisms to detect and handle node failures gracefully, ensuring the system remains available and consistent for the remaining participants.

## Table of Contents

This engineering notebook is structured chronologically in the following parts:

* [Part 0: Introduction](#part-0-introduction) – Overview of the project, motivation, goals, and core concepts  
* [Part 1: Project Setup and Initial Architecture](#part-1-project-setup-and-initial-architecture) – Initial stack, architecture diagrams  
* [Part 2: Audio Playback Implementation](#part-2-audio-playback-implementation) – CoreAudio exploration and player implementation  
* [Part 3: Server-Client Communication](#part-3-server-client-communication-milestone-2) – gRPC setup, server logic, audio fetching  
* [Part 4: Peer-to-Peer Synchronization (Initial Concepts)](#part-4-peer-to-peer-synchronization-initial-concepts) – NTP ideas, latency estimation, protocol design  
* [Part 5: Peer-to-Peer Implementation and Challenges](#part-5-peer-to-peer-implementation-and-challenges) – gRPC service, bugs, command scheduling  
* [Part 6: Synchronization Refinement](#part-6-synchronization-refinement) – Latency-aware delays, robust scheduling logic  
* [Part 7: Testing and Integration](#part-7-testing-and-integration) – Unit/system tests, tools, challenges  
* [Part 8: Finalization and Future Work](#part-8-finalization-and-future-work) – Demo prep, documentation, future plans  

# Part 1: Project Setup and Initial Architecture

This section covers initial setup, technology choices, and architectural design.

## Technology Stack Selection

Our goal was to build a robust, reasonably performant system while using CS262 concepts. We settled on the following stack:

*   **Language: C++:** Performance characteristics and the availability of low-level system APIs, which seemed potentially necessary for fine-grained audio control and synchronization.
*   **Communication Protocol: gRPC / Protocol Buffers (protobuf):** For communication between the server and clients, and potentially between clients themselves, we opted for gRPC. Its use of HTTP/2 for transport, protobuf for interface definition and serialization, and support for bidirectional streaming made it a good candidate for efficient and structured network communication in a distributed environment. It also made sense in the context of what we learned from the course.
*   **Audio Playback (Initial): CoreAudio (macOS):** Handling audio playback reliably and with low latency is nontrivial. Given the initial project scope and the team's development environment, we decided to start with Apple's CoreAudio framework for macOS. This provided a native, powerful, albeit complex, API for audio processing. We acknowledged this introduced platform dependency (initially macOS-only) but prioritized getting a functional playback system working first. Alternatives or cross-platform libraries were considered but deferred.
*   **Timing/Synchronization Protocol: TBD (Initially Logical Clocks/NTP-inspired):** Synchronization was identified early on as a core challenge. Our initial proposal suggested exploring logical clocks (like Lamport timestamps, similar to a design exercise) or adapting Network Time Protocol (NTP) concepts. Eventually we settled on **NEED TO FILL THIS IN MIRA**

## Initial System Architecture

Our initial vision for the system architecture, captured in a diagram around the time of the proposal, depicted a client-server model augmented with peer-to-peer communication for synchronization:

![Initial System Architecture Diagram](docs/notebook/initial_diagram.png)


We were thinking:

1.  **Server:** A central entity responsible for:
    *   Hosting the audio files (e.g., WAV, MP3).
    *   Allowing clients to connect and discover available audio tracks.
    *   Serving audio data chunks to clients upon request.
    *   Potentially maintaining a list of active clients to facilitate peer discovery, although the exact mechanism for peer discovery was not fully defined initially.
2.  **Clients (Peers):** The individual devices (laptops) participating in the playback session. Each client would:
    *   Connect to the server to fetch the list of available songs and download the selected audio file, buffering it locally.
    *   Implement the audio playback logic using CoreAudio.
    *   Communicate with other clients (peers) to achieve and maintain playback synchronization.
    *   Handle user commands (play, pause, stop) and coordinate these actions with peers.
    *   Implement fault detection and recovery logic to handle peer crashes.

# Part 2: Audio Playback Implementation

One of the first major technical hurdles was implementing reliable audio playback on the client side. As decided early on, we targeted macOS using the CoreAudio framework. This section covers our exploration, implementation challenges, and the resulting audio player component, based on our from notes and discussions around April 5th and April 12th.

## Understanding CoreAudio

CoreAudio is Apple's low-level framework for handling audio on macOS and iOS. While powerful, it's known for its complexity. Our initial task was to grasp its fundamental concepts to simply play an audio file from memory or disk.

April 5th was mostly spent in the initial learning process. An important realization was understanding the role of `AudioUnit`. It's not a unit of data, but rather the core architecture component responsible for processing and playing audio. Think of it as the 'musician', while the actual audio data is the 'sheet music'.

Important to the project is the `RenderCallback` function. The `AudioUnit` invokes this callback whenever it needs more audio data to play. The callback's job is to act as the 'page turner', fetching the next chunk ('page') of audio data from our source (e.g., a loaded file buffer) and providing it to the `AudioUnit`'s input buffer. This allows for continuous playback without loading the entire audio file into memory at once and enables real-time processing if needed.

We found Apple's documentation helpful, particularly the [Audio Unit Programming Guide](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnit/TheAudioUnit.html) (though navigating Apple's developer docs can sometimes be a journey in itself).

## Implementation Details (circa April 12th)

Based on this understanding, the implementation of the `AudioPlayer` class began, primarily located in `client/src/audioplayer.cpp`, with instantiation and basic usage demonstrated in `client/src/main.cpp`. The necessary Apple frameworks included `AudioToolbox`, `CoreAudio`, and `CoreFoundation`.

Key implementation aspects included:

*   **File Handling:** Using the `AudioFile` APIs within the AudioToolbox framework to open and read audio data from various file formats supported by macOS. This abstracted away the complexities of parsing different audio codecs and containers.
*   **Buffering:** Implementing a buffering strategy is essential for smooth, glitch-free playback, especially when dealing with callbacks and potential timing variations. We might adopt a three-buffer system, likely a form of double or triple buffering, where each buffer might hold a small duration of audio (e.g., 0.5 seconds). While one buffer is being played, another can be filled by the `RenderCallback`, ensuring data is always ready for the `AudioUnit`.
*   **Callback Logic:** The `RenderCallback` function was implemented to read the appropriate segment of audio data from the loaded file (based on the current playback position) and copy it into the buffer provided by the `AudioUnit` during the callback invocation.
*   **Build System:** Integrating CoreAudio required careful handling in our CMake build system (`client/src/CMakeLists.txt` and the root `CMakeLists.txt`). We noted that CoreAudio sometimes required absolute paths, adding a minor complexity to the build configuration.

## Platform Dependency and Alternatives

Our reliance on CoreAudio inherently tied the initial client implementation to macOS. This was a conscious trade-off made for expediency and access to a powerful native framework. Discussions occurred around April 12th and 13th regarding potentially replacing CoreAudio with a cross-platform library later, but the immediate focus remained on getting the macOS version functional.

## Development Workflow

On April 12th we also detailed the typical development workflow established for building and testing the client, including the audio player component:

1.  **Build:** Navigate to the `build` directory, run `cmake ..` (potentially with flags like `-DBUILD_TESTS=ON` or `-DCMAKE_BUILD_TYPE=Debug`), and then `make`.
2.  **Run Client:** Execute the client application via `./bin/music262_client`.
3.  **Testing:** Use `ctest` from the build directory to run automated tests (including those for the audio player in `tests/unit/audioplayer.cpp`). Running the test executable directly (`./tests/audio_player_tests`) provided more verbose output for debugging.
4.  **Code Coverage:** We also set up code coverage using `lcov` via `make coverage`, with reports viewable in a browser (`open coverage_report/index.html`).

This structured approach, combining CMake, unit testing (with Google Test), and coverage analysis, was vital for managing the complexity of the C++ codebase and ensuring the audio player component functioned as expected before integrating it with the networking and synchronization logic discussed in later parts.


# Part 3: Server-Client Communication (Milestone 2)

With a basic audio playback mechanism established on the client (Part 2), the next logical step was enabling clients to fetch audio content from the central server. This involved defining and implementing the communication protocol between the server and clients using gRPC, a core task leading up to Milestone 2 (deadline April 24th).

## Server Role and Responsibilities

As outlined in the initial architecture (Part 1) and reiterated in planning notes (e.g., April 20th), the server acts as the central coordinator and content source. Its primary responsibilities included:

*   **Hosting Audio Files:** Storing the music tracks that the clients would play.
*   **Client Registration/Discovery:** Allowing clients to connect and potentially learn about other active clients in the system (though the peer discovery mechanism evolved).
*   **Serving Audio Data:** Providing lists of available songs and streaming the actual audio data to clients upon request.
*   **State Management:** Keeping track of connected clients and potentially some shared state, although the goal was to push synchronization logic primarily to the peers.

## Defining the Interface: gRPC and Protobuf

We chose gRPC with protobuf for server-client communication. This required defining the services and message types in `.proto` files. Based on the project structure observed in the GitHub repository (`src/proto/`), we defined services for actions like:

*   Registering a client with the server
*   Requesting a list of available audio tracks
*   Requesting a specific audio track

## Implementation Efforts

Leading up to the April 24th deadline we set for for Milestone 2, our focus was on implementing this server-client interaction:

*   **Server-Side:** Involved setting up the gRPC server, implementing the service methods defined in the `.proto` file (`src/server/`), managing client connections, reading audio files from disk, and streaming them back to clients in chunks.
*   **Client-Side:** Required implementing the gRPC client stub (`src/client/`), connecting to the server, calling the RPC methods (e.g., `GetSongList`, `GetSong`), receiving the streamed audio data, and feeding it into the `AudioPlayer` component developed earlier.
*   **Integration Testing:** Developing tests (`tests/`) to ensure the client could successfully connect to the server, retrieve song lists, download audio data, and potentially play it back, verifying the end-to-end flow.

## Challenges and Refinements (around Apr 24)

We ran into quite a few issues around April 24th in oru implementation phase:

*   **gRPC Build/Compile Issues:** Fixing gRPC compilation problems, possibly related to dependencies or CMake configuration, which is common when integrating complex libraries.
*   **Connection Feedback:** Clients needed clearer feedback on whether their connection attempt to the server was successful before proceeding.
*   **Peer Discovery:** Clients needed a way to discover other peers connected to the server.
*   **Song Selection:** The mechanism shifted towards selecting songs by number (ID) rather than filename, simplifying the interface.
*   **Caching (Stretch Goal):** The idea of caching downloaded songs on the client to avoid re-downloading could be a potential future improvement.

Successfully implementing this server-client communication was a critical step. It ensured clients could obtain the necessary audio data, paving the way for the next major challenge: synchronizing playback between these clients, which will be discussed in the following parts.

# Part 4: Peer-to-Peer Synchronization (Initial Concepts)

Having established server-client communication for fetching audio (Part 3), the project's core challenge came into focus: synchronizing playback across multiple, independent clients (peers). This section captures the initial brainstorming and design ideas for the peer-to-peer (P2P) communication and synchronization logic, primarily based on discussions and notes from around April 13th onward.

## The Synchronization Problem

The fundamental goal is to have all clients play the same audio track such that the sound output is perceived as simultaneous. This is difficult because:

*   **Independent Clocks:** Each client runs on a separate machine with its own hardware clock, which may drift relative to others over time.
*   **Network Latency:** Messages sent between clients take a non-zero and potentially variable amount of time to arrive.
*   **Processing Delays:** There are delays associated with processing incoming messages, interacting with the audio playback system, etc.

Simply telling all clients to "play at time T" is insufficient because their local clocks might disagree on what "time T" is, and the command itself will arrive at different times due to network latency.

## Initial Ideas

We started discussing around April 13th how to approach synchronization.

1.  **Latency Estimation (NTP-inspired):** A key idea was to estimate the network latency between pairs of clients. The proposed method was similar to the basic principle of the Network Time Protocol (NTP):
    *   Client A sends a ping request to Client B at its local time `t1`.
    *   Client B receives the ping at its local time `t2` and immediately sends a pong response back at its local time `t3`.
    *   Client A receives the pong at its local time `t4`.
    *   The round-trip time (RTT) is `(t4 - t1) - (t3 - t2)`. Assuming symmetric latency, the one-way latency could be estimated as RTT / 2.
    *   The clock offset between B and A could be estimated as `((t2 - t1) + (t3 - t4)) / 2`. We initiall thought maybe we could use a pinging process multiple times (e.g., 20 times) and averaging the results to get a more stable latency estimate for each peer-to-peer connection. This estimated latency would be crucial for scheduling playback actions.

2.  **Command Scheduling:** Once latency estimates (`L_ab` for latency from A to B) are available, when Client A wants to initiate an action (e.g., play at position `P` starting now, `t_now_A`), it would send a command to Client B.
    *   Client A would start playing immediately (or after a small, fixed delay).
    *   Client B, upon receiving the command, would know the estimated latency `L_ab`. It could then schedule its own playback to start at its local time corresponding to `t_now_A + L_ab`, effectively trying to compensate for the message delay.
    *   We considered the idea of waiting for the estimated latency duration *before* playing locally when sending a command, potentially simplifying the logic on the receiving end. This suggests coordinating actions based on an agreed-upon future start time relative to the command broadcast.

3.  **Wall Clocks vs. Logical Clocks:** We discussed whether to rely on system wall clocks (adjusted using NTP-like methods) or logical clocks (like Lamport timestamps) for ordering events. While logical clocks are excellent for determining causal order, they don't directly help with aligning actions in real-time. We shifted towards using wall clocks combined with latency estimation and offset correction, closer to NTP's approach, to achieve real-time synchronization.

## Defining the Peer-to-Peer Interface

To support this, we considered a gRPC interface for client-to-client communication. While brainstorming, we outlined a `ClientHandler` service with messages like:

```protobuf
// Example: src/proto/client_service.proto
syntax = "proto3";

package music_distributed;

service ClientHandler {
  // Used for latency measurement
  rpc Ping(PingRequest) returns (PingResponse);

  // Send playback commands (play, pause, etc.)
  rpc SendMusicCommand(MusicCommand) returns (CommandResponse);

  // Request current playback position from a peer
  rpc GetPosition(GetPositionRequest) returns (GetPositionResponse);
}

message PingRequest {
  // Could include sender timestamp if needed for more complex NTP
}

message PingResponse {
  // Could include receiver/sender timestamps
}

message MusicCommand {
  string action = 1; // e.g., "start", "pause", "resume", "stop"
  double wall_clock = 2; // Sender's wall clock time when action is initiated/scheduled
  int64 position_nanos = 3; // Playback position in nanoseconds (or other precise unit)
  // Potentially add sender ID
}

message CommandResponse {
  bool success = 1;
}

message GetPositionRequest {}

message GetPositionResponse {
  int64 position_nanos = 1;
}

```

This interface includes RPCs for pinging (latency measurement), sending music commands (with action, timestamp, and position), and querying the current playback position of a peer.

The client implementation would need to:

*   Maintain connections (stubs) to all other known peers.
*   Periodically ping peers to establish and update latency estimates (`active_clients` dictionary).
*   Implement handlers for incoming RPCs (`handle_ping_request`, `handle_music_request`).
*   Implement logic to broadcast commands to all peers when a user initiates an action (`user_action` function), potentially scheduling the local action based on estimated latencies.

These initial concepts laid the foundation for the peer-to-peer synchronization mechanism.


# Part 5: Peer-to-Peer Implementation and Challenges

Building on the synchronization concepts outlined in Part 4, this section details the implementation phase of the peer-to-peer (P2P) communication and synchronization logic. This work spanned roughly starting from April 24th, including efforts on the gRPC interface, client logic, and addressing the numerous challenges that arose when trying to make distributed clients play audio in sync.

## Implementing the P2P Communication Layer

The core of the P2P system involved clients directly communicating with each other using the gRPC `ClientHandler` service defined earlier. Key implementation tasks included:

*   **gRPC Service Implementation:** Each client needed to run a gRPC server instance to handle incoming requests (like `Ping`, `SendMusicCommand`, `GetPosition`) from other peers. This involved implementing the `ClientHandler` service interface (`src/client/client_handler_impl.cpp` or similar).
*   **Peer Discovery and Connection Management:** Clients needed to obtain the addresses of other peers (likely facilitated by the central server during registration, as discussed in Part 3) and establish gRPC client connections (stubs) to them. We might maintain a dictionary of peer addresses and their estimated latencies (`active_clients` or `peer_latency_` map).
*   **Latency Measurement:** Implementing the NTP-inspired pinging mechanism to estimate RTT and one-way latency between each pair of connected peers. This likely involved sending `PingRequest` messages periodically or upon connection establishment and processing the `PingResponse` to calculate and store latency estimates.

## Handling Playback Commands

Once the communication channels and latency estimates were in place, the logic for handling playback commands (`MusicCommand`) became central:

*   **Broadcasting Commands:** When a user initiated an action (e.g., play, pause) on one client, that client needed to broadcast the corresponding `MusicCommand` (containing the action, its current wall clock time, and the target playback position) to all other known peers.
*   **Receiving and Scheduling Commands:** Upon receiving a `MusicCommand`, a client needed to:
    1.  Parse the command (action, sender's wall clock time, position).
    2.  Estimate the sender's intended start time in its own local clock domain, factoring in the estimated network latency from the sender.
    3.  Schedule the action (e.g., telling the `AudioPlayer` to play, pause, or seek) to occur at that calculated local time.
    This scheduling is crucial for compensating for network delays.

## Challenges Encountered (Milestone 3/4, Apr 24/25/28)

*   **Merge Conflicts (Apr 24):** Integrating code from different team members working on related components (server, client, proto files) inevitably led to merge conflicts, requiring careful resolution.
*   **gRPC/Proto Issues (Apr 24/25):** Fixing the proto file definition and gRPC implementation. The initial proto compiled but didn't function correctly, requiring debugging.
*   **Client Logic Implementation (Apr 25):** Filling in the client-side logic to correctly handle incoming commands and interact with the audio player based on the synchronization protocol.
*   **Message Header Parsing (Apr 25):** Needing to change message header parsing, encoding and decoding wasn't working super well.
*   **Synchronization Bugs (Apr 24):** Issues with the actual synchronization:
    *   **One Client Plays, Others Don't:** When one client initiated playback, the command wasn't correctly triggering playback on other clients.
    *   **Default Leader Issue:** The client initiating the connection (`join`) seemed to become a default leader, preventing other clients from taking control.
    Problems in either the command broadcasting, the receiving/scheduling logic, or the interaction with the `AudioPlayer`.
*   **Refining Synchronization Logic (Apr 28):** Editing the synchronization logic within the `peer_network` component. This involved defining how clients should adjust their internal clocks or schedule actions based on received commands and estimated latencies, including considering the maximum latency among all peers when broadcasting commands to ensure even the slowest peer could potentially sync up.


# Part 6: Synchronization Refinement

Part 5 detailed the initial implementation of peer-to-peer synchronization and the challenges encountered, particularly getting clients to react correctly and consistently to playback commands. We then focused on on fixing the synchronization logic within the `peer_network` component, addressing the bugs observed earlier.

## Refining the Synchronization Goals (Apr 28)

We aimed for these goals:

1.  **Accurate Command Scheduling (Receiving):** When a client receives a `MusicCommand` from a peer, it must accurately calculate the intended execution time in its local clock domain and schedule the action (play, pause, seek) accordingly. This requires reliable latency estimation and clock offset awareness (or a protocol that mitigates the need for precise offset calculation).
2.  **Coordinated Command Broadcasting (Sending):** When a client broadcasts a `MusicCommand`, it needs to ensure that all receiving peers (including itself) execute the command at roughly the same real time. This might involve delaying the local action or including specific timing information in the command.

## Areas for Edits

We focused on a few differente edits for the protocol/logic.

*   **Leveraging Latency Estimates:** The prerequisite for the refined logic is that each client maintains an up-to-date map of estimated latencies (`peer_latency_`) to all other known peers, likely derived from the NTP-inspired pinging mechanism discussed in Part 4.
*   **Tracking Maximum Latency:** Each client should also track the *maximum* latency (`max_latency`) it experiences with any of its connected peers. This value represents the longest expected delay for a command to reach any participant in the current session.
*   **Including `max_latency` in Commands:** When broadcasting a `MusicCommand`, the sender should include its currently observed `max_latency` value within the message payload.

For example:
```protobuf
message MusicCommand {
  string action = 1; 
  int64 position_nanos = 2; // Renamed for clarity, assuming nanosecond precision
  double sender_wall_clock = 3; // Sender's wall clock when command is initiated
  double max_latency_seconds = 4; // Sender's observed max latency to any peer
}
```
*   **Sender-Side Logic (Broadcasting):** When Client A sends a `MusicCommand`:
    1.  It determines its current `max_latency` to any peer.
    2.  It includes this `max_latency` in the `MusicCommand` message.
    3.  It broadcasts the command to all peers.
    4.  Crucially, Client A **waits** for a duration equal to `max_latency` *before* executing the command locally on its own `AudioPlayer`. This ensures that even the peer with the highest latency has theoretically received the command by the time the sender executes it.

*   **Receiver-Side Logic (Handling Command):** When Client B receives a `MusicCommand` from Client A:
    1.  It extracts the action, position, sender's wall clock, and sender's `max_latency`.
    2.  It calculates the *scheduled execution time* in its local clock domain. A simple approach, assuming the sender waited `max_latency`, is to execute the command almost immediately upon receipt, as the sender's delay was intended to compensate for the network transit time. However, a more robust approach might still involve estimating the sender's intended execution time (`sender_wall_clock + sender_max_latency`) and translating that into the receiver's local clock, potentially using clock offset estimates if available.
    3.  We might adjust based on the received information. The core idea is that the sender's `max_latency` delay provides a buffer, allowing receivers to schedule the action more reliably relative to the sender's execution time.

## Implications and Trade-offs

This  approach where the sender delays its own action based on the maximum observed peer latency aims to create a common execution point in the near future for all clients.

*   **Pros:** Simplifies the receiver's logic, as much of the compensation happens on the sender's side. It directly addresses the issue of commands arriving at different times.
*   **Cons:**
    *   Introduces a delay (equal to `max_latency`) between a user initiating an action and that action actually occurring, which could impact responsiveness, especially in networks with high or variable maximum latencies.
    *   Relies heavily on the accuracy of the latency estimates. Inaccurate estimates could lead to synchronization drift.
    *   Doesn't fully solve clock drift between peers over longer periods; periodic resynchronization or adjustments might still be necessary.

This edit is an important step (a whole part of its own!) because it moves from basic concepts to a concrete algorithm involving sender-side delays based on network conditions. However, it does depend  on the quality of latency measurements and the stability of the network. Further testing and potential adjustments were needed, as discussed in the subsequent parts covering testing and finalization.

# Part 7: Testing and Integration

This section focuses on the testing strategies and integration efforts undertaken throughout the project, particularly on April 29th onwards.

## Importance of Testing

*   **Unit Testing:** Verifying the correctness of individual components in isolation, such as the `AudioPlayer`, specific utility functions, or parts of the communication logic.
*   **Integration Testing:** Ensuring that different components interact correctly, for example, verifying that the client can successfully fetch audio from the server or that peer-to-peer commands are processed as expected.
*   **System Testing (Manual/Demo):** Evaluating the end-to-end functionality of the system with multiple clients running, assessing the quality of synchronization, and testing fault tolerance scenarios.

## Testing Framework and Infrastructure

As noted in Part 2 and reflected in our `tests/` directory, we adopted standard C++ testing tools:

*   **Google Test (GTest):** A widely used C++ testing framework for writing unit tests. The presence of `tests/unit/audioplayer.cpp` confirms its use for testing components like the audio player.
*   **CMake/CTest:** The build system (CMake) was configured to build and run tests using CTest. The command `ctest --output-on-failure` was used to execute the test suite.
*   **Code Coverage (lcov):** We integrated `lcov` to measure test coverage, providing insights into which parts of the codebase were exercised by the tests. The `make coverage` command generated HTML reports.

## Specific Testing Efforts

*   **Audio Player Unit Tests (Apr 12):** Early testing focused on the `AudioPlayer` component, ensuring it could load files, manage buffers, and interact with the CoreAudio APIs correctly.
*   **Server-Client Integration Tests (Milestone 2 - Apr 24):** Integration tests to verify the server-client communication involved starting a server instance, having a client connect, request a song list, download audio data, and potentially check the integrity or playback initiation of the received data.
*   **Peer-to-Peer Testing (Milestone 3/4 - Apr 24/28):** Testing the P2P synchronization was inherently more complex. It involves:
    *   Unit tests for the latency estimation logic
    *   Integration tests simulating multiple clients, sending commands, and verifying the timing of actions or the resulting state
    *   Manual testing by running multiple client instances locally or on different machines to observe synchronization quality and debug issues like the 

## Integration Challenges

*   **Build System Complexity:** Managing dependencies (gRPC, Protobuf, CoreAudio, GTest) within CMake requires careful configuration to ensure consistent builds across development environments and in CI/CD pipelines
*   **Environment Differences:** Differences between development machines could lead to issues that only appear during integration or system testing
*   **Debugging Distributed Behavior:** Debugging synchronization issues or race conditions across multiple running processes is really difficult, so we're implementing as much logging (`spdlog`) as we can.

# Part 8: Finalization and Future Work

## Final Push: Documentation, Testing, Demo (Apr 29)

**TBD**

## Reflections and Future Work

**TBD**