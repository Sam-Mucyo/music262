#include "include/client.h"

#include <filesystem>
#include <fstream>
#include <iostream>

using audio_service::AudioChunk;
using audio_service::LoadAudioRequest;
using audio_service::PeerListRequest;
using audio_service::PeerListResponse;
using audio_service::PlaylistRequest;
using audio_service::PlaylistResponse;
AudioClient::AudioClient(std::shared_ptr<Channel> channel)
    : stub_(audio_service::audio_service::NewStub(channel)), player_() {}

std::vector<std::string> AudioClient::GetPlaylist() {
  PlaylistRequest request;
  PlaylistResponse response;
  ClientContext context;

  Status status = stub_->GetPlaylist(&context, request, &response);

  std::vector<std::string> playlist;
  if (status.ok()) {
    for (const auto& song : response.song_names()) {
      playlist.push_back(song);
    }
  } else {
    std::cout << "GetPlaylist RPC failed: " << status.error_message()
              << std::endl;
  }

  return playlist;
}

bool AudioClient::LoadAudio(const std::string& song_name) {
  LoadAudioRequest request;
  request.set_song_name(song_name);

  ClientContext context;
  std::unique_ptr<ClientReader<AudioChunk>> reader(
      stub_->LoadAudio(&context, request));

  // Clear previously loaded audio data
  audio_data_.clear();

  // Process the audio stream
  AudioChunk chunk;
  size_t total_bytes = 0;

  while (reader->Read(&chunk)) {
    const std::string& data = chunk.data();
    // Append data to our in-memory buffer
    audio_data_.insert(audio_data_.end(), data.begin(), data.end());
    total_bytes += data.size();
  }

  Status status = reader->Finish();
  if (status.ok()) {
    std::cout << "Successfully received " << total_bytes << " bytes for "
              << song_name << std::endl;

    // Load audio data into player from memory
    if (!player_.loadFromMemory(audio_data_.data(), audio_data_.size())) {
      std::cerr << "Failed to load audio data into player" << std::endl;
      return false;
    }

    return true;
  } else {
    std::cout << "LoadAudio RPC failed: " << status.error_message()
              << std::endl;
    return false;
  }
}

void AudioClient::Play() { player_.play(); }

void AudioClient::Pause() { player_.pause(); }

void AudioClient::Resume() { player_.resume(); }

void AudioClient::Stop() { player_.stop(); }

unsigned int AudioClient::GetPosition() const { return player_.get_position(); }

std::vector<std::string> AudioClient::GetPeerClientIPs() {
  PeerListRequest request;
  PeerListResponse response;
  ClientContext context;

  Status status = stub_->GetPeerClientIPs(&context, request, &response);

  std::vector<std::string> peers;
  if (status.ok()) {
    for (const auto& peer : response.client_ips()) {
      peers.push_back(peer);
    }
  } else {
    std::cout << "GetPeerClientIPs RPC failed: " << status.error_message()
              << std::endl;
  }

  return peers;
}
