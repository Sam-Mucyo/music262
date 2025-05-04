#include "include/audio_server.h"

#include <iostream>

#include "../common/include/logger.h"

// Helper function to get all .wav files in the given directory
std::vector<std::string> get_audio_files(const std::string& directory) {
  std::vector<std::string> audio_files;

  if (!fs::exists(directory) || !fs::is_directory(directory)) {
    std::cerr << "Directory does not exist: " << directory << std::endl;
    return audio_files;
  }

  for (const auto& entry : fs::directory_iterator(directory)) {
    if (entry.is_regular_file() && entry.path().extension() == ".wav") {
      audio_files.push_back(entry.path().filename());
    }
  }

  return audio_files;
}

AudioServer::AudioServer(const std::string& audio_dir)
    : audio_directory_(audio_dir), next_client_id_(0) {
  // Load the available songs on startup
  playlist_ = get_audio_files(audio_directory_);
  LOG_INFO("Loaded {} songs from {}", playlist_.size(), audio_directory_);
}

std::vector<std::string> AudioServer::GetPlaylist() const { return playlist_; }

std::string AudioServer::GetAudioFilePath(int song_num) const {
  if (song_num <= 0 || song_num > static_cast<int>(playlist_.size())) {
    LOG_ERROR("Invalid song number: {}", song_num);
    return "";
  }

  std::string song_name = playlist_[song_num - 1];
  std::string file_path = audio_directory_ + "/" + song_name;

  if (!fs::exists(file_path)) {
    LOG_ERROR("Song file not found: {}", file_path);
    return "";
  }

  return file_path;
}

int AudioServer::RegisterClient(const std::string& client_id) {
  std::lock_guard<std::mutex> lock(clients_mutex_);

  // Clean up the IP address by extracting and decoding it
  std::string clean_ip = ExtractIPFromPeer(client_id);

  // Check if this client is already registered
  for (const auto& [id, ip] : connected_clients_) {
    if (ip == clean_ip) {
      return id;  // Client already registered, return existing ID
    }
  }

  // Register new client with the clean IP
  int new_id = next_client_id_++;
  connected_clients_[new_id] = clean_ip;
  LOG_INFO("Client connected: {} (raw: {})", clean_ip, client_id);

  return new_id;
}

std::vector<std::string> AudioServer::GetConnectedClients(
    const std::string& exclude_client_id) const {
  std::lock_guard<std::mutex> lock(clients_mutex_);

  std::string clean_exclude_id = "";
  if (!exclude_client_id.empty()) {
    clean_exclude_id = ExtractIPFromPeer(exclude_client_id);
  }

  std::vector<std::string> clients;
  for (const auto& [id, ip] : connected_clients_) {
    if (ip != clean_exclude_id) {
      clients.push_back(ip);
    }
  }

  return clients;
}

std::string AudioServer::ExtractIPFromPeer(const std::string& peer) {
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

void AudioServer::PrintStatus(const std::string& local_ip, int port) const {
  std::cout << "Server Status:" << std::endl;
  std::cout << "  Songs available: " << playlist_.size() << std::endl;

  std::cout << "  IP Address: " << local_ip << std::endl;
  std::cout << "  Port: " << port << std::endl;

  std::lock_guard<std::mutex> lock(clients_mutex_);
  std::cout << "  Connected clients: " << connected_clients_.size()
            << std::endl;
  int i = 1;
  for (const auto& [id, ip] : connected_clients_) {
    std::cout << "    " << i++ << ". " << ip << std::endl;
  }
}
