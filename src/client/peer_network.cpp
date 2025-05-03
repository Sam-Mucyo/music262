#include "include/peer_network.h"

#include <chrono>
#include <future>
#include <sstream>
#include <thread>

#include "../common/include/logger.h"
#include "include/client.h"

// ============================================================================
// PeerStreamState implementation
// ============================================================================

PeerStreamState::PeerStreamState(const std::string& peer_address)
    : peer_address_(peer_address), stream_active_(false) {}

PeerStreamState::~PeerStreamState() { CloseStream(); }

bool PeerStreamState::InitializeStream(
    std::shared_ptr<client::ClientHandler::Stub> stub) {
  if (stream_active_) {
    CloseStream();
  }

  stub_ = stub;
  context_ = std::make_unique<grpc::ClientContext>();

  // Set indefinite timeout for the bidirectional stream
  context_->set_deadline(std::chrono::system_clock::now() +
                         std::chrono::hours(24));

  // Create the bidirectional stream
  stream_ = stub_->StreamCommands(context_.get());
  if (!stream_) {
    LOG_ERROR("Failed to create command stream to peer {}", peer_address_);
    return false;
  }

  stream_active_ = true;

  // Start response processing thread
  response_thread_ = std::thread(&PeerStreamState::ProcessResponses, this);

  LOG_INFO("Established command stream with peer {}", peer_address_);
  return true;
}

void PeerStreamState::CloseStream() {
  if (stream_active_) {
    stream_active_ = false;

    // Wait for response thread to finish
    if (response_thread_.joinable()) {
      response_thread_.join();
    }

    stream_.reset();
    context_.reset();
    LOG_INFO("Closed command stream with peer {}", peer_address_);
  }
}

bool PeerStreamState::SendCommand(const client::MusicCommand& command) {
  if (!stream_active_ || !stream_) {
    LOG_ERROR("Cannot send command to peer {}: stream not active",
              peer_address_);
    return false;
  }

  bool success = stream_->Write(command);
  if (!success) {
    LOG_ERROR("Failed to send command to peer {}, closing stream",
              peer_address_);
    CloseStream();
    return false;
  }

  LOG_DEBUG("Sent command {} to peer {}", command.command_id(), peer_address_);
  return true;
}

void PeerStreamState::ProcessResponses() {
  client::CommandStatus status;

  while (stream_active_ && stream_->Read(&status)) {
    LOG_DEBUG("Received command status from peer {}: command_id={}, success={}",
              peer_address_, status.command_id(), status.success());

    if (!status.success()) {
      LOG_WARN("Command {} failed at peer {}: {}", status.command_id(),
               peer_address_, status.error_message());
    }
  }

  // Stream closed or error occurred
  stream_active_ = false;
  LOG_INFO("Command stream from peer {} closed", peer_address_);
}

// ============================================================================
// NetworkMonitor implementation
// ============================================================================

NetworkMonitor::NetworkMonitor(SyncClock& sync_clock)
    : sync_clock_(sync_clock), running_(false) {}

NetworkMonitor::~NetworkMonitor() { Stop(); }

void NetworkMonitor::Start() {
  if (running_) return;

  running_ = true;
  monitor_thread_ = std::thread(&NetworkMonitor::MonitorThread, this);
  LOG_INFO("Network monitoring thread started");
}

void NetworkMonitor::Stop() {
  if (!running_) return;

  running_ = false;
  if (monitor_thread_.joinable()) {
    monitor_thread_.join();
  }
  LOG_INFO("Network monitoring thread stopped");
}

void NetworkMonitor::AddPeer(
    const std::string& peer_address,
    std::shared_ptr<client::ClientHandler::Stub> stub) {
  std::lock_guard<std::mutex> lock(peers_mutex_);
  monitored_peers_[peer_address] = stub;
  LOG_INFO("Added peer {} to network monitor", peer_address);
}

void NetworkMonitor::RemovePeer(const std::string& peer_address) {
  std::lock_guard<std::mutex> lock(peers_mutex_);
  monitored_peers_.erase(peer_address);
  LOG_INFO("Removed peer {} from network monitor", peer_address);
}

void NetworkMonitor::MonitorThread() {
  while (running_) {
    // Copy peers to avoid holding the lock during RPC calls
    std::map<std::string, std::shared_ptr<client::ClientHandler::Stub>>
        peers_copy;
    {
      std::lock_guard<std::mutex> lock(peers_mutex_);
      peers_copy = monitored_peers_;
    }

    // Perform offset calculation for each peer
    std::vector<float> offsets;
    float max_rtt = 0.0f;

    for (const auto& [peer_address, stub] : peers_copy) {
      CalculatePeerOffset(peer_address, stub);
    }

    // Sleep until next recalculation
    std::this_thread::sleep_for(
        std::chrono::milliseconds(recalculation_interval_ms_));
  }
}

void NetworkMonitor::CalculatePeerOffset(
    const std::string& peer_address,
    std::shared_ptr<client::ClientHandler::Stub> stub) {
  // Collect multiple ping measurements for accuracy
  constexpr int ping_count = 5;
  std::vector<float> offsets;
  float max_rtt = 0.0f;

  for (int i = 0; i < ping_count; ++i) {
    // Create request and context
    client::PingRequest request;
    client::PingResponse response;
    grpc::ClientContext context;

    // Set a reasonable deadline for ping
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(1));

    // Get start time (t0)
    TimePointNs t0 = SyncClock::GetCurrentTimeNs();
    request.set_sender_time(t0);

    // Send ping
    auto status = stub->Ping(&context, request, &response);

    // Get end time (t3)
    TimePointNs t3 = SyncClock::GetCurrentTimeNs();

    if (!status.ok()) {
      LOG_WARN("Ping to peer {} failed: {}", peer_address,
               status.error_message());
      continue;
    }

    // Process ping response
    auto [offset, rtt] = sync_clock_.ProcessPingResponse(t0, t3, response);
    offsets.push_back(offset);
    max_rtt = std::max(max_rtt, rtt);

    // Brief pause between pings
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (!offsets.empty()) {
    // Calculate average offset
    float avg_offset = sync_clock_.CalculateAverageOffset(offsets);

    // Update SyncClock with latest measurements
    sync_clock_.SetAverageOffset(avg_offset);
    sync_clock_.SetMaxRtt(max_rtt);

    LOG_DEBUG("Updated clock offset for peer {}: offset={} ns, max_rtt={} ns",
              peer_address, avg_offset, max_rtt);
  }
}

// ============================================================================
// PeerService implementation
// ============================================================================

PeerService::PeerService(AudioClient* client) : client_(client) {}

grpc::Status PeerService::Ping(grpc::ServerContext* context,
                               const client::PingRequest* request,
                               client::PingResponse* response) {
  // Get time when received (t1)
  TimePointNs t1 = SyncClock::GetCurrentTimeNs();

  // Get time before sending (t2)
  TimePointNs t2 = SyncClock::GetCurrentTimeNs();

  // Fill response with timestamps
  response->set_sender_time(request->sender_time());
  response->set_receiver_time_recv(t1);
  response->set_receiver_time_send(t2);

  return grpc::Status::OK;
}

grpc::Status PeerService::Gossip(grpc::ServerContext* context,
                                 const client::GossipRequest* request,
                                 client::GossipResponse* response) {
  // Process peer list from request
  std::vector<std::string> peer_addresses;
  for (int i = 0; i < request->peer_list_size(); ++i) {
    peer_addresses.push_back(request->peer_list(i));
  }

  // Forward to client for processing
  client_->ProcessPeerList(peer_addresses);

  return grpc::Status::OK;
}

grpc::Status PeerService::SendMusicCommand(grpc::ServerContext* context,
                                           const client::MusicCommand* request,
                                           client::CommandStatus* response) {
  int action = static_cast<int>(request->action());
  LOG_INFO("Received command: {} with position {}", action,
           request->position());

  response->set_command_id(request->command_id());

  // Calculate target time from wall_clock
  TimePointNs target_time_ns = request->wall_clock();

  // Calculate local adjusted time based on our clock offset
  auto& sync_clock = client_->GetPeerNetwork().GetSyncClock();
  TimePointNs adjusted_time = SyncClock::AdjustTargetTime(
      target_time_ns, sync_clock.GetAverageOffset());

  // Calculate how long to wait
  TimePointNs now = SyncClock::GetCurrentTimeNs();

  // If target time is in the future, sleep until then
  if (adjusted_time > now) {
    SyncClock::SleepUntil(adjusted_time);
  }

  // Record execution time
  TimePointNs execution_time = SyncClock::GetCurrentTimeNs();
  response->set_execution_time(execution_time);

  bool success = false;

  // Execute the command on our local player
  try {
    switch (request->action()) {
      case client::MusicCommand::PLAY:
        client_->Play(request->position(), false);
        break;
      case client::MusicCommand::PAUSE:
        client_->Pause(false);
        break;
      case client::MusicCommand::RESUME:
        client_->Resume(false);
        break;
      case client::MusicCommand::STOP:
        client_->Stop(false);
        break;
      default:
        LOG_ERROR("Unknown command action: {}", action);
        response->set_success(false);
        response->set_error_message("Unknown command action");
        return grpc::Status::OK;
    }

    success = true;
  } catch (const std::exception& e) {
    LOG_ERROR("Exception during command execution: {}", e.what());
    response->set_success(false);
    response->set_error_message(e.what());
    return grpc::Status::OK;
  }

  response->set_success(success);

  // Log timing information
  int64_t delay = execution_time - target_time_ns;
  LOG_DEBUG("Command {} executed with {}ns delay from target",
            request->command_id(), delay);

  return grpc::Status::OK;
}

grpc::Status PeerService::GetPosition(grpc::ServerContext* context,
                                      const client::GetPositionRequest* request,
                                      client::GetPositionResponse* response) {
  // Get current position from the client
  int position = client_->GetPosition();
  response->set_position(position);
  return grpc::Status::OK;
}

grpc::Status PeerService::StreamCommands(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<client::CommandStatus, client::MusicCommand>*
        stream) {
  LOG_INFO("Command stream established from peer {}", context->peer());

  client::MusicCommand command;
  while (stream->Read(&command)) {
    int action = static_cast<int>(command.action());
    LOG_DEBUG("Received streaming command: {} with id {}", action,
              command.command_id());

    client::CommandStatus status;
    status.set_command_id(command.command_id());

    // Calculate target time from wall_clock
    TimePointNs target_time_ns = command.wall_clock();

    // Calculate local adjusted time based on our clock offset
    auto& sync_clock = client_->GetPeerNetwork().GetSyncClock();
    TimePointNs adjusted_time = SyncClock::AdjustTargetTime(
        target_time_ns, sync_clock.GetAverageOffset());

    // Calculate how long to wait
    TimePointNs now = SyncClock::GetCurrentTimeNs();

    // If target time is in the future, sleep until then
    if (adjusted_time > now) {
      SyncClock::SleepUntil(adjusted_time);
    }

    // Record execution time
    TimePointNs execution_time = SyncClock::GetCurrentTimeNs();
    status.set_execution_time(execution_time);

    bool success = false;

    // Execute the command on our local player
    try {
      switch (command.action()) {
        case client::MusicCommand::PLAY:
          client_->Play(command.position(), false);
          break;
        case client::MusicCommand::PAUSE:
          client_->Pause(false);
          break;
        case client::MusicCommand::RESUME:
          client_->Resume(false);
          break;
        case client::MusicCommand::STOP:
          client_->Stop(false);
          break;
        default:
          LOG_ERROR("Unknown command action: {}", action);
          status.set_success(false);
          status.set_error_message("Unknown command action");
          stream->Write(status);
          continue;
      }

      success = true;
    } catch (const std::exception& e) {
      LOG_ERROR("Exception during command execution: {}", e.what());
      status.set_success(false);
      status.set_error_message(e.what());
      stream->Write(status);
      continue;
    }

    status.set_success(success);

    // Log timing information
    int64_t delay = execution_time - target_time_ns;
    LOG_DEBUG("Command {} executed with {}ns delay from target",
              command.command_id(), delay);

    // Send status back to client
    stream->Write(status);
  }

  LOG_INFO("Command stream closed from peer {}", context->peer());
  return grpc::Status::OK;
}

grpc::Status PeerService::MonitorNetwork(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<client::NetworkStatus, client::NetworkStatus>*
        stream) {
  LOG_INFO("Network monitoring stream established from peer {}",
           context->peer());

  client::NetworkStatus status;
  while (stream->Read(&status)) {
    // Process network status from peer
    LOG_DEBUG("Received network status from peer {}: rtt={}, offset={}",
              status.peer_address(), status.rtt(), status.clock_offset());

    // Send back our status
    client::NetworkStatus our_status;
    our_status.set_rtt(client_->GetPeerNetwork().GetSyncClock().GetMaxRtt());
    our_status.set_clock_offset(
        client_->GetPeerNetwork().GetSyncClock().GetAverageOffset());

    // Convert port to string for peer_address
    std::string port_str =
        std::to_string(client_->GetPeerNetwork().GetServerPort());
    our_status.set_peer_address("localhost:" + port_str);

    if (!stream->Write(our_status)) {
      LOG_ERROR("Failed to write network status to peer {}", context->peer());
      break;
    }
  }

  LOG_INFO("Network monitoring stream closed from peer {}", context->peer());
  return grpc::Status::OK;
}

// ============================================================================
// PeerNetwork implementation
// ============================================================================

PeerNetwork::PeerNetwork(AudioClient* client)
    : client_(client),
      server_running_(false),
      server_port_(0),
      queue_running_(false),
      next_command_id_(0) {
  // Create a NetworkMonitor for background offset calculation
  network_monitor_ = std::make_unique<NetworkMonitor>(sync_clock_);
}

PeerNetwork::~PeerNetwork() {
  // Stop async processing
  queue_running_ = false;
  queue_cv_.notify_all();
  if (queue_thread_.joinable()) {
    queue_thread_.join();
  }

  // Stop network monitor
  network_monitor_->Stop();

  // Stop server
  StopServer();

  // Disconnect from all peers
  DisconnectFromAllPeers();
}

bool PeerNetwork::StartServer(int port) {
  if (server_running_) {
    LOG_WARN("Server already running on port {}", server_port_);
    return false;
  }

  // Create the server service
  service_ = std::make_unique<PeerService>(client_);

  // Build the server
  std::string server_address = "0.0.0.0:" + std::to_string(port);
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials(),
                           &server_port_);
  builder.RegisterService(service_.get());

  // Start the server
  server_ = builder.BuildAndStart();
  if (!server_) {
    LOG_ERROR("Failed to start server on port {}", port);
    return false;
  }

  server_running_ = true;
  LOG_INFO("Server started on port {}", server_port_);

  // Start the server thread
  server_thread_ = std::thread([this]() {
    LOG_INFO("Server thread started");
    server_->Wait();
    LOG_INFO("Server thread stopped");
  });

  // Start the command queue processing thread
  queue_running_ = true;
  queue_thread_ = std::thread(&PeerNetwork::ProcessCommandQueue, this);

  // Start the network monitor
  network_monitor_->Start();

  return true;
}

void PeerNetwork::StopServer() {
  if (!server_running_) {
    return;
  }

  // Stop the server
  server_->Shutdown();
  if (server_thread_.joinable()) {
    server_thread_.join();
  }

  server_running_ = false;
  LOG_INFO("Server stopped");
}

bool PeerNetwork::ConnectToPeer(const std::string& peer_address) {
  // Check if already connected
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    if (peer_stubs_.find(peer_address) != peer_stubs_.end()) {
      LOG_WARN("Already connected to peer {}", peer_address);
      return true;
    }
  }

  // Create a channel and stub
  auto channel =
      grpc::CreateChannel(peer_address, grpc::InsecureChannelCredentials());
  auto stub = client::ClientHandler::NewStub(channel);

  if (!stub) {
    LOG_ERROR("Failed to create stub for peer {}", peer_address);
    return false;
  }

  // Test the connection with a ping
  client::PingRequest request;
  client::PingResponse response;
  grpc::ClientContext context;
  context.set_deadline(std::chrono::system_clock::now() +
                       std::chrono::seconds(5));

  TimePointNs t0 = SyncClock::GetCurrentTimeNs();
  request.set_sender_time(t0);

  auto status = stub->Ping(&context, request, &response);

  if (!status.ok()) {
    LOG_ERROR("Failed to connect to peer {}: {}", peer_address,
              status.error_message());
    return false;
  }

  TimePointNs t3 = SyncClock::GetCurrentTimeNs();

  // Process ping response to calculate initial offset
  auto [offset, rtt] = sync_clock_.ProcessPingResponse(t0, t3, response);
  LOG_INFO("Connected to peer {}: offset={} ns, rtt={} ns", peer_address,
           offset, rtt);

  // Create a shared_ptr to the stub for thread safety
  std::shared_ptr<client::ClientHandler::Stub> shared_stub = std::move(stub);

  // Store the stub
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    peer_stubs_[peer_address] = shared_stub;
  }

  // Create a stream state for this peer
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto stream_state = std::make_unique<PeerStreamState>(peer_address);
    if (stream_state->InitializeStream(shared_stub)) {
      peer_streams_[peer_address] = std::move(stream_state);
    }
  }

  // Add to network monitor
  network_monitor_->AddPeer(peer_address, shared_stub);

  return true;
}

bool PeerNetwork::DisconnectFromPeer(const std::string& peer_address) {
  // Close the stream if it exists
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = peer_streams_.find(peer_address);
    if (it != peer_streams_.end()) {
      it->second->CloseStream();
      peer_streams_.erase(it);
    }
  }

  // Remove from network monitor
  network_monitor_->RemovePeer(peer_address);

  // Remove the stub
  {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peer_stubs_.erase(peer_address) > 0;
  }
}

void PeerNetwork::DisconnectFromAllPeers() {
  // Get the list of peers
  std::vector<std::string> peers = GetConnectedPeers();

  // Disconnect from each peer
  for (const auto& peer : peers) {
    DisconnectFromPeer(peer);
  }
}

std::vector<std::string> PeerNetwork::GetConnectedPeers() const {
  std::vector<std::string> result;

  std::lock_guard<std::mutex> lock(peers_mutex_);
  for (const auto& entry : peer_stubs_) {
    result.push_back(entry.first);
  }

  return result;
}

float PeerNetwork::CalculateAverageOffset() const {
  // This is now handled by the NetworkMonitor in the background
  // This method is kept for backward compatibility
  return sync_clock_.GetAverageOffset();
}

void PeerNetwork::BroadcastCommand(const std::string& action, int position) {
  // Get list of connected peers
  std::vector<std::string> peer_list = GetConnectedPeers();

  if (peer_list.empty()) {
    LOG_DEBUG("No peers to broadcast command to");
    return;
  }

  LOG_INFO("Broadcasting command '{}' with position {} to {} peers", action,
           position, peer_list.size());

  // Calculate target execution time
  // Add a safety margin to account for processing time (1ms)
  float safety_margin_ns = 1000000.0f;
  TimePointNs target_time_ns =
      sync_clock_.CalculateTargetExecutionTime(safety_margin_ns);

  // Create the command
  client::MusicCommand command;

  // Set the action enum value (rather than string)
  if (action == "play") {
    command.set_action(client::MusicCommand::PLAY);
  } else if (action == "pause") {
    command.set_action(client::MusicCommand::PAUSE);
  } else if (action == "resume") {
    command.set_action(client::MusicCommand::RESUME);
  } else if (action == "stop") {
    command.set_action(client::MusicCommand::STOP);
  } else {
    LOG_ERROR("Unknown action: {}", action);
    return;
  }

  command.set_position(position);
  command.set_wall_clock(target_time_ns);
  command.set_wait_time(0);  // No longer needed with our approach
  command.set_command_id(GenerateCommandId());

  int success_count = 0;

  // First, try to send via bidirectional streams (new method)
  {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    for (auto& [peer_address, stream_state] : peer_streams_) {
      if (stream_state->IsActive() && stream_state->SendCommand(command)) {
        success_count++;
      }
    }
  }

  // If some peers don't have active streams, fall back to the legacy method
  if (success_count < peer_list.size()) {
    // Only get peers that don't have active streams
    std::vector<std::string> legacy_peers;
    {
      std::lock_guard<std::mutex> streams_lock(streams_mutex_);
      for (const auto& peer : peer_list) {
        if (peer_streams_.find(peer) == peer_streams_.end() ||
            !peer_streams_[peer]->IsActive()) {
          legacy_peers.push_back(peer);
        }
      }
    }

    // Use legacy RPC method for these peers
    for (const auto& peer_address : legacy_peers) {
      client::CommandStatus response;
      grpc::ClientContext context;

      // Use adaptive timeout based on network conditions
      int timeout_ms = std::max(
          1000, static_cast<int>(sync_clock_.GetMaxRtt() / 1000000) * 2 + 500);
      context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::milliseconds(timeout_ms));

      // Get the stub with mutex protection
      {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto it = peer_stubs_.find(peer_address);
        if (it == peer_stubs_.end()) {
          LOG_WARN("Peer {} disconnected before broadcast", peer_address);
          continue;
        }

        // Use the stub to send the command
        auto status =
            it->second->SendMusicCommand(&context, command, &response);
        if (!status.ok()) {
          LOG_ERROR("Failed to send command to peer {}: {}", peer_address,
                    status.error_message());
        } else {
          success_count++;
        }
      }
    }
  }

  LOG_INFO("Broadcast complete: successfully sent to {}/{} peers",
           success_count, peer_list.size());

  // Now we need to wait until the target time before executing locally
  SyncClock::SleepUntil(target_time_ns);

  // Command execution is handled by the caller
}

void PeerNetwork::BroadcastGossip() {
  // Get the list of peers
  std::vector<std::string> peer_addresses = GetConnectedPeers();

  if (peer_addresses.empty()) {
    LOG_DEBUG("No peers to gossip with");
    return;
  }

  LOG_INFO("Broadcasting gossip to {} peers", peer_addresses.size());

  // Create the gossip request
  client::GossipRequest request;
  for (const auto& peer : peer_addresses) {
    request.add_peer_list(peer);
  }

  // Add our own server address if available
  if (server_port_ > 0) {
    // Get our external IP (this is a simplification, in a real system you'd
    // need a more robust way to determine your externally-accessible address)
    std::string our_address = "localhost:" + std::to_string(server_port_);
    request.add_peer_list(our_address);
  }

  // Track success count for logging
  int success_count = 0;

  // Collect futures for parallel execution
  std::vector<std::future<bool>> futures;

  // Send gossip to all peers in parallel
  for (const auto& peer_address : peer_addresses) {
    auto future = std::async(
        std::launch::async, [this, peer_address, &request]() -> bool {
          client::GossipResponse response;
          grpc::ClientContext context;

          // Set a reasonable deadline
          context.set_deadline(std::chrono::system_clock::now() +
                               std::chrono::seconds(2));

          // Get the stub with mutex protection
          std::shared_ptr<client::ClientHandler::Stub> stub;
          {
            std::lock_guard<std::mutex> lock(peers_mutex_);
            auto it = peer_stubs_.find(peer_address);
            if (it == peer_stubs_.end()) {
              LOG_WARN("Peer {} disconnected before gossip", peer_address);
              return false;
            }
            stub = it->second;
          }

          // Use the stub to send the gossip
          auto status = stub->Gossip(&context, request, &response);
          if (!status.ok()) {
            LOG_ERROR("Failed to send gossip to peer {}: {}", peer_address,
                      status.error_message());
            return false;
          }

          return true;
        });

    futures.push_back(std::move(future));
  }

  // Wait for all futures to complete
  for (auto& future : futures) {
    if (future.get()) {
      success_count++;
    }
  }

  LOG_INFO("Gossip complete: successfully sent to {}/{} peers", success_count,
           peer_addresses.size());
}

void PeerNetwork::ProcessCommandQueue() {
  LOG_INFO("Command queue processing thread started");

  while (queue_running_) {
    QueuedCommand command;
    bool has_command = false;

    // Wait for a command to be available
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      if (queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
            return !queue_running_ || !command_queue_.empty();
          })) {
        if (!queue_running_) {
          break;
        }

        if (!command_queue_.empty()) {
          command = command_queue_.top();
          command_queue_.pop();
          has_command = true;
        }
      }
    }

    // Process the command if we have one
    if (has_command) {
      // Check if it's time to execute
      auto now = std::chrono::system_clock::now();
      if (command.timestamp > now) {
        // Sleep until the command's execution time
        std::this_thread::sleep_until(command.timestamp);
      }

      // Execute the command (implementation depends on your needs)
      LOG_INFO("Executing queued command: {}", command.command.command_id());

      // ... Command execution code goes here ...
    }
  }

  LOG_INFO("Command queue processing thread stopped");
}

int PeerNetwork::GenerateCommandId() { return next_command_id_++; }