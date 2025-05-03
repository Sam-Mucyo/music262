#include "include/peer_network.h"

#include <chrono>
#include <cstdint>

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

// Helper: get current time in nanoseconds
static inline int64_t NowNs() {
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

// PeerService implementation
PeerService::PeerService(AudioClient* client) : client_(client) {
  LOG_DEBUG("PeerService initialized");
}

grpc::Status PeerService::Ping(grpc::ServerContext* context,
                               const client::PingRequest* request,
                               client::PingResponse* response) {

  // Set t1 current time
  const int64_t t1 = NowNs();
  response->set_t1(static_cast<double>(t1)); 

  // Set t2 current time
  const int64_t t2 = NowNs();
  response->set_t2(static_cast<double>(t2));
                                
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
  network->CalculateAverageOffset();
  return grpc::Status::OK;
}

grpc::Status PeerService::SendMusicCommand(grpc::ServerContext* context,
                                           const client::MusicRequest* request,
                                           client::MusicResponse* response) {
  if (!client_) {
    LOG_ERROR("Client not initialized in PeerService");
    return grpc::Status(grpc::StatusCode::INTERNAL, "Client not initialized");
  }

  // DEBUG: get current time
  const int64_t t0 = NowNs();
  LOG_INFO("Current time: {}", t0);
  // now print in literal 00:00:00:00 time - convert t0 to standard clock display

  const std::string& action = request->action();
  float target_time = request->target_time();

  LOG_INFO(
      "Received music command from peer {}: action={} @ target_time={}",
      context->peer(), action, target_time);

  // Mark that this command came from a broadcast to prevent echo
  client_->SetCommandFromBroadcast(true);

  // Adjust target time to local time using average offset
  // LOG_INFO("Global target time: {}", target_time);
  int64_t offset_time = client_->GetPeerNetwork()->GetAverageOffset();
  // LOG_INFO("Average offset: {}", offset_time);
  const int64_t new_target_time = target_time - offset_time;
  // LOG_INFO("Local target time (adjusted minus): {}", new_target_time);
  const int64_t sleep_time = new_target_time - NowNs();
  // LOG_INFO("Sleeping for {} ns", sleep_time);

  if (sleep_time > 0) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time));
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

int64_t PeerNetwork::CalculateAverageOffset() {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  if (peer_stubs_.empty()) {
    LOG_DEBUG("No peers connected, cannot calculate average offset");
    return 0.0f;
  }

  double total_offset = 0.0;
  for (const auto& entry : peer_stubs_) {
    client::PingRequest request;
    client::PingResponse response;
    grpc::ClientContext context;

    // Set t0 current time
    const int64_t t0 = NowNs();
    auto status = entry.second->Ping(&context, request, &response);

    if (!status.ok()) {
      LOG_ERROR("Failed to get offset from peer {}: {}", entry.first,
                status.error_message());
      continue;
    }

    // Get t1, t2 from response
    const int64_t t1 = static_cast<int64_t>(response.t1());
    const int64_t t2 = static_cast<int64_t>(response.t2());

    // Set t3 current time
    const int64_t t3 = NowNs();

    // Calculate rtt and offset
    // const double offset = (double(t1 - t0) + double(t2 - t3)) / 2.0;
    // float rtt = std::max(rtt, float((t3 - t0) - (t2 - t1)));

    // New way to calculate offset
    const double offset = double(t2);

    total_offset += offset;
  } 

  // // assign peer network avg_offset to this
  // // avg_offset_ = static_cast<int64_t>(total_offset / peer_stubs_.size());
  // avg_offset_.store(
  // static_cast<int64_t>(total_offset / peer_stubs_.size()),
  //   std::memory_order_relaxed);
  // return avg_offset_.load(std::memory_order_relaxed);    
  // // return avg_offset_;

  // New way to calculate offset
  avg_offset_.store(
    static_cast<int64_t>((total_offset + NowNs()) / (peer_stubs_.size() + 1)) - NowNs(),
    std::memory_order_relaxed);
  return avg_offset_.load(std::memory_order_relaxed);
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
  LOG_INFO("Gossip broadcast complete, offset is {}", avg_offset_.load());
}

void PeerNetwork::BroadcastCommand(const std::string& action) {
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

  LOG_INFO("Broadcasting command '{}' to {} peers", action, peer_list.size());

  // Create the command request
  client::MusicRequest request;
  request.set_action(action);
  float target_time = NowNs() + GetAverageOffset() + 3e9;
  request.set_target_time(static_cast<double>(target_time));

  // Send to all connected peers
  int success_count = 0;

  // DEBUG: get current time
  const int64_t t0 = NowNs();
  LOG_INFO("Current time: {}", t0);
  // now print in literal 00:00:00:00 time
  LOG_INFO("Current time: {}",
            std::chrono::duration_cast<std::chrono::hours>(
                std::chrono::nanoseconds(t0))
                .count());

  for (const auto& peer_address : peer_list) {

    // Create the response object
    client::MusicResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(1000));

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
      }
    }
  }

  // Adjust target time to local time using average offset
  // LOG_INFO("Global target time: {}", target_time);
  // LOG_INFO("Average offset: {}", avg_offset_.load());
  const int64_t new_target_time = target_time - avg_offset_.load();
  // LOG_INFO("Local target time (adjusted minus): {}", new_target_time);

  // sleep for
  const int64_t sleep_time = new_target_time - NowNs();
  // LOG_INFO("Sleeping for {} ns", sleep_time);
  if (sleep_time > 0) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time));
  }
  
  LOG_INFO("Broadcast complete: successfully sent to {}/{} peers",
           success_count, peer_list.size());
}