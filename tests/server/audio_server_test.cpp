#include "server/include/audio_server.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "../testlib/include/test_utils.h"

namespace fs = std::filesystem;

// Create a test fixture for AudioServer tests
class AudioServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary directory for test audio files
    test_dir_ = fs::temp_directory_path() / "music262_test";
    fs::create_directories(test_dir_);

    // Create some test WAV files
    createTestWavFile(test_dir_ / "test1.wav");
    createTestWavFile(test_dir_ / "test2.wav");

    // Create the AudioServer with the test directory
    server_ = std::make_unique<AudioServer>(test_dir_.string());
  }

  void TearDown() override {
    // Clean up test files
    fs::remove_all(test_dir_);
  }

  // Helper to create a simple test WAV file
  void createTestWavFile(const fs::path& path) {
    std::ofstream file(path, std::ios::binary);

    // Create a minimal WAV header
    WavHeader header;
    memset(&header, 0, sizeof(header));

    // Set RIFF header
    memcpy(header.riff, "RIFF", 4);
    header.fileSize = sizeof(header) + 1024;  // Header + 1KB of data
    memcpy(header.wave, "WAVE", 4);

    // Set format chunk
    memcpy(header.fmt, "fmt ", 4);
    header.fmtSize = 16;
    header.audioFormat = 1;  // PCM
    header.numChannels = 2;  // Stereo
    header.sampleRate = 44100;
    header.byteRate =
        44100 * 2 * 2;      // SampleRate * NumChannels * BytesPerSample
    header.blockAlign = 4;  // NumChannels * BytesPerSample
    header.bitsPerSample = 16;

    // Set data chunk
    memcpy(header.data, "data", 4);
    header.dataSize = 1024;  // 1KB of audio data

    // Write header
    file.write(reinterpret_cast<char*>(&header), sizeof(header));

    // Write some dummy audio data (1KB of zeros)
    char buffer[1024] = {0};
    file.write(buffer, sizeof(buffer));

    file.close();
  }

  fs::path test_dir_;
  std::unique_ptr<AudioServer> server_;
};

// Test getting the playlist
TEST_F(AudioServerTest, GetPlaylist) {
  auto playlist = server_->GetPlaylist();

  // Check that we have the expected number of songs
  EXPECT_EQ(playlist.size(), 2);

  // Check that our test files are in the playlist
  EXPECT_THAT(playlist,
              ::testing::UnorderedElementsAre("test1.wav", "test2.wav"));
}

// Test getting a valid audio file path
TEST_F(AudioServerTest, GetValidAudioFilePath) {
  std::string file_path = server_->GetAudioFilePath(1);

  // Check that the path is not empty
  EXPECT_FALSE(file_path.empty());

  // Check that the file exists
  EXPECT_TRUE(fs::exists(file_path));

  // Check that it's one of our test files
  EXPECT_TRUE(file_path.find("test1.wav") != std::string::npos ||
              file_path.find("test2.wav") != std::string::npos);
}

// Test getting an invalid audio file path
TEST_F(AudioServerTest, GetInvalidAudioFilePath) {
  // Try to get a file that doesn't exist (index out of bounds)
  std::string file_path = server_->GetAudioFilePath(100);

  // Should return an empty string
  EXPECT_TRUE(file_path.empty());

  // Try with a negative index
  file_path = server_->GetAudioFilePath(-1);

  // Should also return an empty string
  EXPECT_TRUE(file_path.empty());
}

// Test client registration and retrieval
TEST_F(AudioServerTest, RegisterAndGetClients) {
  // Register some clients
  int id1 = server_->RegisterClient("ipv4:192.168.1.1:12345");
  int id2 = server_->RegisterClient("ipv6:[::1]:54321");

  // Check that the IDs are different
  EXPECT_NE(id1, id2);

  // Get all clients
  auto clients = server_->GetConnectedClients();

  // Should have 2 clients
  EXPECT_EQ(clients.size(), 2);

  // Check that our clients are in the list
  EXPECT_THAT(clients, ::testing::UnorderedElementsAre("192.168.1.1:12345",
                                                       "[::1]:54321"));

  // Get clients excluding one
  clients = server_->GetConnectedClients("ipv4:192.168.1.1:12345");

  // Should have 1 client
  EXPECT_EQ(clients.size(), 1);
  EXPECT_EQ(clients[0], "[::1]:54321");
}

// Test extracting IP from peer string
TEST_F(AudioServerTest, ExtractIPFromPeer) {
  // Test IPv4 address
  std::string ip = AudioServer::ExtractIPFromPeer("ipv4:192.168.1.1:12345");
  EXPECT_EQ(ip, "192.168.1.1:12345");

  // Test IPv6 address
  ip = AudioServer::ExtractIPFromPeer("ipv6:[::1]:54321");
  EXPECT_EQ(ip, "[::1]:54321");

  // Test with no prefix
  ip = AudioServer::ExtractIPFromPeer("no_prefix");
  EXPECT_EQ(ip, "no_prefix");
}

// Test registering the same client twice
TEST_F(AudioServerTest, RegisterSameClientTwice) {
  // Register a client
  int id1 = server_->RegisterClient("ipv4:192.168.1.1:12345");

  // Register the same client again
  int id2 = server_->RegisterClient("ipv4:192.168.1.1:12345");

  // Should get the same ID
  EXPECT_EQ(id1, id2);

  // Get all clients
  auto clients = server_->GetConnectedClients();

  // Should have 1 client (no duplicates)
  EXPECT_EQ(clients.size(), 1);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
