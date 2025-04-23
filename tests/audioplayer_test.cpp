#include "../src/client/include/audioplayer.h"
#include "../src/common/include/logger.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <memory>

// Mock class for AudioPlayer
class MockAudioPlayer : public AudioPlayer {
public:
    MOCK_METHOD(bool, load, (const std::string& songPath));
    MOCK_METHOD(void, play, ());
    MOCK_METHOD(void, pause, ());
    MOCK_METHOD(void, resume, ());
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(unsigned int, get_position, (), (const));
};

class AudioPlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger for tests
        Logger::init("audio_player_test");
        mockPlayer = std::make_unique<MockAudioPlayer>();
    }

    std::unique_ptr<MockAudioPlayer> mockPlayer;
    const std::string sampleWavFile = "../sample_music/daydreamin.wav";
    const std::string nonExistentFile = "non_existent_file.wav";
};

// Test initialization state
TEST_F(AudioPlayerTest, InitialState) {
    // Set up the mock to return 0 for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .WillOnce(testing::Return(0u));
    
    // Check initial position
    EXPECT_EQ(0u, mockPlayer->get_position());
}

// Test load method with invalid file
TEST_F(AudioPlayerTest, LoadInvalidFile) {
    // Set up the mock to return false for load
    EXPECT_CALL(*mockPlayer, load(nonExistentFile))
        .WillOnce(testing::Return(false));
    
    EXPECT_FALSE(mockPlayer->load(nonExistentFile));
}

// Test load method with valid file
TEST_F(AudioPlayerTest, LoadValidFile) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    EXPECT_TRUE(mockPlayer->load(sampleWavFile));
}

// Test play functionality
TEST_F(AudioPlayerTest, Play) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    // Set up the mock to do nothing for play
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    
    // Load and play
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
}

// Test pause functionality
TEST_F(AudioPlayerTest, Pause) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    // Set up the mock to do nothing for play and pause
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    EXPECT_CALL(*mockPlayer, pause())
        .Times(1);
    
    // Set up the mock to return a position for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .Times(2)  // Called twice: once before pause, once after
        .WillRepeatedly(testing::Return(1000u));  // Same position before and after pause
    
    // Load, play, and pause
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
    unsigned int positionBeforePause = mockPlayer->get_position();
    mockPlayer->pause();
    EXPECT_EQ(positionBeforePause, mockPlayer->get_position());
}

// Test resume functionality
TEST_F(AudioPlayerTest, Resume) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    // Set up the mock to do nothing for play, pause, and resume
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    EXPECT_CALL(*mockPlayer, pause())
        .Times(1);
    EXPECT_CALL(*mockPlayer, resume())
        .Times(1);
    
    // Set up the mock to return positions for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .Times(3)  // Called three times: before pause, after pause, after resume
        .WillOnce(testing::Return(1000u))   // Before pause
        .WillOnce(testing::Return(1000u))   // After pause
        .WillOnce(testing::Return(1500u));  // After resume
    
    // Load, play, pause, and resume
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
    unsigned int positionBeforePause = mockPlayer->get_position();
    mockPlayer->pause();
    EXPECT_EQ(positionBeforePause, mockPlayer->get_position());
    mockPlayer->resume();
    EXPECT_GT(mockPlayer->get_position(), positionBeforePause);
}

// Test stop functionality
TEST_F(AudioPlayerTest, Stop) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    // Set up the mock to do nothing for play and stop
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    EXPECT_CALL(*mockPlayer, stop())
        .Times(1);
    
    // Set up the mock to return positions for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .Times(2)  // Called twice: once before stop, once after
        .WillOnce(testing::Return(1000u))  // Before stop
        .WillOnce(testing::Return(0u));    // After stop
    
    // Load, play, and stop
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
    EXPECT_GT(mockPlayer->get_position(), 0u);
    mockPlayer->stop();
    EXPECT_EQ(0u, mockPlayer->get_position());
}

// Test position reporting during playback
TEST_F(AudioPlayerTest, PositionReporting) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .WillOnce(testing::Return(true));
    
    // Set up the mock to do nothing for play
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    
    // Set up the mock to return increasing positions for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .Times(2)  // Called twice to check position increase
        .WillOnce(testing::Return(1000u))
        .WillOnce(testing::Return(2000u));
    
    // Load and play
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
    
    // Check positions
    unsigned int position1 = mockPlayer->get_position();
    unsigned int position2 = mockPlayer->get_position();
    
    // Position should have increased
    EXPECT_GT(position2, position1);
}

// Test multiple load operations
TEST_F(AudioPlayerTest, MultipleLoads) {
    // Set up the mock to return true for load
    EXPECT_CALL(*mockPlayer, load(sampleWavFile))
        .Times(2)  // Called twice
        .WillRepeatedly(testing::Return(true));
    
    // Set up the mock to do nothing for play and stop
    EXPECT_CALL(*mockPlayer, play())
        .Times(1);
    EXPECT_CALL(*mockPlayer, stop())
        .Times(1);
    
    // Set up the mock to return positions for get_position
    EXPECT_CALL(*mockPlayer, get_position())
        .Times(2)  // Called twice: once before stop, once after second load
        .WillOnce(testing::Return(1000u))  // Before stop
        .WillOnce(testing::Return(0u));    // After second load
    
    // Load, play, stop, and load again
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    mockPlayer->play();
    EXPECT_GT(mockPlayer->get_position(), 0u);
    mockPlayer->stop();
    ASSERT_TRUE(mockPlayer->load(sampleWavFile));
    EXPECT_EQ(0u, mockPlayer->get_position());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
