#include "include/peer_network.h"

#include <ifaddrs.h>

#include <chrono>

#include "include/client.h"
#include "logger.h"

// Helper: get first non-loopback IPv4 address
std::string GetLocalIPAddress() {
  struct ifaddrs* ifas = nullptr;
  if (getifaddrs(&ifas) == -1) return "";
  for (auto* ifa = ifas; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    std::string name(ifa->ifa_name);
    if (name == "lo0") continue;
    auto* addr = (struct sockaddr_in*)ifa->ifa_addr;
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf))) {
      freeifaddrs(ifas);
      return std::string(buf);
    }
  }
  freeifaddrs(ifas);
  return "";
}

// PeerService implementation
PeerService::PeerService(AudioClient* client) : client_(client) {
  LOG_DEBUG("PeerService initialized");
}

grpc::Status PeerService::Ping(grpc::ServerContext* context,
                               const client::PingRequest* request,
                               client::PingResponse* response) {
  LOG_DEBUG("Received ping request from peer: {}", context->peer());

  // Set t1 to current time when received
  auto t1 = SyncClock::GetCurrentTimeNs();

  // Set t2 to current time when about to respond
  auto t2 = SyncClock::GetCurrentTimeNs();

  // Set values in response
  response->set_t1(t1);
  response->set_t2(t2);

  return grpc::Status::OK;
}

grpc::Status PeerService::Gossip(grpc::ServerContext* context,
                                 const client::GossipRequest* request,
                                 client::GossipResponse* response) {
  LOG_INFO("Received GossipRequest from peer: {}", context->peer());

  if (!client_) {
    LOG_ERROR("Client not initialized in PeerService");
    return grpc::Status(grpc::StatusCode::INTERNAL, "Client not initialized");
  }

  std::shared_ptr<PeerNetwork> network_ptr = client_->GetPeerNetwork();
  if (!network_ptr) {
    LOG_ERROR("Peer network not available");
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Peer network not available");
  }
  // use raw pointer or keep using shared_ptr directly
  PeerNetwork* network = network_ptr.get();
  if (!network) {
    LOG_ERROR("Peer network not available");
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Peer network not available");
  }

  // Clear and reconnect
  network->DisconnectFromAllPeers();
  for (const auto& addr : request->peer_list()) {
    // Skip itself
    if (addr ==
        GetLocalIPAddress() + ":" + std::to_string(network->GetServerPort())) {
      continue;
    }
    network->ConnectToPeer(addr);
  }

  // Calculate average offset for future use
  // network->SetAverageOffset(CalculateAverageOffset());
  return grpc::Status::OK;
}

grpc::Status PeerService::SendMusicCommand(grpc::ServerContext* context,
                                           const client::MusicRequest* request,
                                           client::MusicResponse* response) {
  if (!client_) {
    LOG_ERROR("Client not initialized in PeerService");
    return grpc::Status(grpc::StatusCode::INTERNAL, "Client not initialized");
  }

  const std::string& action = request->action();
  int position = request->position();
  int64_t wait_ms = request->wait_time_ms();

  // Handle load actions immediately
  if (action == "load") {
    int song_num = request->song_num();
    LOG_INFO("Received load command from peer {}: song_num={}", context->peer(),
             song_num);
    client_->SetCommandFromBroadcast(true);
    bool ok = client_->LoadAudio(song_num);
    client_->SetCommandFromBroadcast(false);
    if (!ok) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "LoadAudio failed");
    }
    return grpc::Status::OK;
  }

  LOG_INFO(
      "Received music command from peer {}: action={}, position={}, wait_ms={}",
      context->peer(), action, position, wait_ms);

  // Mark that this command came from a broadcast to prevent echo
  client_->SetCommandFromBroadcastAction(action);
  client_->SetCommandFromBroadcast(true);

  std::thread([client = client_, action, position, wait_ms]() {
    if (wait_ms > 0)
      std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    if (action == "play")
      client->Play();
    else if (action == "pause")
      client->Pause();
    else if (action == "resume") {
      // Reset to broadcaster's position to prevent drift
      client->GetPlayer().set_position(position);
      client->Resume();
    } else if (action == "stop")
      client->Stop();
    else
      LOG_WARN("Unknown command from peer: {}", action);
    client->SetCommandFromBroadcast(false);
    client->SetCommandFromBroadcastAction(" ");
    LOG_DEBUG("Music command executed successfully: {}", action);
  }).detach();
  return grpc::Status::OK;
}

grpc::Status PeerService::GetPosition(grpc::ServerContext* context,
                                      const client::GetPositionRequest* request,
                                      client::GetPositionResponse* response) {
  if (!client_) {
    LOG_ERROR("Client not initialized in PeerService");
    return grpc::Status(grpc::StatusCode::INTERNAL, "Client not initialized");
  }

  unsigned int position = client_->GetPlayer().get_position();
  response->set_position(position);

  LOG_DEBUG("Position request from peer {}, current position: {}",
            context->peer(), position);

  return grpc::Status::OK;
}

grpc::Status PeerService::Exit(grpc::ServerContext* context,
                               const client::ExitRequest* request,
                               client::ExitResponse* response) {
  std::string peer = context->peer();
  // strip protocol prefix (ipv4: or ipv6:)
  if (peer.rfind("ipv4:", 0) == 0 || peer.rfind("ipv6:", 0) == 0)
    peer = peer.substr(peer.find(':') + 1);
  auto network = client_->GetPeerNetwork();
  if (network) {
    network->DisconnectFromPeer(peer);
    LOG_INFO("Removed peer {} on Exit notification", peer);
  }
  return grpc::Status::OK;
}

// PeerNetwork implementation
PeerNetwork::PeerNetwork(
    AudioClient* client,
    std::unique_ptr<music262::PeerServiceInterface> peer_service)
    : client_(client),
      server_(nullptr),
      service_(nullptr),
      server_running_(false),
      server_port_(0),
      peer_service_(peer_service ? std::move(peer_service)
                                 : music262::CreatePeerService()) {
  LOG_DEBUG("PeerNetwork initialized");
}

PeerNetwork::~PeerNetwork() {
  LOG_DEBUG("PeerNetwork shutting down");
  // Notify peers that we are exiting
  BroadcastExit();
  StopServer();
  DisconnectFromAllPeers();
}

bool PeerNetwork::StartServer(int port) {
  if (server_running_) {
    LOG_INFO("Peer server already running on port {}", server_port_);
    return true;
  }

  server_port_ = port;

  // Create service
  service_ = std::make_unique<PeerService>(client_);

  // Build and start server
  std::string server_address = "0.0.0.0:" + std::to_string(port);
  grpc::ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(service_.get());

  LOG_INFO("Starting peer server on {}", server_address);
  server_ = builder.BuildAndStart();
  if (!server_) {
    LOG_ERROR("Failed to start peer server on port {}", port);
    return false;
  }

  LOG_INFO("Peer server started successfully on {}", server_address);

  // Start server thread
  server_running_ = true;
  server_thread_ = std::thread([this]() {
    LOG_DEBUG("Peer server thread started");
    server_->Wait();
    LOG_DEBUG("Peer server thread exiting");
    server_running_ = false;
  });

  return true;
}

void PeerNetwork::StopServer() {
  if (!server_running_) {
    LOG_DEBUG("Peer server not running, nothing to stop");
    return;
  }

  LOG_INFO("Stopping peer server");
  server_->Shutdown();

  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  server_running_ = false;
  LOG_INFO("Peer server stopped");
}

bool PeerNetwork::ConnectToPeer(const std::string& peer_address) {
  LOG_INFO("Connecting to peer: {}", peer_address);

  // Check if already connected
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = std::find(connected_peers_.begin(), connected_peers_.end(),
                        peer_address);
    if (it != connected_peers_.end()) {
      LOG_INFO("Already connected to peer: {}", peer_address);
      return true;
    }
  }

  // Test the connection with a ping
  int64_t t1 = 0, t2 = 0;
  if (!peer_service_->Ping(peer_address, t1, t2)) {
    LOG_ERROR("Failed to connect to peer {}", peer_address);
    return false;
  }

  // Store the peer address
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    connected_peers_.push_back(peer_address);
    LOG_INFO("Connected to peer: {}", peer_address);
  }

  return true;
}

bool PeerNetwork::DisconnectFromPeer(const std::string& peer_address) {
  LOG_INFO("Disconnecting from peer: {}", peer_address);

  std::lock_guard<std::mutex> lock(peers_mutex_);
  auto it =
      std::find(connected_peers_.begin(), connected_peers_.end(), peer_address);
  if (it != connected_peers_.end()) {
    connected_peers_.erase(it);
    LOG_INFO("Disconnected from peer: {}", peer_address);
    return true;
  }

  LOG_WARN("Peer not found: {}", peer_address);
  return false;
}

void PeerNetwork::DisconnectFromAllPeers() {
  LOG_INFO("Disconnecting from all peers");

  std::lock_guard<std::mutex> lock(peers_mutex_);
  size_t count = connected_peers_.size();
  connected_peers_.clear();

  LOG_INFO("Disconnected from {} peers", count);
}

std::vector<std::string> PeerNetwork::GetConnectedPeers() const {
  std::lock_guard<std::mutex> lock(peers_mutex_);
  return connected_peers_;
}

float PeerNetwork::CalculateAverageOffset() {
  std::vector<std::string> peer_list;
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    if (connected_peers_.empty()) {
      LOG_DEBUG("No peers to calculate offset with");
      return 0.0f;
    }

    // Make a copy of peer addresses to avoid holding the mutex during RPC calls
    peer_list = connected_peers_;
  }

  LOG_DEBUG("Calculating average offset with {} peers", peer_list.size());

  // Collect round-trip times and clock offsets from each peer
  std::vector<float> offsets;
  std::vector<float> rtts;  // round-trip times in ms

  const int NUM_SAMPLES = 5;  // Number of ping samples per peer

  for (const auto& peer_address : peer_list) {
    for (int i = 0; i < NUM_SAMPLES; i++) {
      // Record t0 (client send time)
      auto t0 = SyncClock::GetCurrentTimeNs();

      // Send ping
      int64_t t1 = 0, t2 = 0;
      if (!peer_service_->Ping(peer_address, t1, t2)) {
        LOG_WARN("Failed to ping peer {} during offset calculation",
                 peer_address);
        continue;
      }

      // Record t3 (client receive time)
      auto t3 = SyncClock::GetCurrentTimeNs();

      // Calculate round-trip time: (t3 - t0) - (t2 - t1)
      float rtt = static_cast<float>((t3 - t0) - (t2 - t1)) / 1000000.0f;
      rtts.push_back(rtt);

      // Calculate clock offset: ((t1 - t0) + (t2 - t3)) / 2
      float offset = static_cast<float>((t1 - t0) + (t2 - t3)) / 2.0f;
      offsets.push_back(offset);

      LOG_DEBUG("Ping sample {}/{} to {}: RTT={:.2f}ms, offset={:.2f}ns", i + 1,
                NUM_SAMPLES, peer_address, rtt, offset);

      // Add a small delay between pings
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  if (offsets.empty()) {
    LOG_WARN("No valid offset measurements collected");
    return 0.0f;
  }

  // Calculate average offset
  float sum_offset = 0.0f;
  for (float offset : offsets) {
    sum_offset += offset;
  }
  float avg_offset = sum_offset / offsets.size();

  // Calculate average RTT
  float sum_rtt = 0.0f;
  for (float rtt : rtts) {
    sum_rtt += rtt;
  }
  float avg_rtt = sum_rtt / rtts.size();

  LOG_INFO("Average network RTT: {:.2f}ms, clock offset: {:.2f}ns", avg_rtt,
           avg_offset);

  // Update the sync clock with the new offset
  sync_clock_.SetAverageOffset(avg_offset);

  return avg_offset;
}

void PeerNetwork::BroadcastGossip() {
  std::vector<std::string> peer_list = GetConnectedPeers();
  if (peer_list.empty()) {
    LOG_DEBUG("No peers to broadcast gossip to");
    return;
  }

  LOG_INFO("Broadcasting gossip to {} peers", peer_list.size());

  // Create the full peer list including self
  std::vector<std::string> full_peer_list = peer_list;

  // Add self to the peer list
  if (server_running_) {
    std::string self_address =
        GetLocalIPAddress() + ":" + std::to_string(server_port_);
    full_peer_list.push_back(self_address);
  }

  // Send gossip to each peer
  for (const auto& peer : peer_list) {
    if (!peer_service_->Gossip(peer, full_peer_list)) {
      LOG_ERROR("Failed to send gossip to {}", peer);
    } else {
      LOG_INFO("Sent gossip to {}", peer);
    }
  }

  // Calculate average offset for future use
  CalculateAverageOffset();
}

bool PeerNetwork::BroadcastLoad(int song_num) {
  std::vector<std::string> peers = GetConnectedPeers();
  if (peers.empty()) {
    LOG_DEBUG("No peers to broadcast load to");
    return true;
  }

  int success = 0;
  for (const auto& peer : peers) {
    if (peer_service_->SendMusicCommand(peer, "load", 0, 0, song_num)) {
      success++;
    } else {
      LOG_ERROR("Failed to send load command to {}", peer);
    }
  }

  LOG_INFO("Load broadcast complete: {}/{} peers", success, peers.size());
  return success == static_cast<int>(peers.size());
}

void PeerNetwork::BroadcastCommand(const std::string& action, int position) {
  // First, recalculate network timing to ensure we have fresh data
  CalculateAverageOffset();

  std::vector<std::string> peer_list = GetConnectedPeers();
  if (peer_list.empty()) {
    LOG_DEBUG("No peers to broadcast command to");
    return;
  }

  LOG_INFO("Broadcasting command '{}' with position {} to {} peers", action,
           position, peer_list.size());

  // Add a safety margin to account for timing jitter (1ms)
  float safety_margin_ns = 1000000.0f;
  TimePointNs target_time_ns =
      sync_clock_.CalculateTargetExecutionTime(safety_margin_ns);
  // Compute relative wait time in milliseconds
  TimePointNs now_ns = SyncClock::GetCurrentTimeNs();
  int64_t wait_ms =
      target_time_ns > now_ns ? (target_time_ns - now_ns) / 1000000 : 0;

  // Dispatch non-blocking RPCs to all peers
  for (const auto& peer_address : peer_list) {
    std::thread([this, peer_address, action, position, wait_ms]() {
      if (!peer_service_->SendMusicCommand(peer_address, action, position,
                                           wait_ms)) {
        LOG_ERROR("Failed to send command to peer {}", peer_address);
      } else {
        LOG_INFO("Sent music command to {}", peer_address);
      }
    }).detach();
  }

  LOG_INFO("Dispatched command '{}' to {} peers, wait_ms={}", action,
           peer_list.size(), wait_ms);

  // Sleep locally for the same delay
  std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
}

bool PeerNetwork::BroadcastExit() {
  std::vector<std::string> peers = GetConnectedPeers();
  if (peers.empty()) {
    LOG_DEBUG("No peers to notify on exit");
    return true;
  }

  int success = 0;
  for (const auto& peer : peers) {
    if (!peer_service_->Exit(peer)) {
      LOG_ERROR("Failed to notify peer {} on exit", peer);
    } else {
      success++;
    }
  }
  return success == static_cast<int>(peers.size());
}
