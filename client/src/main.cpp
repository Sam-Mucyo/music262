#include <iostream>
#include <string>
#include "audioplayer.h"

void displayHelp() {
    std::cout << "\nCommands:" << std::endl;
    std::cout << "  list              - Show available songs" << std::endl;
    std::cout << "  play <song_number>- Request and play a song by number" << std::endl;
    std::cout << "  resume            - Resume playback" << std::endl;
    std::cout << "  pause             - Pause playback" << std::endl;
    std::cout << "  stop              - Stop playback" << std::endl;
    std::cout << "  seek <seconds>    - Seek to position" << std::endl;
    std::cout << "  position          - Show current position" << std::endl;
    std::cout << "  duration          - Show song duration" << std::endl;
    std::cout << "  help              - Show this help" << std::endl;
    std::cout << "  exit              - Exit the client" << std::endl;
}

int main(int argc, char* argv[]) {
    // Init player
    AudioPlayer player;

    // Load song
    player.loadSong("../../music/daydreamin.wav");

    // Play music
    player.play();
}