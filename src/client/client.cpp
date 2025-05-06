#include "include/client.h"

#include "include/peer_network.h"
#include "logger.h"

AudioClient::AudioClient(
    std::unique_ptr<music262::AudioServiceInterface> audio_service)
    : audio_service_(std::move(audio_service)),
      player_(),
      peer_sync_enabled_(false),
      command_from_broadcast_(false),
      command_from_broadcast_action_(" "),
      current_song_num_(-1) {
  LOG_DEBUG("AudioClient initialized");
}

AudioClient::~AudioClient() { LOG_DEBUG("AudioClient shutting down"); }

std::vector<std::string> AudioClient::GetPlaylist() {
  LOG_DEBUG("Requesting playlist from server");

  return audio_service_->GetPlaylist();
}

bool AudioClient::LoadAudio(int song_num) {
  LOG_INFO("Loading audio for song: {}", song_num);

  // Clear previously loaded audio data
  audio_data_.clear();

  // Get channel index
  int num_channels = peer_network_->GetConnectedPeers().size() + 1;
  int channel_idx;
  if (num_channels < 2) {
    channel_idx = -1; // no channel splitting
  }
  else {
    // random chance 0 or 1
    // channel_idx = rand() % 2;
    channel_idx = peer_network_->GetServerPort() % 2;
  }

  // Use a callback to collect audio chunks
  bool success = audio_service_->LoadAudio(
      song_num, channel_idx, [this](const std::vector<char>& data) {
        // Append data to our in-memory buffer
        audio_data_.insert(audio_data_.end(), data.begin(), data.end());
      });

  if (success) {
    LOG_INFO("Successfully received {} bytes for {}", audio_data_.size(),
             song_num);

    // Load audio data into player from memory
    if (!player_.loadFromMemory(audio_data_.data(), audio_data_.size())) {
      LOG_ERROR("Failed to load audio data into player");
      return false;
    }
    // Track loaded song for sync
    current_song_num_ = song_num;
    return true;
  } else {
    LOG_ERROR("LoadAudio failed");
    return false;
  }
}

void AudioClient::Play() {

  std::lock_guard<std::mutex> lk(player_mutex_);

  // Broadcast load then play to peers if sync-enabled
  if (peer_sync_enabled_ && !command_from_broadcast_ && peer_network_) {
    if (current_song_num_ >= 0) {
      LOG_DEBUG("Broadcasting load command to peers for song {}",
                current_song_num_);
      peer_network_->BroadcastLoad(current_song_num_);
    } else {
      LOG_WARN("No song loaded to broadcast load");
    }
    LOG_DEBUG("Broadcasting play command to peers");
    peer_network_->BroadcastCommand("play", player_.get_position());
  }
  player_.play();
}

void AudioClient::Pause() {

  std::lock_guard<std::mutex> lk(player_mutex_);

  // Broadcast command to peers if enabled and not from broadcast
  if (peer_sync_enabled_ && !command_from_broadcast_ && peer_network_) {
    LOG_DEBUG("Broadcasting pause command to peers");
    peer_network_->BroadcastCommand("pause", player_.get_position());
    // BroadcastCommand now handles synchronization timing internally
  }
  player_.pause();
}

void AudioClient::Resume() {

  std::lock_guard<std::mutex> lk(player_mutex_);

  // Broadcast command to peers if enabled and not from broadcast
  if (peer_sync_enabled_ && !command_from_broadcast_ && peer_network_) {
    LOG_DEBUG("Broadcasting resume command to peers");
    peer_network_->BroadcastCommand("resume", player_.get_position());
    // BroadcastCommand now handles synchronization timing internally
  }
  player_.resume();
}

void AudioClient::Stop() {

  std::lock_guard<std::mutex> lk(player_mutex_);

  // Broadcast command to peers if enabled and not from broadcast
  if (peer_sync_enabled_ && !command_from_broadcast_ && peer_network_) {
    LOG_DEBUG("Broadcasting stop command to peers");
    peer_network_->BroadcastCommand("stop", 0);
    // BroadcastCommand now handles synchronization timing internally
  }
  player_.stop();
}

void AudioClient::SeekTo(int seconds) {
  const WavHeader& header = GetPlayer().get_header();
  unsigned int bytesPerSecond = header.sampleRate * header.numChannels * (header.bitsPerSample / 8);
  unsigned int targetByte = seconds * bytesPerSecond;
  GetPlayer().get_position_ref().store(targetByte);
}

unsigned int AudioClient::GetPosition() const { return player_.get_position(); }

std::vector<std::string> AudioClient::GetPeerClientIPs() {
  LOG_DEBUG("Requesting peer client IPs from server");

  return audio_service_->GetPeerClientIPs();
}

void AudioClient::EnablePeerSync(bool enable) {
  peer_sync_enabled_ = enable;
  LOG_INFO("Peer synchronization {}", enable ? "enabled" : "disabled");
}

void AudioClient::SetPeerNetwork(std::shared_ptr<PeerNetwork> peer_network) {
  peer_network_ = peer_network;
  LOG_DEBUG("Peer network set");
}

bool AudioClient::IsServerConnected() {
  LOG_DEBUG("Verifying server connection");

  return audio_service_->IsServerConnected();
}
