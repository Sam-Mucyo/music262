#include <QApplication>
#include <cstdlib>
#include <string>

#include "include/mainwindow.h"
#include "logger.h"

int main(int argc, char** argv) {
  // Initialize the logger
  Logger::init("music_client_gui");

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

  // Create Qt Application
  QApplication app(argc, argv);

  // Set application metadata
  app.setApplicationName("Music Client");
  app.setApplicationVersion("1.0");
  app.setOrganizationName("CS262");

  // Create the main window
  LOG_INFO("Starting Music Client GUI");
  MainWindow mainWindow(server_address, p2p_port);
  mainWindow.show();

  // Run the application
  return app.exec();
}