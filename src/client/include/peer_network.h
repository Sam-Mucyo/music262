#ifndef PEER_NETWORK_H
#define PEER_NETWORK_H

#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "client.h"
#include "peer_service.h"
#include "../proto/audio_sync.pb.h" 

class PeerNetwork {
 public:
  explicit PeerNetwork(AudioClient* client);
  ~PeerNetwork();

  bool StartServer(int port);
  void StopServer();
  bool ConnectToPeer(const std::string& peer_address);
  bool DisconnectFromPeer(const std::string& peer_address);
  void DisconnectFromAllPeers();
  std::vector<std::string> GetConnectedPeers() const;
  float CalculateAverageOffset() const;
  void BroadcastGossip();
  void BroadcastCommand(const std::string& action, int position);
  int GetServerPort() const { return server_port_; }

 private:
  AudioClient* client_;
  std::unique_ptr<PeerService> service_;
  std::unique_ptr<grpc::Server> server_;
  std::thread server_thread_;
  bool server_running_;
  int server_port_;
  mutable std::mutex peers_mutex_;
  std::unordered_map<std::string, std::unique_ptr<client::ClientHandler::Stub>> peer_stubs_;
  float avg_offset_ = 0.0f;
};

#endif  // PEER_NETWORK_H