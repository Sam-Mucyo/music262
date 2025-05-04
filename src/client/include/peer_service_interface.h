#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace music262 {

/**
 * Interface for peer-to-peer service operations
 * This abstracts the gRPC ClientHandler service to make testing easier
 */
class PeerServiceInterface {
 public:
  virtual ~PeerServiceInterface() = default;

  // Send a ping request to measure network latency
  virtual bool Ping(const std::string& peer_address, int64_t& t1,
                    int64_t& t2) = 0;

  // Send gossip information about known peers
  virtual bool Gossip(const std::string& peer_address,
                      const std::vector<std::string>& peer_list) = 0;

  // Send a music command to a peer
  virtual bool SendMusicCommand(const std::string& peer_address,
                                const std::string& action, int position = 0,
                                int64_t wait_time_ms = 0,
                                int song_num = -1) = 0;

  // Get the current playback position from a peer
  virtual bool GetPosition(const std::string& peer_address, int& position) = 0;
};

// Factory function to create a concrete implementation
std::unique_ptr<PeerServiceInterface> CreatePeerService();

}  // namespace music262
