#include <memory>
#include <string>

#include "audio_service_interface.h"
#include "include/client.h"
#include "include/peer_network.h"
#include "peer_service_interface.h"

/**
 * Factory function to create an AudioClient with real gRPC service
 * implementations
 */
std::unique_ptr<AudioClient> CreateAudioClient(
    const std::string& server_address) {
  // Create the audio service implementation
  auto audio_service = music262::CreateAudioService(server_address);

  // Create the client with the audio service
  auto client = std::make_unique<AudioClient>(std::move(audio_service));

  // Create the peer network with the client
  auto peer_network = std::make_shared<PeerNetwork>(client.get());

  // Set the peer network in the client
  client->SetPeerNetwork(peer_network);

  return client;
}
