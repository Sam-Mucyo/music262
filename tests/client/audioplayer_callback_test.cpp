#include "../testlib/include/test_utils.h"
#include <thread>
#include <chrono>

// Use the base test fixture for AudioPlayer tests
class AudioPlayerCallbackTest : public AudioPlayerTestBase {};


TEST_F(AudioPlayerCallbackTest, SimulatePlayback) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    ASSERT_TRUE(player->load(testFilePath));
    
    // Initial position should be at the end of the header
    EXPECT_EQ(player->get_position(), sizeof(WavHeader));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Simulate the RenderCallback being called
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp timestamp = {};
    UInt32 busNumber = 0;
    UInt32 numberFrames = 100;
    
    AudioBuffer buffer = {};
    buffer.mNumberChannels = 2;
    buffer.mDataByteSize = numberFrames * 2 * sizeof(float);
    buffer.mData = new float[numberFrames * 2];
    
    AudioBufferList bufferList = {};
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0] = buffer;
    
    // Call the callback
    OSStatus status = MockRenderCallback(player.get(), &flags, &timestamp, busNumber, numberFrames, &bufferList);
    EXPECT_EQ(status, noErr);
    
    // Position should have advanced
    EXPECT_GT(player->get_position(), sizeof(WavHeader));
    
    // Clean up
    delete[] static_cast<float*>(buffer.mData);
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerCallbackTest, SimulatePlaybackUntilEnd) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    ASSERT_TRUE(player->load(testFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Simulate the RenderCallback being called until the end of the file
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp timestamp = {};
    UInt32 busNumber = 0;
    UInt32 numberFrames = 1000; // Large enough to reach the end
    
    AudioBuffer buffer = {};
    buffer.mNumberChannels = 2;
    buffer.mDataByteSize = numberFrames * 2 * sizeof(float);
    buffer.mData = new float[numberFrames * 2];
    
    AudioBufferList bufferList = {};
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0] = buffer;
    
    // Call the callback
    OSStatus status = MockRenderCallback(player.get(), &flags, &timestamp, busNumber, numberFrames, &bufferList);
    EXPECT_EQ(status, noErr);
    
    // Position should be at the end of the file
    EXPECT_EQ(player->get_position(), sizeof(WavHeader) + 1024);
    
    // Playing should be false
    EXPECT_FALSE(player->isPlaying());
    
    // Clean up
    delete[] static_cast<float*>(buffer.mData);
    std::remove(testFilePath.c_str());
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 