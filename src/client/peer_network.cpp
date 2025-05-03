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
  auto t1 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

  // Set t2 to current time when about to respond
  auto t2 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

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
  float wall_clock_ns = request->wall_clock();

  LOG_INFO(
      "Received music command from peer {}: action={}, position={}, "
      "target_time={}",
      context->peer(), action, position, static_cast<int64_t>(wall_clock_ns));

  // Mark that this command came from a broadcast to prevent echo
  client_->SetCommandFromBroadcast(true);

  // Get a reference to the peer network for clock adjustment
  std::shared_ptr<PeerNetwork> network_ptr = client_->GetPeerNetwork();
  if (!network_ptr) {
    LOG_ERROR("Peer network not available");
    client_->SetCommandFromBroadcast(false);
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Peer network not available");
  }

  // Get clock offset from network
  float clock_offset = network_ptr->GetAverageOffset();

  // Calculate the target execution time
  // If time is in the future, wait until it's time to execute
  // If time is in the past, execute immediately
  int64_t target_time_ns = static_cast<int64_t>(wall_clock_ns);
  int64_t current_time_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count();

  // Adjust the target time based on our clock offset
  target_time_ns -= static_cast<int64_t>(clock_offset);

  if (target_time_ns > current_time_ns) {
    // We need to wait until the target time
    int64_t wait_ns = target_time_ns - current_time_ns;

    LOG_DEBUG("Waiting {} ns before executing command (target time: {})",
              wait_ns, target_time_ns);

    // Wait until the exact target time
    auto target_timepoint = std::chrono::high_resolution_clock::now() +
                            std::chrono::nanoseconds(wait_ns);
    std::this_thread::sleep_until(target_timepoint);
  } else {
    LOG_DEBUG("Target time already passed, executing immediately");
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

  float total_offset = 0.0f;
  float max_rtt = 0.0f;
  int successful_pings = 0;

  // Perform multiple pings to get more accurate measurements
  const int NUM_PINGS = 5;

  for (const auto& entry : peer_stubs_) {
    float peer_offset = 0.0f;
    float peer_rtt = 0.0f;

    for (int i = 0; i < NUM_PINGS; i++) {
      client::PingRequest request;
      client::PingResponse response;
      grpc::ClientContext context;

      // Set deadline for ping request
      context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::seconds(1));

      // Set t0 (time at client before sending request)
      int64_t t0 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();

      // Send ping request
      auto status = entry.second->Ping(&context, request, &response);

      if (!status.ok()) {
        LOG_ERROR("Failed to get offset from peer {}: {}", entry.first,
                  status.error_message());
        break;
      }

      // Set t3 (time at client after receiving response)
      int64_t t3 = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();

      // Get t1, t2 from response (times at the server)
      int64_t t1 = response.t1();
      int64_t t2 = response.t2();

      // Calculate round-trip time: (t3-t0) - (t2-t1)
      // Total time - server processing time
      float current_rtt = static_cast<float>((t3 - t0) - (t2 - t1));

      // Calculate clock offset: ((t1-t0) + (t2-t3))/2
      // Estimate how much the client clock differs from the server
      float current_offset = static_cast<float>((t1 - t0) + (t2 - t3)) / 2.0f;

      LOG_DEBUG("Ping to {}: RTT={} ns, Offset={} ns", entry.first, current_rtt,
                current_offset);

      peer_offset += current_offset;
      peer_rtt = std::max(peer_rtt, current_rtt);
    }

    // Average the results from multiple pings to this peer
    if (NUM_PINGS > 0) {
      peer_offset /= NUM_PINGS;
      total_offset += peer_offset;
      max_rtt = std::max(max_rtt, peer_rtt);
      successful_pings++;

      LOG_INFO("Average offset from peer {}: {} ns, RTT: {} ns", entry.first,
               peer_offset, peer_rtt);
    }
  }

  if (successful_pings == 0) {
    LOG_WARN("No successful pings to calculate offset");
    return 0.0f;
  }

  // Calculate and store the average offset across all peers
  float new_avg_offset = total_offset / successful_pings;

  // Store max RTT for use in sync calculations
  const_cast<PeerNetwork*>(this)->rtt_ = max_rtt;

  // Store average offset
  const_cast<PeerNetwork*>(this)->avg_offset_ = new_avg_offset;

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

  // Calculate execution time for all peers
  // Use RTT to determine how much time to add for network delay
  // Add a safety margin to account for processing time
  float safety_margin_ns = 1000000.0f;  // 1ms safety margin

  // Calculate the target execution time in the future
  // We want all peers to execute this command at approximately the same time
  auto now = std::chrono::high_resolution_clock::now();
  auto target_time = now + std::chrono::nanoseconds(
                               static_cast<int64_t>(rtt_ + safety_margin_ns));

  // Convert to nanoseconds since epoch for sending in messages
  auto target_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            target_time.time_since_epoch())
                            .count();

  // Send to all connected peers
  int success_count = 0;
  for (const auto& peer_address : peer_list) {
    // Create the command request
    client::MusicRequest request;
    request.set_action(action);
    request.set_position(position);

    // Use the target execution time as the wall_clock
    request.set_wall_clock(static_cast<float>(target_time_ns));

    // Each peer will need to adjust based on our estimate of their clock offset
    // This wait_time is the amount of time the peer should wait before
    // executing
    request.set_wait_time(0);  // We no longer need this with our new approach

    // Create the response object
    client::MusicResponse response;
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(1));

    LOG_DEBUG("Sending command '{}' to peer {} with target time {}", action,
              peer_address, target_time_ns);

    // Get the stub with mutex protection
    {
      std::lock_guard<std::mutex> lock(peers_mutex_);
      auto it = peer_stubs_.find(peer_address);
      if (it == peer_stubs_.end()) {
        LOG_WARN("Peer {} disconnected before broadcast", peer_address);
        continue;
      }

      // Use the stub to send the command
      auto status = it->second->SendMusicCommand(&context, request, &response);
      if (!status.ok()) {
        LOG_ERROR("Failed to send command to peer {}: {}", peer_address,
                  status.error_message());
      } else {
        success_count++;
      }
    }
  }

  LOG_INFO("Broadcast complete: successfully sent to {}/{} peers",
           success_count, peer_list.size());

  // Now we need to wait until the target time before executing locally
  auto current_time = std::chrono::high_resolution_clock::now();
  if (current_time < target_time) {
    auto wait_duration = target_time - current_time;
    LOG_DEBUG(
        "Waiting {} ns before executing local command",
        std::chrono::duration_cast<std::chrono::nanoseconds>(wait_duration)
            .count());
    std::this_thread::sleep_until(target_time);
  }

  // The command execution is handled by the caller (AudioClient's
  // Play/Pause/Resume/Stop method) We don't need to call the command execution
  // here
}