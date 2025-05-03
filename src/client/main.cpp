#include <grpcpp/grpcpp.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "include/client.h"
#include "include/peer_network.h"
#include "logger.h"

void PrintUsage() {
  std::cout << "Usage: \n"
            << "  playlist - Get list of available songs\n"
            << "  play <song_num> - Load and play a song\n"
            << "  pause - Pause the currently playing song\n"
            << "  resume - Resume the currently paused song\n"
            << "  stop - Stop the currently playing song\n"
            << "  peers - Get list of connected peers from server\n"
            << "  join <ip:port> - Join a peer for synchronized playback\n"
            << "  leave <ip:port> - Leave a connected peer\n"
            << "  connections - List all active peer connections\n"
            << "  gossip - Share all active peer connections with all peers\n"
            << "  help - Show this help message\n"
            << "  quit - Exit the client\n";
}

int main(int argc, char** argv) {
  // Initialize logger
  Logger::init("music_client");

  // Default values
  const char* env_addr = std::getenv("MUSIC262_SERVER_ADDRESS");
  std::string server_address = env_addr ? env_addr : "localhost:50051";
  int p2p_port = 50052;

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--server" && i + 1 < argc) {
      server_address = argv[++i];
    } else if (arg == "--p2p-port" && i + 1 < argc) {
      p2p_port = std::stoi(argv[++i]);
    }
  }

  // Create a channel to the server
  LOG_INFO("Connecting to server at {}", server_address);
  AudioClient client(
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

  // Create peer network and start P2P server automatically
  auto peer_network = std::make_shared<PeerNetwork>(&client);
  client.SetPeerNetwork(peer_network);

  // Start peer server automatically
  if (peer_network->StartServer(p2p_port)) {
    LOG_INFO("P2P server started on port {}", p2p_port);
    std::cout << "P2P server started on port " << p2p_port << std::endl;
  } else {
    LOG_ERROR("Failed to start P2P server on port {}", p2p_port);
    std::cout
        << "Failed to start P2P server. Some functionality may be limited."
        << std::endl;
  }

  // Enable peer sync by default
  client.EnablePeerSync(true);

  bool running = true;
  std::string command;

  LOG_INFO("Client started");
  PrintUsage();

  while (running) {
    std::cout << "> ";
    std::getline(std::cin, command);

    if (command == "playlist") {
      std::vector<std::string> playlist = client.GetPlaylist();

      std::cout << "Available songs:" << std::endl;
      if (playlist.empty()) {
        std::cout << "No songs available on the server." << std::endl;
      } else {
        for (size_t i = 0; i < playlist.size(); i++) {
          std::cout << i + 1 << ". " << playlist[i] << std::endl;
        }
      }
    } else if (command.substr(0, 5) == "play ") {
      int song_num = std::stoi(command.substr(5));
      std::cout << "Loading " << song_num << "..." << std::endl;
      if (client.LoadAudio(song_num)) {
        std::cout << "Playing " << song_num << "..." << std::endl;
        client.Play();
      }
    } else if (command == "pause") {
      client.Pause();
      std::cout << "Playback paused." << std::endl;
    } else if (command == "resume") {
      client.Resume();
      std::cout << "Playback resumed." << std::endl;
    } else if (command == "stop") {
      client.Stop();
      std::cout << "Playback stopped." << std::endl;
    } else if (command == "peers") {
      std::vector<std::string> peers = client.GetPeerClientIPs();

      std::cout << "Clients connected to server:" << std::endl;
      if (peers.empty()) {
        std::cout << "No other clients connected to server." << std::endl;
      } else {
        for (size_t i = 0; i < peers.size(); i++) {
          std::cout << i + 1 << ". " << peers[i] << std::endl;
        }
      }
    } else if (command.substr(0, 5) == "join ") {
      std::string peer_address = command.substr(5);
      if (peer_network->ConnectToPeer(peer_address)) {
        std::cout << "Connected to peer: " << peer_address << std::endl;
      } else {
        std::cout << "Failed to connect to peer: " << peer_address << std::endl;
      }
    } else if (command.substr(0, 6) == "leave ") {
      std::string peer_address = command.substr(6);
      if (peer_network->DisconnectFromPeer(peer_address)) {
        std::cout << "Disconnected from peer: " << peer_address << std::endl;
      } else {
        std::cout << "Not connected to peer: " << peer_address << std::endl;
      }
    } else if (command == "connections") {
      std::vector<std::string> connected_peers =
          peer_network->GetConnectedPeers();

      std::cout << "Active peer connections:" << std::endl;
      if (connected_peers.empty()) {
        std::cout << "No active peer connections." << std::endl;
      } else {
        for (size_t i = 0; i < connected_peers.size(); i++) {
          std::cout << i + 1 << ". " << connected_peers[i] << std::endl;
        }
      }
      float average_offset = peer_network->GetAverageOffset();
      float max_rtt = peer_network->GetRTT();
      if (average_offset > 0) {
        std::cout << "Average offset: "
                  << average_offset << " ns"
                  << std::endl;
        std::cout << "Max RTT: "
                  << peer_network->GetRTT() << " ns"
                  << std::endl;
      } else {
        std::cout << "Average offset: NA" << std::endl;
        std::cout << "Max RTT: NA" << std::endl;
      }
    } else if (command == "gossip") {
      peer_network->BroadcastGossip();
      std::cout << "Gossiping peer connections to all peers." << std::endl;
    } else if (command == "help") {
      PrintUsage();
    } else if (command == "quit" || command == "exit") {
      LOG_INFO("Client shutting down");
      running = false;
    } else {
      std::cout << "Unknown command. Type 'help' for usage." << std::endl;
    }
  }

  return 0;
}
