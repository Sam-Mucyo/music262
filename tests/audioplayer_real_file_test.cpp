#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "client/include/audioplayer.h"
#include <memory>
#include <cstring>
#include <fstream>
#include <vector>
#include <filesystem>

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

class AudioPlayerRealFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        player = std::make_unique<AudioPlayer>();
        
        // Use the absolute path to the daydreamin.wav file
        wavFilePath = "/Users/mirayu/CS 262 Problems/music262/sample_music/daydreamin.wav";
        
        // Verify the file exists
        ASSERT_TRUE(std::filesystem::exists(wavFilePath)) 
            << "Test file not found: " << wavFilePath;
    }
    
    void TearDown() override {
        player.reset();
    }
    
    std::unique_ptr<AudioPlayer> player;
    std::string wavFilePath;
};

TEST_F(AudioPlayerRealFileTest, LoadRealWavFile) {
    // Load the real WAV file
    EXPECT_TRUE(player->load(wavFilePath));
}

TEST_F(AudioPlayerRealFileTest, PlayRealWavFile) {
    // Load the real WAV file
    ASSERT_TRUE(player->load(wavFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
}

TEST_F(AudioPlayerRealFileTest, PauseAndResumeRealWavFile) {
    // Load the real WAV file
    ASSERT_TRUE(player->load(wavFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Pause the file
    player->pause();
    EXPECT_FALSE(player->isPlaying());
    
    // Resume the file
    player->resume();
    EXPECT_TRUE(player->isPlaying());
}

TEST_F(AudioPlayerRealFileTest, StopRealWavFile) {
    // Load the real WAV file
    ASSERT_TRUE(player->load(wavFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Stop the file
    player->stop();
    EXPECT_FALSE(player->isPlaying());
    EXPECT_EQ(player->get_position(), sizeof(WavHeader));
}

TEST_F(AudioPlayerRealFileTest, GetPositionRealWavFile) {
    // Load the real WAV file
    ASSERT_TRUE(player->load(wavFilePath));
    
    // Initial position should be at the end of the header
    EXPECT_EQ(player->get_position(), sizeof(WavHeader));
    
    // Play the file
    player->play();
    
    // Position should advance as the file plays
    // Note: In a real test, we would need to simulate the RenderCallback
    // to actually advance the position. This is a simplified test.
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 