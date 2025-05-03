#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "audio_sync.grpc.pb.h"
#include "sync_clock.h"

// Forward declaration
class AudioClient;

// Command queue entry
struct QueuedCommand {
  client::MusicCommand command;
  std::chrono::time_point<std::chrono::system_clock> timestamp;
  bool operator<(const QueuedCommand& other) const {
    return timestamp >
           other.timestamp;  // Priority queue is max-heap by default
  }
};

// Handler for peer-to-peer client service
class PeerService final : public client::ClientHandler::Service {
 public:
  PeerService(AudioClient* client);

  grpc::Status Ping(grpc::ServerContext* context,
                    const client::PingRequest* request,
                    client::PingResponse* response) override;

  grpc::Status Gossip(grpc::ServerContext* context,
                      const client::GossipRequest* request,
                      client::GossipResponse* response) override;

  grpc::Status SendMusicCommand(grpc::ServerContext* context,
                                const client::MusicCommand* request,
                                client::CommandStatus* response) override;

  grpc::Status GetPosition(grpc::ServerContext* context,
                           const client::GetPositionRequest* request,
                           client::GetPositionResponse* response) override;

  // New streaming RPC handlers
  grpc::Status StreamCommands(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<client::CommandStatus, client::MusicCommand>*
          stream) override;

  grpc::Status MonitorNetwork(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<client::NetworkStatus, client::NetworkStatus>*
          stream) override;

 private:
  AudioClient* client_;  // Non-owning pointer to main client
};

// Stream state to manage bidirectional stream with a peer
class PeerStreamState {
 public:
  PeerStreamState(const std::string& peer_address);
  ~PeerStreamState();

  // Initialize stream connection to peer
  bool InitializeStream(std::shared_ptr<client::ClientHandler::Stub> stub);

  // Close stream connection
  void CloseStream();

  // Send command through stream
  bool SendCommand(const client::MusicCommand& command);

  // Process received responses on this stream
  void ProcessResponses();

  // Check if stream is active
  bool IsActive() const { return stream_active_; }

  // Get peer address
  const std::string& GetPeerAddress() const { return peer_address_; }

 private:
  std::string peer_address_;
  std::unique_ptr<grpc::ClientContext> context_;
  std::shared_ptr<client::ClientHandler::Stub> stub_;
  std::unique_ptr<
      grpc::ClientReaderWriter<client::MusicCommand, client::CommandStatus>>
      stream_;
  std::atomic<bool> stream_active_;
  std::thread response_thread_;
};

// Network monitoring thread for background offset calculation
class NetworkMonitor {
 public:
  NetworkMonitor(SyncClock& sync_clock);
  ~NetworkMonitor();

  // Start background monitoring
  void Start();

  // Stop background monitoring
  void Stop();

  // Add peer for monitoring
  void AddPeer(const std::string& peer_address,
               std::shared_ptr<client::ClientHandler::Stub> stub);

  // Remove peer from monitoring
  void RemovePeer(const std::string& peer_address);

 private:
  // Background thread function
  void MonitorThread();

  // Calculate offsets for a specific peer
  void CalculatePeerOffset(const std::string& peer_address,
                           std::shared_ptr<client::ClientHandler::Stub> stub);

  SyncClock& sync_clock_;
  std::atomic<bool> running_;
  std::thread monitor_thread_;
  std::mutex peers_mutex_;
  std::map<std::string, std::shared_ptr<client::ClientHandler::Stub>>
      monitored_peers_;

  // How often to recalculate offsets (in milliseconds)
  const int recalculation_interval_ms_ = 5000;
};

// Class to manage peer-to-peer connections
class PeerNetwork {
 public:
  PeerNetwork(AudioClient* client);
  ~PeerNetwork();

  // Start a server to accept peer connections
  bool StartServer(int port = 50052);

  // Stop the server
  void StopServer();

  // Connect to a peer client
  bool ConnectToPeer(const std::string& peer_address);

  // Disconnect from a peer client
  bool DisconnectFromPeer(const std::string& peer_address);

  // Disconnect from all peers
  void DisconnectFromAllPeers();

  // Get the server port
  int GetServerPort() const { return server_port_; }

  // Get list of connected peers
  std::vector<std::string> GetConnectedPeers() const;

  // Calculate average offset from peers (legacy, now handled by NetworkMonitor)
  float CalculateAverageOffset() const;

  // Get the average offset from peers
  float GetAverageOffset() const { return sync_clock_.GetAverageOffset(); }

  // Broadcast a command to all connected peers asynchronously
  void BroadcastCommand(const std::string& action, int position);

  // Broadcast gossip to all connected peers asynchronously
  void BroadcastGossip();

  // Get the sync clock instance
  SyncClock& GetSyncClock() { return sync_clock_; }
  const SyncClock& GetSyncClock() const { return sync_clock_; }

 private:
  // Asynchronous command processing
  void ProcessCommandQueue();

  // Generate a unique command ID
  int GenerateCommandId();

  // Main client reference
  AudioClient* client_;  // Non-owning pointer

  // Peer server
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<PeerService> service_;
  std::thread server_thread_;
  bool server_running_;
  int server_port_;

  // Peer connections (legacy stubs for compatibility)
  std::map<std::string, std::shared_ptr<client::ClientHandler::Stub>>
      peer_stubs_;
  mutable std::mutex peers_mutex_;

  // Streaming peer connections
  std::map<std::string, std::unique_ptr<PeerStreamState>> peer_streams_;
  mutable std::mutex streams_mutex_;

  // Command queue for async processing
  std::priority_queue<QueuedCommand> command_queue_;
  mutable std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::thread queue_thread_;
  std::atomic<bool> queue_running_;

  // Command ID tracking
  std::atomic<int> next_command_id_;

  // Network monitor for background offset calculation
  std::unique_ptr<NetworkMonitor> network_monitor_;

  // Sync clock for time synchronization
  SyncClock sync_clock_;
};