#include <arpa/inet.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "../common/include/logger.h"
#include "audio_service.grpc.pb.h"
#include "include/audio_server.h"
#include "../client/include/wavheader.h"

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


// Helper: extract stereo channel from WAV file data
std::vector<char> ExtractStereoChannel(const std::vector<char>& full,
  int channel_index /* -1 = full, 0 = L, 1 = R */) {
if (channel_index == -1) {
return full;  // full audio requested
}

const WavHeader* hdr = reinterpret_cast<const WavHeader*>(full.data());
if (hdr->bitsPerSample % 8 != 0) {
LOG_ERROR("Unsupported bitsPerSample: {}", hdr->bitsPerSample);
return full;
}

const uint16_t bytesPerSample = hdr->bitsPerSample / 8;
const size_t totalFrames = (full.size() - sizeof(WavHeader)) / (bytesPerSample * hdr->numChannels);

std::vector<char> mono_data;
mono_data.resize(sizeof(WavHeader) + totalFrames * bytesPerSample);

// Copy and rewrite header for mono
WavHeader out_hdr = *hdr;
out_hdr.numChannels = 1;
out_hdr.byteRate = out_hdr.sampleRate * bytesPerSample;
out_hdr.blockAlign = bytesPerSample;
out_hdr.dataSize = static_cast<uint32_t>(totalFrames * bytesPerSample);
std::memcpy(mono_data.data(), &out_hdr, sizeof(WavHeader));

const char* src = full.data() + sizeof(WavHeader);
char* dst = mono_data.data() + sizeof(WavHeader);

if (hdr->numChannels == 2) {
// True stereo: extract channel 0 (left) or 1 (right)
src += channel_index * bytesPerSample;
for (size_t i = 0; i < totalFrames; ++i) {
std::memcpy(dst, src, bytesPerSample);
dst += bytesPerSample;
src += bytesPerSample * 2;  // move to same channel in next frame
}
} else if (hdr->numChannels == 1) {
// Mono fallback: alternate samples into "channels"
for (size_t i = 0; i < totalFrames; ++i) {
if ((i % 2) == channel_index) {
const char* sample = src + i * bytesPerSample;
std::memcpy(dst, sample, bytesPerSample);
dst += bytesPerSample;
}
}
// Resize to match number of copied samples
mono_data.resize(sizeof(WavHeader) + (dst - (mono_data.data() + sizeof(WavHeader))));
} else {
LOG_WARN("Unsupported numChannels: {}", hdr->numChannels);
return full;
}

return mono_data;
}

// AudioServiceImpl class that implements the gRPC service
// This class now delegates to the AudioServer class for business logic
class AudioServiceImpl final : public audio_service::audio_service::Service {
 public:
  AudioServiceImpl(std::shared_ptr<AudioServer> server) : server_(server) {
    LOG_INFO("AudioServiceImpl initialized");
  }

  grpc::Status GetPlaylist(grpc::ServerContext* context,
                           const audio_service::PlaylistRequest* request,
                           audio_service::PlaylistResponse* response) override {
    LOG_INFO("Received playlist request from client");

    // Register client in the connected clients list
    std::string client_ip = context->peer();
    server_->RegisterClient(client_ip);

    // Add each song filename to the response
    for (const auto& song : server_->GetPlaylist()) {
      response->add_song_names(song);
    }

    return grpc::Status::OK;
  }

  grpc::Status LoadAudio(
      grpc::ServerContext* context,
      const audio_service::LoadAudioRequest* request,
      grpc::ServerWriter<audio_service::AudioChunk>* writer) override {
    int song_num = request->song_num();
    int channel_index = request->channel_index();
    LOG_INFO("Received request to load song: {} from channel: {}", song_num, channel_index);

    // Get the file path from the server
    std::string file_path = server_->GetAudioFilePath(song_num);
    if (file_path.empty()) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, "Song not found");
    }

    // Open the audio file
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
      LOG_ERROR("Failed to open song file: {}", file_path);
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Failed to open song file");
    }

    // Register client in the connected clients list
    std::string client_ip = context->peer();
    server_->RegisterClient(client_ip);

    // If split channel
    if (channel_index == 0 || channel_index == 1) {
      // Load full WAV file into memory
      std::vector<char> full_data((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
    
      std::vector<char> mono_data = ExtractStereoChannel(full_data, channel_index);
    
      // Send mono data in chunks
      constexpr size_t CHUNK_SIZE = 64 * 1024;  // 64 KB chunks
      size_t total_bytes_sent = 0;
      for (size_t offset = 0; offset < mono_data.size(); offset += CHUNK_SIZE) {
        size_t chunk_size = std::min(CHUNK_SIZE, mono_data.size() - offset);
        audio_service::AudioChunk chunk;
        chunk.set_data(mono_data.data() + offset, chunk_size);
        if (!writer->Write(chunk)) {
          LOG_ERROR("Failed to write mono audio chunk to client");
          break;
        }
        total_bytes_sent += chunk_size;
      }
    
      LOG_INFO("Sent {} bytes of mono audio data (channel {})",
               total_bytes_sent, channel_index);
      return grpc::Status::OK;
    }    

    // If sending as one channel: read and send the file in chunks
    constexpr size_t CHUNK_SIZE = 64 * 1024;  // 64 KB chunks
    std::vector<char> buffer(CHUNK_SIZE);
    size_t total_bytes_sent = 0;

    while (file) {
      file.read(buffer.data(), CHUNK_SIZE);
      std::streamsize bytes_read = file.gcount();

      if (bytes_read > 0) {
        audio_service::AudioChunk chunk;
        chunk.set_data(buffer.data(), bytes_read);

        if (!writer->Write(chunk)) {
          LOG_ERROR("Failed to write audio chunk to client");
          break;
        }

        total_bytes_sent += bytes_read;
      }
    }

    LOG_INFO("Sent {} bytes of audio data", total_bytes_sent);
    return grpc::Status::OK;
  }

  grpc::Status GetPeerClientIPs(
      grpc::ServerContext* context,
      const audio_service::PeerListRequest* request,
      audio_service::PeerListResponse* response) override {
    LOG_INFO("Received peer list request");

    // Register the requesting client
    std::string requester_ip = context->peer();
    server_->RegisterClient(requester_ip);

    // Get connected clients excluding the requester
    std::vector<std::string> clients =
        server_->GetConnectedClients(requester_ip);

    // Add all clients to the response
    for (const auto& client : clients) {
      response->add_client_ips(client);
    }

    return grpc::Status::OK;
  }

  // Helper function to extract clean IP:port from gRPC peer string
  std::string ExtractIPFromPeer(const std::string& peer) {
    // gRPC peer strings are typically in format "ipv4:127.0.0.1:12345" or
    // "ipv6:[::1]:12345"
    size_t colon_pos = peer.find(':');
    if (colon_pos != std::string::npos) {
      // Skip the prefix (e.g., "ipv4:" or "ipv6:")
      std::string address = peer.substr(colon_pos + 1);
      return address;
    }
    return peer;  // Return original if format is unexpected
  }

  // Get current server stats for the CLI
  void PrintStatus(int port) const {
    std::string local_ip = GetLocalIPAddress();
    if (local_ip.empty()) local_ip = "127.0.0.1";

    server_->PrintStatus(local_ip, port);
  }

 private:
  std::shared_ptr<AudioServer> server_;
};

void displayHelp() {
  std::cout << "\nCommands:" << std::endl;
  std::cout << "  status            - Show server status (IP Address and port, "
               "active clients, etc.)"
            << std::endl;
  std::cout << "  help              - Show this help message" << std::endl;
  std::cout << "  exit              - Shutdown the server" << std::endl;
}

void RunServer(AudioServiceImpl* service, int port) {
  std::string server_address = "0.0.0.0:" + std::to_string(port);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG_INFO("Server listening on {}", server_address);

  server->Wait();
}

int main(int argc, char* argv[]) {
  // Initialize the logger
  Logger::init("music_server");

  // Default values
  int port = 50051;
  std::string audio_directory = "../sample_music";

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    } else if (arg == "--audio_dir" && i + 1 < argc) {
      audio_directory = argv[++i];
    }
  }

  // Enable gRPC reflection service
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  // Create the AudioServer instance (business logic)
  auto audio_server = std::make_shared<AudioServer>(audio_directory);

  // Create the service implementation (networking layer)
  AudioServiceImpl service(audio_server);

  // Start the gRPC server in a separate thread
  std::thread server_thread(RunServer, &service, port);

  std::cout << "Music Streaming Server - Starting up..." << std::endl;
  std::cout << "Configured to use port: " << port << std::endl;
  std::cout << "Audio directory: " << audio_directory << std::endl;

  // Determine and log actual network IP and port
  std::string local_ip = GetLocalIPAddress();
  if (local_ip.empty()) local_ip = "127.0.0.1";
  std::cout << "Server listening on " << local_ip << ":" << port << std::endl;

  std::cout << "Welcome to the Music262 Server!" << std::endl;
  std::cout << "Type 'help' to see available commands." << std::endl;

  // Main command loop
  std::string command;
  bool running = true;

  while (running) {
    std::cout << "\n> ";  // Keep this as std::cout for the prompt
    std::getline(std::cin, command);

    if (command == "status") {
      service.PrintStatus(port);
    } else if (command == "help") {
      displayHelp();
    } else if (command == "exit") {
      std::cout << "Shutting down server..." << std::endl;
      running = false;
    } else if (!command.empty()) {
      LOG_WARN("Unknown command: {}. Type 'help' for available commands.",
               command);
    }
  }

  std::cout << "Server shutdown complete." << std::endl;
  return 0;
}