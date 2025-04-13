#include "include/audioplayer.h"
#include "../common/include/logger.h"
#include <iostream>

void displayHelp() {
  std::cout << "\nCommands:" << std::endl;
  std::cout << "  list              - Show available songs" << std::endl;
  std::cout << "  play <song_number>- Request and play a song by number"
            << std::endl;
  std::cout << "  resume            - Resume playback" << std::endl;
  std::cout << "  pause             - Pause playback" << std::endl;
  std::cout << "  stop              - Stop playback" << std::endl;
  std::cout << "  position          - Show current position" << std::endl;
  std::cout << "  duration          - Show song duration" << std::endl;
  std::cout << "  help              - Show this help" << std::endl;
  std::cout << "  exit              - Exit the client" << std::endl;
}

int main(int argc, char *argv[]) {
  // Initialize the logger
  Logger::init("music_client");
  std::cout << "Music Client started" << std::endl;
  
  displayHelp();
    AudioPlayer player;
    std::string command;

    while (true) {
        std::cout << "\nEnter command: ";
        std::cin >> command;

        if (command == "play") {
            std::string filePath;
            std::cout << "Enter file path: ";
            std::cin >> filePath;

            if (!player.load(filePath)) {
                std::cerr << "Failed to load audio file.\n";
                continue;
            }

            player.play();
            std::cout << "Playing audio...\n";
        } else if (command == "pause") {
            player.pause();
            std::cout << "Playback paused.\n";
        } else if (command == "stop") {
            player.stop();
            std::cout << "Playback stopped.\n";
        } else if (command == "position") {
            std::cout << "Current position: " << player.getPosition() << "s\n";
        } else if (command == "exit") {
            std::cout << "Exiting...\n";
            break;
        } else {
            displayHelp();
        }
    }

    return 0;
}