#include "include/client.h"

#include "include/peer_network.h"
#include "logger.h"

using audio_service::AudioChunk;
using audio_service::LoadAudioRequest;
using audio_service::PeerListRequest;
using audio_service::PeerListResponse;
using audio_service::PlaylistRequest;
using audio_service::PlaylistResponse;

AudioClient::AudioClient(std::shared_ptr<Channel> channel)
    : stub_(audio_service::audio_service::NewStub(channel)),
      player_(),
      peer_sync_enabled_(false) {
  LOG_DEBUG("AudioClient initialized");
}

AudioClient::~AudioClient() { LOG_DEBUG("AudioClient shutting down"); }

std::vector<std::string> AudioClient::GetPlaylist() {
  LOG_DEBUG("Requesting playlist from server");

  PlaylistRequest request;
  PlaylistResponse response;
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

bool AudioClient::LoadAudio(int song_num) {
  LOG_INFO("Loading audio for song: {}", song_num);

  // Create a request to load audio
  LoadAudioRequest request;
  request.set_song_num(song_num);

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
    LOG_INFO("Successfully received {} bytes for {}", total_bytes, song_num);

    // Load audio data into player from memory
    if (!player_.loadFromMemory(audio_data_.data(), audio_data_.size())) {
      LOG_ERROR("Failed to load audio data into player");
      return false;
    }

    return true;
  } else {
    LOG_ERROR("LoadAudio RPC failed: {}", status.error_message());
    return false;
  }
}

void AudioClient::Play(int position, bool broadcast) {
  // Broadcast command to peers if enabled
  if (peer_sync_enabled_ && broadcast && peer_network_) {
    LOG_DEBUG("Broadcasting play command to peers");
    peer_network_->BroadcastCommand("play", position);
    // BroadcastCommand now handles synchronization timing internally
    // and will delay until the right time to play
  }

  // Always play locally
  player_.play();
}

void AudioClient::Pause(bool broadcast) {
  // Broadcast command to peers if enabled
  if (peer_sync_enabled_ && broadcast && peer_network_) {
    LOG_DEBUG("Broadcasting pause command to peers");
    peer_network_->BroadcastCommand("pause", player_.get_position());
    // BroadcastCommand now handles synchronization timing internally
  }

  // Always pause locally
  player_.pause();
}

void AudioClient::Resume(bool broadcast) {
  // Broadcast command to peers if enabled
  if (peer_sync_enabled_ && broadcast && peer_network_) {
    LOG_DEBUG("Broadcasting resume command to peers");
    peer_network_->BroadcastCommand("resume", player_.get_position());
    // BroadcastCommand now handles synchronization timing internally
  }

  // Always resume locally
  player_.resume();
}

void AudioClient::Stop(bool broadcast) {
  // Broadcast command to peers if enabled
  if (peer_sync_enabled_ && broadcast && peer_network_) {
    LOG_DEBUG("Broadcasting stop command to peers");
    peer_network_->BroadcastCommand("stop", 0);
    // BroadcastCommand now handles synchronization timing internally
  }

  // Always stop locally
  player_.stop();
}

unsigned int AudioClient::GetPosition() const { return player_.get_position(); }

std::vector<std::string> AudioClient::GetPeerClientIPs() {
  LOG_DEBUG("Requesting peer client IPs from server");

  PeerListRequest request;
  PeerListResponse response;
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

void AudioClient::EnablePeerSync(bool enable) {
  peer_sync_enabled_ = enable;
  LOG_INFO("Peer synchronization {}", enable ? "enabled" : "disabled");
}

void AudioClient::SetPeerNetwork(std::shared_ptr<PeerNetwork> peer_network) {
  peer_network_ = peer_network;
  LOG_DEBUG("Peer network set");
}

void AudioClient::ProcessPeerList(const std::vector<std::string>& peers) {
  if (!peer_network_) {
    LOG_WARN("Cannot process peer list: peer network not initialized");
    return;
  }

  LOG_INFO("Processing peer list with {} entries", peers.size());

  // Get our current peer list
  std::vector<std::string> current_peers = peer_network_->GetConnectedPeers();

  // Connect to new peers
  for (const auto& peer : peers) {
    // Skip connecting to ourselves
    if (peer == "localhost:" + std::to_string(peer_network_->GetServerPort())) {
      continue;
    }

    // Check if we already know this peer
    if (std::find(current_peers.begin(), current_peers.end(), peer) ==
        current_peers.end()) {
      LOG_INFO("Connecting to new peer: {}", peer);
      peer_network_->ConnectToPeer(peer);
    }
  }
}