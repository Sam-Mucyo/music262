#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "client/include/audioplayer.h"
#include <memory>
#include <cstring>
#include <fstream>
#include <vector>

// Mock CoreAudio APIs
extern "C" {
    // Mock AudioComponentFindNext
    AudioComponent MockAudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription* inDesc) {
        return (AudioComponent)0x1234; // Return a dummy component
    }
    
    // Mock AudioComponentInstanceNew
    OSStatus MockAudioComponentInstanceNew(AudioComponent inComponent, AudioUnit* outInstance) {
        *outInstance = (AudioUnit)0x5678; // Return a dummy audio unit
        return noErr;
    }
    
    // Mock AudioUnitSetProperty
    OSStatus MockAudioUnitSetProperty(AudioUnit inUnit, 
                                      AudioUnitPropertyID inID, 
                                      AudioUnitScope inScope, 
                                      AudioUnitElement inElement, 
                                      const void* inData, 
                                      UInt32 inDataSize) {
        return noErr;
    }
    
    // Mock AudioUnitInitialize
    OSStatus MockAudioUnitInitialize(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioOutputUnitStart
    OSStatus MockAudioOutputUnitStart(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioOutputUnitStop
    OSStatus MockAudioOutputUnitStop(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioUnitUninitialize
    OSStatus MockAudioUnitUninitialize(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioComponentInstanceDispose
    OSStatus MockAudioComponentInstanceDispose(AudioUnit inUnit) {
        return noErr;
    }
}

// Replace the real CoreAudio functions with our mocks
#define AudioComponentFindNext MockAudioComponentFindNext
#define AudioComponentInstanceNew MockAudioComponentInstanceNew
#define AudioUnitSetProperty MockAudioUnitSetProperty
#define AudioUnitInitialize MockAudioUnitInitialize
#define AudioOutputUnitStart MockAudioOutputUnitStart
#define AudioOutputUnitStop MockAudioOutputUnitStop
#define AudioUnitUninitialize MockAudioUnitUninitialize
#define AudioComponentInstanceDispose MockAudioComponentInstanceDispose

class AudioPlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        player = std::make_unique<AudioPlayer>();
    }
    
    void TearDown() override {
        player.reset();
    }
    
    // Helper function to create a test WAV file
    std::string createTestWavFile() {
        std::string testFilePath = "test.wav";
        std::ofstream file(testFilePath, std::ios::binary);
        
        WavHeader header;
        header.riff[0] = 'R';
        header.riff[1] = 'I';
        header.riff[2] = 'F';
        header.riff[3] = 'F';
        header.fileSize = 1024 + sizeof(WavHeader) - 8;
        header.wave[0] = 'W';
        header.wave[1] = 'A';
        header.wave[2] = 'V';
        header.wave[3] = 'E';
        header.fmt[0] = 'f';
        header.fmt[1] = 'm';
        header.fmt[2] = 't';
        header.fmt[3] = ' ';
        header.fmtSize = 16;
        header.audioFormat = 1;
        header.numChannels = 2;
        header.sampleRate = 44100;
        header.byteRate = 44100 * 2 * 2;
        header.blockAlign = 2 * 2;
        header.bitsPerSample = 16;
        header.data[0] = 'd';
        header.data[1] = 'a';
        header.data[2] = 't';
        header.data[3] = 'a';
        header.dataSize = 1024;
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        
        // Write some dummy audio data
        std::vector<char> audioData(1024, 0);
        file.write(audioData.data(), audioData.size());
        
        file.close();
        return testFilePath;
    }
    
    std::unique_ptr<AudioPlayer> player;
};

TEST_F(AudioPlayerTest, LoadValidWavFile) {
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    EXPECT_TRUE(player->load(testFilePath));
    
    // Clean up
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerTest, LoadInvalidWavFile) {
    // Create an invalid WAV file
    std::string invalidFilePath = "invalid.wav";
    std::ofstream file(invalidFilePath, std::ios::binary);
    file << "This is not a valid WAV file";
    file.close();
    
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