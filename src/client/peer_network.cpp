#include "include/peer_network.h"

#include <chrono>

#include "include/client.h"
#include "logger.h"
#include <ifaddrs.h>

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
    return grpc::Status(grpc::StatusCode::INTERNAL, "Peer network not available");
  }
  // use raw pointer or keep using shared_ptr directly
  PeerNetwork* network = network_ptr.get();
  if (!network) {
  LOG_ERROR("Peer network not available");
  return grpc::Status(grpc::StatusCode::INTERNAL, "Peer network not available");
  }

  // Clear and reconnect
  network->DisconnectFromAllPeers();
  for (const auto& addr : request->peer_list()) {
    // Skip itself
    if (addr == GetLocalIPAddress() + ":" + std::to_string(network->GetServerPort())) {
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
  float wall_clock = request->wall_clock();
  float wait_time = request->wait_time();

  LOG_INFO(
      "Received music command from peer {}: action={}, position={}, "
      "wall_clock={}",
      context->peer(), action, position, wall_clock);

  // Mark that this command came from a broadcast to prevent echo
  client_->SetCommandFromBroadcast(true);

  // Wait appropriate amount of time
  if (wait_time > 0) {
    LOG_DEBUG("Waiting for {} seconds before executing command", wait_time);
    std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int>(wait_time * 10)));
  }

  // Execute the requested action
  if (action == "play") {
    client_->Play();
  } else if (action == "pause") {
    client_->Pause();
  } else if (action == "resume") {
    client_->Resume();
  } else if (action == "stop") {
    client_->Stop();
  } else {
    LOG_WARN("Unknown command from peer: {}", action);
  }

  // Reset the broadcast flag
  client_->SetCommandFromBroadcast(false);

  LOG_DEBUG("Music command executed successfully: {}", action);
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

  float total_offset = 0;
  float rtt = 0;
  for (const auto& entry : peer_stubs_) {
    client::PingRequest request;
    client::PingResponse response;
    grpc::ClientContext context;

    // Set t0 current time
    int t0 = static_cast<int>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    auto status = entry.second->Ping(&context, request, &response);

    if (!status.ok()) {
      LOG_ERROR("Failed to get offset from peer {}: {}", entry.first,
                status.error_message());
      continue;
    }

    // Get t1, t2 from response
    int t1 = static_cast<int>(response.t1());
    int t2 = static_cast<int>(response.t2());

    // Set t3 current time
    int t3 = static_cast<int>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    
    // Calculate rtt and offset
    float rtt = std::max(rtt, float((t3 - t0) - (t2 - t1)));
    float offset = (t1 - t0 + t2 - t3) / 2;

    total_offset += offset;
  }

  // assign peer network avg_offset to this
  avg_offset_ = float(total_offset / peer_stubs_.size());
  return avg_offset_;
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
        LOG_ERROR("Failed to send gossip to {}: {}", peer, status.error_message());
      } else {
        LOG_INFO("Sent gossip to {}", peer);
      }
    }
  }
  // Calculate average offset for future use
  CalculateAverageOffset();
}

void PeerNetwork::BroadcastCommand(const std::string& action, int position) {
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

  // Send to all connected peers
  int success_count = 0;
  for (const auto& peer_address : peer_list) {

    // Create the command request
    client::MusicRequest request;
    request.set_action(action);
    request.set_position(position);

    // Use current timestamp
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds =
        std::chrono::duration_cast<std::chrono::duration<float>>(duration)
            .count();
    request.set_wall_clock(seconds);
    request.set_wait_time((peer_list.size() - success_count)*10);

    // Create the response object
    client::MusicResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(1));

    LOG_DEBUG("Sending command '{}' to peer {}", action, peer_address);

    // Get the stub with mutex protection
    std::unique_ptr<client::ClientHandler::Stub> stub;
    {
      std::lock_guard<std::mutex> lock(peers_mutex_);
      auto it = peer_stubs_.find(peer_address);
      if (it == peer_stubs_.end()) {
        LOG_WARN("Peer {} disconnected before broadcast", peer_address);
        continue;
      }
      // Use the stub
      auto status = it->second->SendMusicCommand(&context, request, &response);
      if (!status.ok()) {
        LOG_ERROR("Failed to send command to peer {}: {}", peer_address,
                  status.error_message());
      } else {
        success_count++;
        std::this_thread::sleep_for(
            std::chrono::nanoseconds(10));
      }
    }
  }

  LOG_INFO("Broadcast complete: successfully sent to {}/{} peers",
           success_count, peer_list.size());
}