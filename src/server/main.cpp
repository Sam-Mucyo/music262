#include <iostream>
#include <string>

void displayHelp() {
  std::cout << "\nCommands:" << std::endl;
  std::cout << "  status            - Show server status (IP Address and port, "
               "active clients, etc.)"
            << std::endl;
}

int main(int argc, char *argv[]) {
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
    std::cout << "\n> ";
    std::getline(std::cin, command);

    if (command == "status") {
      std::cout << "TODO: " << std::endl;
    } else if (command == "help") {
      displayHelp();
    }

    else if (!command.empty()) {
      std::cout << "Unknown command: " << command
                << ". Type 'help' for available commands." << std::endl;
    }
  }

  // Won't ever get here, for now. TODO: add a handler for Ctrl-C (when server
  // is killed) to clean up resources, if any.
}
