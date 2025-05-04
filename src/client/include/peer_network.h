#pragma once

#include <grpcpp/grpcpp.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_sync.grpc.pb.h"
#include "peer_service_interface.h"
#include "sync_clock.h"

// Forward declaration
class AudioClient;

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
                                const client::MusicRequest* request,
                                client::MusicResponse* response) override;

  grpc::Status GetPosition(grpc::ServerContext* context,
                           const client::GetPositionRequest* request,
                           client::GetPositionResponse* response) override;

 private:
  AudioClient* client_;  // Non-owning pointer to main client
};

// Class to manage peer-to-peer connections
class PeerNetwork {
 public:
  PeerNetwork(
      AudioClient* client,
      std::unique_ptr<music262::PeerServiceInterface> peer_service = nullptr);
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

  // Calculate average offset from peers
  float CalculateAverageOffset();

  // Get the average offset from peers
  float GetAverageOffset() const { return sync_clock_.GetAverageOffset(); }

  // Broadcast a command to all connected peers
  void BroadcastCommand(const std::string& action, int position);

  // Broadcast gossip to all connected peers
  void BroadcastGossip();

  // Broadcast a load command synchronously and wait for all peers to load
  bool BroadcastLoad(int song_num);

  // Get the sync clock instance
  SyncClock& GetSyncClock() { return sync_clock_; }
  const SyncClock& GetSyncClock() const { return sync_clock_; }

 private:
  // Main client reference
  AudioClient* client_;  // Non-owning pointer

  // Peer server
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<PeerService> service_;
  std::thread server_thread_;
  bool server_running_;
  int server_port_;

  // Peer service interface for making peer requests
  std::unique_ptr<music262::PeerServiceInterface> peer_service_;

  // Connected peers
  std::vector<std::string> connected_peers_;
  mutable std::mutex peers_mutex_;

  // Sync clock for time synchronization
  SyncClock sync_clock_;
};
