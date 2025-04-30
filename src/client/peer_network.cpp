#include "include/peer_network.h"

#include <chrono>

#include "include/client.h"
#include "logger.h"
#include <ifaddrs.h>

// PeerService implementation
PeerService::PeerService(AudioClient* client) : client_(client) {
  LOG_DEBUG("PeerService initialized");
}

grpc::Status PeerService::Ping(grpc::ServerContext* context,
                               const client::PingRequest* request,
                               client::PingResponse* response) {
  LOG_DEBUG("Received ping request from peer: {}", context->peer());

  // Record receive time (t1)
  double t1 = std::chrono::duration_cast<std::chrono::duration<double>>(
    std::chrono::steady_clock::now().time_since_epoch())
    .count();

  // Fill response with t1 and t2
  response->set_t1(t1);

  // Simulate processing (very minimal delay)
  double t2 = std::chrono::duration_cast<std::chrono::duration<double>>(
    std::chrono::steady_clock::now().time_since_epoch())
    .count();
  response->set_t2(t2);

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

bool PeerNetwork::SendPingToPeer(const std::string& peer_address) {
  std::lock_guard<std::mutex> lock(peers_mutex_);

  auto it = peer_stubs_.find(peer_address);
  if (it == peer_stubs_.end()) {
    LOG_WARN("Cannot ping: not connected to {}", peer_address);
    return false;
  }

  auto& stub = it->second;

  client::PingRequest request;
  client::PingResponse response;
  grpc::ClientContext context;

  // t0 = local send time
  double t0 = std::chrono::duration_cast<std::chrono::duration<double>>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count();
  request.set_t0(t0);

  // Send ping
  auto status = stub->Ping(&context, request, &response);

  if (!status.ok()) {
    LOG_ERROR("Ping to {} failed: {}", peer_address, status.error_message());
    return false;
  }

  // t3 = local receive time
  double t3 = std::chrono::duration_cast<std::chrono::duration<double>>(
                  std::chrono::steady_clock::now().time_since_epoch())
                  .count();

  // Extract t1 and t2 from peer
  double t1 = response.t1();
  double t2 = response.t2();

  // Calculate RTT and offset
  double rtt = (t3 - t0) - (t2 - t1);
  double offset = ((t1 - t0) + (t2 - t3)) / 2;

  LOG_INFO("Ping to {}: RTT = {:.3f} ms, offset = {:.3f} ms",
           peer_address, rtt * 1000, offset * 1000);

  // Store for later use
  peer_offsets_[peer_address] = offset;
  rtt = static_cast<int>(rtt * 1000);  // store as ms if needed

  return true;
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

  // // Join request: send your address to peer and receive list of current peers
  // client::JoinRequest join_request;
  // client::JoinResponse join_response;
  // // Get my local address
  // std::string my_address = GetLocalIPAddress() + ":" + std::to_string(server_port_);
  // LOG_DEBUG("My local address is: {}:{}", my_address);
  // join_request.set_address(my_address);

  // Test connection with ping
  client::PingRequest ping_request;
  client::PingResponse ping_response;
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));

  LOG_DEBUG("Sending ping to peer {}", peer_address);
  auto status = stub->Ping(&context, ping_request, &ping_response);

  // Get offsets
  SendPingToPeer(peer_address);

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