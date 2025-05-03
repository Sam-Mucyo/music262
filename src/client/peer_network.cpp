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
    }
    else if (action == "stop")
      client->Stop();
    else
      LOG_WARN("Unknown command from peer: {}", action);
    client->SetCommandFromBroadcast(false);
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

  unsigned int position = client_->GetPosition();
  response->set_position(position);

  LOG_DEBUG("Position request from peer {}, current position: {}",
            context->peer(), position);

  return grpc::Status::OK;
}

// PeerNetwork implementation
PeerNetwork::PeerNetwork(AudioClient* client)
    : client_(client), server_running_(false), server_port_(50052) {
  LOG_DEBUG("PeerNetwork initialized");
}

PeerNetwork::~PeerNetwork() {
  LOG_DEBUG("PeerNetwork shutting down");
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
  std::lock_guard<std::mutex> lock(peers_mutex_);

  // Check if already connected
  if (peer_stubs_.find(peer_address) != peer_stubs_.end()) {
    LOG_INFO("Already connected to peer: {}", peer_address);
    return true;
  }

  LOG_INFO("Attempting to connect to peer: {}", peer_address);

  // Create channel
  auto channel =
      grpc::CreateChannel(peer_address, grpc::InsecureChannelCredentials());
  auto stub = client::ClientHandler::NewStub(channel);

  // Test connection with ping
  client::PingRequest ping_request;
  client::PingResponse ping_response;
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));

  LOG_DEBUG("Sending ping to peer {}", peer_address);
  auto status = stub->Ping(&context, ping_request, &ping_response);

  if (!status.ok()) {
    LOG_ERROR("Failed to connect to peer {}: {}", peer_address,
              status.error_message());
    return false;
  }

  // Store the connection
  peer_stubs_[peer_address] = std::move(stub);
  LOG_INFO("Successfully connected to peer: {}", peer_address);

  return true;
}

bool PeerNetwork::DisconnectFromPeer(const std::string& peer_address) {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  auto it = peer_stubs_.find(peer_address);
  if (it == peer_stubs_.end()) {
    LOG_WARN("Cannot disconnect: not connected to peer {}", peer_address);
    return false;
  }

  peer_stubs_.erase(it);
  LOG_INFO("Disconnected from peer: {}", peer_address);

  return true;
}

void PeerNetwork::DisconnectFromAllPeers() {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  size_t count = peer_stubs_.size();
  if (count > 0) {
    LOG_INFO("Disconnecting from {} peers", count);
    peer_stubs_.clear();
  } else {
    LOG_DEBUG("No peers to disconnect from");
  }
}

std::vector<std::string> PeerNetwork::GetConnectedPeers() const {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  std::vector<std::string> peers;
  for (const auto& entry : peer_stubs_) {
    peers.push_back(entry.first);
  }

  LOG_DEBUG("Retrieved list of {} connected peers", peers.size());
  return peers;
}

float PeerNetwork::CalculateAverageOffset() const {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  if (peer_stubs_.empty()) {
    LOG_DEBUG("No peers connected, cannot calculate average offset");
    return 0.0f;
  }

  std::vector<float> offsets;
  float max_rtt = 0.0f;
  int successful_pings = 0;

  // Perform multiple pings to get more accurate measurements
  const int NUM_PINGS = 5;

  for (const auto& entry : peer_stubs_) {
    std::vector<float> peer_offsets;
    float peer_rtt = 0.0f;

    for (int i = 0; i < NUM_PINGS; i++) {
      client::PingRequest request;
      client::PingResponse response;
      grpc::ClientContext context;

      // Set deadline for ping request
      context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::seconds(1));

      // Set t0 (time at client before sending request)
      TimePointNs t0 = SyncClock::GetCurrentTimeNs();

      // Send ping request
      auto status = entry.second->Ping(&context, request, &response);

      if (!status.ok()) {
        LOG_ERROR("Failed to get offset from peer {}: {}", entry.first,
                  status.error_message());
        break;
      }

      // Set t3 (time at client after receiving response)
      TimePointNs t3 = SyncClock::GetCurrentTimeNs();

      // Process the ping response using SyncClock
      auto [current_offset, current_rtt] =
          const_cast<SyncClock&>(sync_clock_)
              .ProcessPingResponse(t0, t3, response);

      LOG_DEBUG("Ping to {}: RTT={} ns, Offset={} ns", entry.first, current_rtt,
                current_offset);

      peer_offsets.push_back(current_offset);
      peer_rtt = std::max(peer_rtt, current_rtt);
    }

    // Average the results from multiple pings to this peer
    if (!peer_offsets.empty()) {
      float peer_avg_offset =
          std::accumulate(peer_offsets.begin(), peer_offsets.end(), 0.0f) /
          peer_offsets.size();
      offsets.push_back(peer_avg_offset);
      max_rtt = std::max(max_rtt, peer_rtt);
      successful_pings++;

      LOG_INFO("Average offset from peer {}: {} ns, RTT: {} ns", entry.first,
               peer_avg_offset, peer_rtt);
    }
  }

  if (successful_pings == 0) {
    LOG_WARN("No successful pings to calculate offset");
    return 0.0f;
  }

  // Set the max RTT in the sync clock
  const_cast<SyncClock&>(sync_clock_).SetMaxRtt(max_rtt);

  // Calculate and store the average offset across all peers
  float new_avg_offset =
      const_cast<SyncClock&>(sync_clock_).CalculateAverageOffset(offsets);

  LOG_INFO("Calculated network average offset: {} ns, max RTT: {} ns",
           new_avg_offset, max_rtt);

  return new_avg_offset;
}

void PeerNetwork::BroadcastGossip() {
  std::vector<std::string> peer_list;
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& entry : peer_stubs_) {
      peer_list.push_back(entry.first);
    }
  }

  // Add clients to list of addresses
  client::GossipRequest request;
  for (const auto& peer : peer_list) {
    request.add_peer_list(peer);
  }
  request.add_peer_list(GetLocalIPAddress() + ":" +
                        std::to_string(server_port_));

  for (const auto& peer : peer_list) {
    grpc::ClientContext context;
    client::GossipResponse response;

    {
      std::lock_guard<std::mutex> lock(peers_mutex_);
      auto it = peer_stubs_.find(peer);
      if (it == peer_stubs_.end()) continue;
      auto status = it->second->Gossip(&context, request, &response);
      if (!status.ok()) {
        LOG_ERROR("Failed to send gossip to {}: {}", peer,
                  status.error_message());
      } else {
        LOG_INFO("Sent gossip to {}", peer);
      }
    }
  }
  // Calculate average offset for future use
  CalculateAverageOffset();
}

bool PeerNetwork::BroadcastLoad(int song_num) {
  std::vector<std::string> peers;
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto& entry : peer_stubs_) peers.push_back(entry.first);
  }
  if (peers.empty()) {
    LOG_DEBUG("No peers to broadcast load to");
    return true;
  }
  int success = 0;
  for (auto& peer : peers) {
    client::MusicRequest req;
    req.set_action("load");
    req.set_song_num(song_num);
    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() +
                     std::chrono::seconds(5));
    client::MusicResponse resp;
    std::lock_guard<std::mutex> lock(peers_mutex_);
    auto it = peer_stubs_.find(peer);
    if (it == peer_stubs_.end()) {
      LOG_WARN("Peer {} disconnected before load", peer);
      continue;
    }
    auto status = it->second->SendMusicCommand(&ctx, req, &resp);
    if (!status.ok()) {
      LOG_ERROR("Failed load to {}: {}", peer, status.error_message());
    } else {
      success++;
    }
  }
  LOG_INFO("Load broadcast complete: {}/{} peers", success, peers.size());
  return success == static_cast<int>(peers.size());
}

void PeerNetwork::BroadcastCommand(const std::string& action, int position) {
  // First, recalculate network timing to ensure we have fresh data
  CalculateAverageOffset();

  std::vector<std::string> peer_list;
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);

    if (peer_stubs_.empty()) {
      LOG_DEBUG("No peers to broadcast command to");
      return;
    }

    // Make a copy of peer addresses to avoid holding the mutex during RPC calls
    for (const auto& entry : peer_stubs_) {
      peer_list.push_back(entry.first);
    }
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
    client::MusicRequest request;
    request.set_action(action);
    request.set_position(position);
    request.set_wait_time_ms(wait_ms);

    std::thread([this, peer_address, request]() {
      client::MusicResponse response;
      grpc::ClientContext context;
      context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::seconds(1));
      std::lock_guard<std::mutex> lock(peers_mutex_);
      auto it = peer_stubs_.find(peer_address);
      if (it == peer_stubs_.end()) {
        LOG_WARN("Peer {} disconnected before broadcast", peer_address);
        return;
      }
      auto status = it->second->SendMusicCommand(&context, request, &response);
      if (!status.ok()) {
        LOG_ERROR("Failed to send command to peer {}: {}", peer_address,
                  status.error_message());
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
