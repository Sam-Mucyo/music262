#include <grpcpp/grpcpp.h>

#include <chrono>

#include "audio_service.grpc.pb.h"
#include "include/audio_service_interface.h"
#include "logger.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;

namespace music262 {

class GrpcAudioService : public AudioServiceInterface {
 public:
  GrpcAudioService(std::shared_ptr<Channel> channel)
      : stub_(audio_service::audio_service::NewStub(channel)) {
    LOG_DEBUG("GrpcAudioService initialized");
  }

  ~GrpcAudioService() override { LOG_DEBUG("GrpcAudioService shutting down"); }

  std::vector<std::string> GetPlaylist() override {
    LOG_DEBUG("Requesting playlist from server");

    audio_service::PlaylistRequest request;
    audio_service::PlaylistResponse response;
    ClientContext context;

    Status status = stub_->GetPlaylist(&context, request, &response);

    std::vector<std::string> playlist;
    if (status.ok()) {
      for (const auto& song : response.song_names()) {
        playlist.push_back(song);
      }
      LOG_INFO("Retrieved playlist with {} songs", playlist.size());
    } else {
      LOG_ERROR("GetPlaylist RPC failed: {}", status.error_message());
    }

    return playlist;
  }

  bool LoadAudio(int song_num, int channel_idx, AudioChunkCallback callback) override {
    LOG_INFO("Loading audio for song: {}", song_num);

    // Create a request to load audio
    audio_service::LoadAudioRequest request;
    request.set_song_num(song_num);
    request.set_channel_index(channel_idx);

    ClientContext context;
    std::unique_ptr<ClientReader<audio_service::AudioChunk>> reader(
        stub_->LoadAudio(&context, request));

    // Process the audio stream
    audio_service::AudioChunk chunk;
    size_t total_bytes = 0;

    while (reader->Read(&chunk)) {
      const std::string& data = chunk.data();
      std::vector<char> chunk_data(data.begin(), data.end());

      // Call the callback with the chunk data
      callback(chunk_data);

      total_bytes += data.size();
    }

    Status status = reader->Finish();
    if (status.ok()) {
      LOG_INFO("Successfully received {} bytes for {}", total_bytes, song_num);
      return true;
    } else {
      LOG_ERROR("LoadAudio RPC failed: {}", status.error_message());
      return false;
    }
  }

  std::vector<std::string> GetPeerClientIPs() override {
    LOG_DEBUG("Requesting peer client IPs from server");

    audio_service::PeerListRequest request;
    audio_service::PeerListResponse response;
    ClientContext context;

    Status status = stub_->GetPeerClientIPs(&context, request, &response);

    std::vector<std::string> peers;
    if (status.ok()) {
      for (const auto& peer : response.client_ips()) {
        peers.push_back(peer);
      }
      LOG_INFO("Retrieved {} peer IPs from server", peers.size());
    } else {
      LOG_ERROR("GetPeerClientIPs RPC failed: {}", status.error_message());
    }

    return peers;
  }

  bool IsServerConnected() override {
    LOG_DEBUG("Verifying server connection");

    // Try to get the playlist as a simple way to check if server is responsive
    audio_service::PlaylistRequest request;
    audio_service::PlaylistResponse response;
    ClientContext context;

    // Set a deadline for the RPC
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(2));

    Status status = stub_->GetPlaylist(&context, request, &response);

    if (status.ok()) {
      LOG_INFO("Server connection successful");
      return true;
    } else {
      LOG_ERROR("Failed to connect to server: {}", status.error_message());
      return false;
    }
  }

 private:
  std::unique_ptr<audio_service::audio_service::Stub> stub_;
};

// Factory implementation
std::unique_ptr<AudioServiceInterface> CreateAudioService(
    const std::string& server_address) {
  auto channel =
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
  return std::make_unique<GrpcAudioService>(channel);
}

}  // namespace music262
