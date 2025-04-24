#include <grpcpp/grpcpp.h>

#include <iostream>
#include <string>
#include <vector>

#include "include/client.h"

void PrintUsage() {
  std::cout << "Usage: \n"
            << "  playlist - Get list of available songs\n"
            << "  play <song_name> - Load and play a song\n"
            << "  pause - Pause the currently playing song\n"
            << "  resume - Resume the currently paused song\n"
            << "  stop - Stop the currently playing song\n"
            << "  peers - Get list of connected peers\n"
            << "  quit - Exit the client\n";
}

int main(int argc, char** argv) {
  // Default values
  std::string server_address = "localhost:50051";

  // Parse command line arguments
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--server" && i + 1 < argc) {
      server_address = argv[++i];
    }
  }

  // Create a channel to the server
  std::cout << "Connecting to server at " << server_address << std::endl;
  AudioClient client(
      grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));

  bool running = true;
  std::string command;

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
      std::string song_name = command.substr(5);

      std::cout << "Loading " << song_name << "..." << std::endl;
      if (client.LoadAudio(song_name)) {
        std::cout << "Playing " << song_name << "..." << std::endl;
        client.Play();
      }
    } else if (command == "pause") {
      client.Pause();
    } else if (command == "resume") {
      client.Resume();
    } else if (command == "stop") {
      client.Stop();
    } else if (command == "peers") {
      std::vector<std::string> peers = client.GetPeerClientIPs();

      std::cout << "Connected peers:" << std::endl;
      if (peers.empty()) {
        std::cout << "No other clients connected." << std::endl;
      } else {
        for (size_t i = 0; i < peers.size(); i++) {
          std::cout << i + 1 << ". " << peers[i] << std::endl;
        }
      }
    } else if (command == "help") {
      PrintUsage();
    } else if (command == "quit" || command == "exit") {
      running = false;
    } else {
      std::cout << "Unknown command. Type 'help' for usage." << std::endl;
    }
  }

  return 0;
}
