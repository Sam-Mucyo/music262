#include "../testlib/include/test_utils.h"

// Use the base test fixture for AudioPlayer tests
class AudioPlayerTest : public AudioPlayerTestBase {};

TEST_F(AudioPlayerTest, LoadValidWavFile) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    EXPECT_TRUE(player->load(testFilePath));
    
    // Clean up
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerTest, LoadInvalidWavFile) {
    // Create an invalid WAV file
    std::string invalidFilePath = createInvalidFile();
    
    // Try to load the invalid file
    EXPECT_FALSE(player->load(invalidFilePath));
    
    // Clean up
    std::remove(invalidFilePath.c_str());
}

TEST_F(AudioPlayerTest, PlayPauseResume) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    ASSERT_TRUE(player->load(testFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Pause the file
    player->pause();
    EXPECT_FALSE(player->isPlaying());
    
    // Resume the file
    player->resume();
    EXPECT_TRUE(player->isPlaying());
    
    // Clean up
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerTest, Stop) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    ASSERT_TRUE(player->load(testFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Stop the file
    player->stop();
    EXPECT_FALSE(player->isPlaying());
    EXPECT_EQ(player->get_position(), sizeof(WavHeader));
    
    // Clean up
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerTest, LoadNewFileWhilePlaying) {
    std::string testFilePath1 = createTestWavFile();
    std::string testFilePath2 = createTestWavFile();
    
    // Load the first file
    ASSERT_TRUE(player->load(testFilePath1));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Load a new file while playing
    ASSERT_TRUE(player->load(testFilePath2));
    EXPECT_FALSE(player->isPlaying());
    
    // Clean up
    std::remove(testFilePath1.c_str());
    std::remove(testFilePath2.c_str());
}

TEST_F(AudioPlayerTest, GetPosition) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    ASSERT_TRUE(player->load(testFilePath));
    
    // Initial position should be at the end of the header
    EXPECT_EQ(player->get_position(), sizeof(WavHeader));
    
    // Play the file
    player->play();
    
    // Position should advance as the file plays
    // Note: In a real test, we would need to simulate the RenderCallback
    // to actually advance the position. This is a simplified test.
    
    // Clean up
    std::remove(testFilePath.c_str());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 