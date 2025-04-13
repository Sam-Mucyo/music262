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
//   // Initialize the logger
//   Logger::init("music_client");
//   std::cout << "Music Client started" << std::endl;
  
//   displayHelp();
//   LOG_CRITICAL("TODO: Add handler to the above commands to play music");
//   // TODO: Add handler for the display commands above
  
//   return 0;
    // Init player
    AudioPlayer player;

    // Load song from disk
    // player.loadSong("../../sample_music/daydreamin.wav");
    player.load("../sample_music/daydreamin.wav");
    if (!player.load("../sample_music/daydreamin.wav")) {
        std::cerr << "Failed to load song." << std::endl;
        return 1;
    }
    else {
        std::cout << "Loaded song successfully." << std::endl;
    }

    // Play music
    player.play();
}