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
  LOG_CRITICAL("TODO: Add handler to the above commands to play music");
  // TODO: Add handler for the display commands above

  // Init player
  AudioPlayer player;
  std::string command;
  
  while (true) {
    std::cout << "\nEnter command: ";
    std::cin >> command;

    if (command == "play") {
        std::string filePath;
        std::cout << "Enter the absolute path to the WAV file: ";
        std::cin >> filePath;

        player.load(filePath);
        player.play();
    }
    else if (command == "pause") {
        player.pause();
    }
    else if (command == "resume") {
        player.resume();
    }
    else if (command == "stop") {
        player.stop();
    }
    else if (command == "resume") {
        player.play();
    }
    else if (command == "position") {
        std::cout << "Current position: " << player.get_position() << std::endl;
    }
    else if (command == "duration") {
        std::cout << "Duration: " << player.get_position() << std::endl;
    }
    else if (command == "help") {
        displayHelp();
    }
    else if (command == "exit") {
        break;
    }
    else {
        std::cout << "Unknown command. Type 'help' for a list of commands." << std::endl;
    }
  }
  return 0;
}
