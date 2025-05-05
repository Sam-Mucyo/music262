#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "audio_service_interface.h"
#include "audioplayer.h"
#include "peer_service_interface.h"

// Forward declaration
class PeerNetwork;

class AudioClient {
 public:
  // Constructor with service interfaces for dependency injection
  AudioClient(std::unique_ptr<music262::AudioServiceInterface> audio_service);
  ~AudioClient();

  // Request the playlist from the server
  std::vector<std::string> GetPlaylist();

  // Load audio data for a specific song
  bool LoadAudio(int song_num);

  // Play the currently loaded audio
  void Play();

  // Pause the currently playing audio
  void Pause();

  // Resume the paused audio
  void Resume();

  // Stop the currently playing audio
  void Stop();

  // Get the current playback position
  unsigned int GetPosition() const;

  // Get the list of connected client IPs
  std::vector<std::string> GetPeerClientIPs();

  // Verify connection to the server
  bool IsServerConnected();

  // Get reference to audio player (for peer service)
  AudioPlayer& GetPlayer() { return player_; }

  // Get reference to the peer network
  std::shared_ptr<PeerNetwork> GetPeerNetwork() { return peer_network_; }

  // Get reference to the audio player's audio data
  std::vector<char>& GetAudioData() { return audio_data_; }

  // Control whether commands should be broadcast to peers
  void EnablePeerSync(bool enable);
  bool IsPeerSyncEnabled() const { return peer_sync_enabled_; }

  // Set the peer network for command broadcasting
  void SetPeerNetwork(std::shared_ptr<PeerNetwork> peer_network);

  // Flag to prevent broadcast loops
  void SetCommandFromBroadcast(bool value) { command_from_broadcast_ = value; }
  bool IsCommandFromBroadcast() const { return command_from_broadcast_; }

 private:
  std::unique_ptr<music262::AudioServiceInterface> audio_service_;
  AudioPlayer player_;
  std::vector<char> audio_data_;
  int current_song_num_{-1};  // index of last loaded song

  // Peer synchronization
  std::shared_ptr<PeerNetwork> peer_network_;
  std::atomic<bool> peer_sync_enabled_{false};
  std::atomic<bool> command_from_broadcast_{false};
};
