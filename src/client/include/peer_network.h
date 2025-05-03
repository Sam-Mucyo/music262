#pragma once

#include <grpcpp/grpcpp.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_sync.grpc.pb.h"

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

  // Calculate average offset from peers
  float CalculateAverageOffset() const;

  // Get the average offset from peers
  float GetAverageOffset() const { return avg_offset_; }

  // Get the maximum round-trip time
  float GetRTT() const { return rtt_; }

  // Broadcast a command to all connected peers
  void BroadcastCommand(const std::string& action);

  // Broadcast gossip to all connected peers
  void BroadcastGossip();

 private:
  // Main client reference
  AudioClient* client_;  // Non-owning pointer

  // Peer server
  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<PeerService> service_;
  std::thread server_thread_;
  bool server_running_;
  int server_port_;

  // Peer connections
  std::map<std::string, std::unique_ptr<client::ClientHandler::Stub>>
      peer_stubs_;
  mutable std::mutex peers_mutex_;
  mutable float rtt_;  // Maximum round-trip time for ping
  mutable float avg_offset_;  // Average offset from peers
};
