#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "audio_service.grpc.pb.h"
#include "audioplayer.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;

// Forward declaration
class PeerNetwork;

class AudioClient {
 public:
  AudioClient(std::shared_ptr<Channel> channel);
  virtual ~AudioClient();

  // Request the playlist from the server
  virtual std::vector<std::string> GetPlaylist();

  // Load audio data for a specific song
  virtual bool LoadAudio(int song_num);

  // Play the currently loaded audio
  virtual void Play();

  // Pause the currently playing audio
  virtual void Pause();

  // Resume the paused audio
  virtual void Resume();

  // Stop the currently playing audio
  virtual void Stop();

  // Get the current playback position
  virtual unsigned int GetPosition() const;

  // Get the list of connected client IPs
  virtual std::vector<std::string> GetPeerClientIPs();

  // Get reference to audio player (for peer service)
  virtual AudioPlayer& GetPlayer() { return player_; }

  // Get reference to the audio player's audio data
  virtual std::vector<char>& GetAudioData() { return audio_data_; }

  // Control whether commands should be broadcast to peers
  virtual void EnablePeerSync(bool enable);
  virtual bool IsPeerSyncEnabled() const { return peer_sync_enabled_; }

  // Set the peer network for command broadcasting
  virtual void SetPeerNetwork(std::shared_ptr<PeerNetwork> peer_network);

  // Flag to prevent broadcast loops
  virtual void SetCommandFromBroadcast(bool value) { command_from_broadcast_ = value; }
  virtual bool IsCommandFromBroadcast() const { return command_from_broadcast_; }

 private:
  std::unique_ptr<audio_service::audio_service::Stub> stub_;
  AudioPlayer player_;
  std::vector<char> audio_data_;

  // Peer synchronization
  std::shared_ptr<PeerNetwork> peer_network_;
  std::atomic<bool> peer_sync_enabled_{false};
  std::atomic<bool> command_from_broadcast_{false};
};
