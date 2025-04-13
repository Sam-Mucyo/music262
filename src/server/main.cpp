#include <iostream>
#include <string>
#include "../common/include/logger.h"

void displayHelp() {
  std::cout << "\nCommands:" << std::endl;
  std::cout << "  status            - Show server status (IP Address and port, "
               "active clients, etc.)"
            << std::endl;
}

int main(int argc, char *argv[]) {
  // Initialize the logger
  Logger::init("music_server");
  
  int port = 8080; // TODO: This should not be hard coded. Maybe get it from
                   // cmdline or config files

  std::cout << "Music Streaming Server - Starting up..." << std::endl;
  std::cout << "Configured to use port: " << port << std::endl;

  std::cout << "Welcome to the Music262 Server!" << std::endl;
  std::cout << "Type 'help' to see available commands." << std::endl;

  // Main command loop
  std::string command;
  bool running = true;

  while (running) {
    std::cout << "\n> "; // Keep this as std::cout for the prompt
    std::getline(std::cin, command);

    if (command == "status") {
      LOG_CRITICAL("TODO: Implement status command");
    } else if (command == "help") {
      displayHelp();
    }
    else if (command == "exit") {
      std::cout << "Shutting down server..." << std::endl;
      running = false;
    }
    else if (!command.empty()) {
      LOG_WARN("Unknown command: {}. Type 'help' for available commands.", command);
    }
  }
  
  std::cout << "Server shutdown complete." << std::endl;
  return 0;

  // Won't ever get here, for now. TODO: add a handler for Ctrl-C (when server
  // is killed) to clean up resources, if any.
}
