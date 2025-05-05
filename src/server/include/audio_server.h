#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Core server logic for the music streaming server
 *
 * This class contains the business logic for the music streaming server,
 * separated from the networking code to allow for easier testing.
 */
class AudioServer {
 public:
  /**
   * @brief Construct a new Audio Server object
   *
   * @param audio_dir Directory containing audio files
   */
  AudioServer(const std::string& audio_dir);

  /**
   * @brief Get the list of available audio files
   *
   * @return std::vector<std::string> List of audio file names
   */
  std::vector<std::string> GetPlaylist() const;

  /**
   * @brief Load an audio file by its index in the playlist
   *
   * @param song_num Index of the song in the playlist (1-based)
   * @return std::string Path to the audio file, empty if not found
   */
  std::string GetAudioFilePath(int song_num) const;

  /**
   * @brief Register a client with the server
   *
   * @param client_id Unique identifier for the client
   * @return int Client ID assigned by the server
   */
  int RegisterClient(const std::string& client_id);

  /**
   * @brief Get the list of connected clients
   *
   * @param exclude_client_id Client ID to exclude from the list
   * @return std::vector<std::string> List of client IDs
   */
  std::vector<std::string> GetConnectedClients(
      const std::string& exclude_client_id = "") const;

  /**
   * @brief Extract the clean IP:port from a peer string
   *
   * @param peer Peer string in format "ipv4:127.0.0.1:12345" or
   * "ipv6:[::1]:12345"
   * @return std::string Clean IP:port
   */
  static std::string ExtractIPFromPeer(const std::string& peer);

  /**
   * @brief Print server status information
   *
   * @param local_ip Local IP address of the server
   * @param port Port the server is listening on
   */
  void PrintStatus(const std::string& local_ip, int port) const;

 private:
  std::string audio_directory_;
  std::vector<std::string> playlist_;

  // Client tracking
  std::map<int, std::string> connected_clients_;
  int next_client_id_;
  mutable std::mutex clients_mutex_;
};
