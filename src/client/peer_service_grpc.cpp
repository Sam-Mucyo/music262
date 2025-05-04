#include <grpcpp/grpcpp.h>

#include "audio_sync.grpc.pb.h"
#include "include/peer_service_interface.h"
#include "logger.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace music262 {

class GrpcPeerService : public PeerServiceInterface {
 public:
  GrpcPeerService() { LOG_DEBUG("GrpcPeerService initialized"); }

  ~GrpcPeerService() override {
    LOG_DEBUG("GrpcPeerService shutting down");
    // Clean up any remaining stubs
    std::lock_guard<std::mutex> lock(stubs_mutex_);
    peer_stubs_.clear();
  }

  bool Ping(const std::string& peer_address, int64_t& t1,
            int64_t& t2) override {
    LOG_DEBUG("Sending ping to peer: {}", peer_address);

    auto stub = GetOrCreateStub(peer_address);
    if (!stub) {
      return false;
    }

    client::PingRequest request;
    client::PingResponse response;
    ClientContext context;

    // Set a short deadline for ping
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(500));

    Status status = stub->Ping(&context, request, &response);

    if (status.ok()) {
      t1 = response.t1();
      t2 = response.t2();
      LOG_DEBUG("Ping successful to {}: t1={}, t2={}", peer_address, t1, t2);
      return true;
    } else {
      LOG_ERROR("Ping failed to {}: {}", peer_address, status.error_message());
      RemoveStub(peer_address);
      return false;
    }
  }

  bool Gossip(const std::string& peer_address,
              const std::vector<std::string>& peer_list) override {
    LOG_DEBUG("Sending gossip to peer: {}", peer_address);

    auto stub = GetOrCreateStub(peer_address);
    if (!stub) {
      return false;
    }

    client::GossipRequest request;
    for (const auto& peer : peer_list) {
      request.add_peer_list(peer);
    }

    client::GossipResponse response;
    ClientContext context;

    // Set a reasonable deadline for gossip
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(2));

    Status status = stub->Gossip(&context, request, &response);

    if (status.ok()) {
      LOG_INFO("Gossip successful to {}", peer_address);
      return true;
    } else {
      LOG_ERROR("Gossip failed to {}: {}", peer_address,
                status.error_message());
      RemoveStub(peer_address);
      return false;
    }
  }

  bool SendMusicCommand(const std::string& peer_address,
                        const std::string& action, int position,
                        int64_t wait_time_ms, int song_num) override {
    LOG_DEBUG("Sending music command to peer: {}, action: {}", peer_address,
              action);

    auto stub = GetOrCreateStub(peer_address);
    if (!stub) {
      return false;
    }

    client::MusicRequest request;
    request.set_action(action);
    request.set_position(position);
    request.set_wait_time_ms(wait_time_ms);

    if (song_num >= 0) {
      request.set_song_num(song_num);
    }

    client::MusicResponse response;
    ClientContext context;

    // Set a reasonable deadline
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(5));

    Status status = stub->SendMusicCommand(&context, request, &response);

    if (status.ok()) {
      LOG_INFO("Music command {} successful to {}", action, peer_address);
      return true;
    } else {
      LOG_ERROR("Music command {} failed to {}: {}", action, peer_address,
                status.error_message());
      RemoveStub(peer_address);
      return false;
    }
  }

  bool GetPosition(const std::string& peer_address, int& position) override {
    LOG_DEBUG("Getting position from peer: {}", peer_address);

    auto stub = GetOrCreateStub(peer_address);
    if (!stub) {
      return false;
    }

    client::GetPositionRequest request;
    client::GetPositionResponse response;
    ClientContext context;

    // Set a short deadline
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(500));

    Status status = stub->GetPosition(&context, request, &response);

    if (status.ok()) {
      position = response.position();
      LOG_DEBUG("GetPosition successful from {}: position={}", peer_address,
                position);
      return true;
    } else {
      LOG_ERROR("GetPosition failed from {}: {}", peer_address,
                status.error_message());
      RemoveStub(peer_address);
      return false;
    }
  }

 private:
  std::shared_ptr<client::ClientHandler::Stub> GetOrCreateStub(
      const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(stubs_mutex_);

    auto it = peer_stubs_.find(peer_address);
    if (it != peer_stubs_.end()) {
      return it->second;
    }

    // Create a new stub
    auto channel =
        grpc::CreateChannel(peer_address, grpc::InsecureChannelCredentials());
    auto stub = client::ClientHandler::NewStub(channel);

    if (stub) {
      // Convert unique_ptr to shared_ptr
      std::shared_ptr<client::ClientHandler::Stub> shared_stub(stub.release());
      peer_stubs_[peer_address] = shared_stub;
      LOG_DEBUG("Created new stub for peer: {}", peer_address);
      return shared_stub;
    } else {
      LOG_ERROR("Failed to create stub for peer: {}", peer_address);
      return nullptr;
    }
  }

  void RemoveStub(const std::string& peer_address) {
    std::lock_guard<std::mutex> lock(stubs_mutex_);
    peer_stubs_.erase(peer_address);
    LOG_DEBUG("Removed stub for peer: {}", peer_address);
  }

  std::map<std::string, std::shared_ptr<client::ClientHandler::Stub>>
      peer_stubs_;
  std::mutex stubs_mutex_;
};

// Factory implementation
std::unique_ptr<PeerServiceInterface> CreatePeerService() {
  return std::make_unique<GrpcPeerService>();
}

}  // namespace music262
