#include "../src/client/include/audioplayer.h"
#include "../src/common/include/logger.h"
#include <gtest/gtest.h>
#include <string>

class AudioPlayerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize logger for tests
    Logger::init("audio_player_test");
  }

  AudioPlayer player;
};

// Test initialization state
TEST_F(AudioPlayerTest, InitialState) {
  // We can only test the public interface, so we'll check get_position()
  EXPECT_EQ(0u, player.get_position());
}

// Test load method with invalid file
TEST_F(AudioPlayerTest, LoadInvalidFile) {
  EXPECT_FALSE(player.load("non_existent_file.wav"));
}

// Test position reporting
TEST_F(AudioPlayerTest, PositionReporting) {
  EXPECT_EQ(0u, player.get_position());
}

// TODO: more tests: actual playing functionality
TEST_F(AudioPlayerTest, DISABLED_Playing) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
