#include "peer_service.h"
#include "peer_network.h"
#include <chrono>

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
    std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int>(wait_time)));
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