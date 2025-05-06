#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace music262 {

// Callback for streaming audio chunks
using AudioChunkCallback = std::function<void(const std::vector<char>& data)>;

/**
 * Interface for audio service operations
 * This abstracts the gRPC audio_service service to make testing easier
 */
class AudioServiceInterface {
 public:
  virtual ~AudioServiceInterface() = default;

  // Get the list of available songs from the server
  virtual std::vector<std::string> GetPlaylist() = 0;

  // Load audio data for a specific song
  // The callback will be called for each chunk of audio data received
  virtual bool LoadAudio(int song_num, int channel_idx, AudioChunkCallback callback) = 0;

  // Get the list of connected client IPs
  virtual std::vector<std::string> GetPeerClientIPs() = 0;

  // Check if the server is connected and responding
  virtual bool IsServerConnected() = 0;
};

// Factory function to create a concrete implementation
std::unique_ptr<AudioServiceInterface> CreateAudioService(
    const std::string& server_address);

}  // namespace music262
