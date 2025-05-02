#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "client/include/audioplayer.h"
#include <memory>
#include <cstring>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

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

// Global variable to store the AudioPlayer instance for the callback
AudioPlayer* g_player = nullptr;

// Mock RenderCallback function
OSStatus MockRenderCallback(void* inRefCon,
                           AudioUnitRenderActionFlags* ioActionFlags,
                           const AudioTimeStamp* inTimeStamp,
                           UInt32 inBusNumber,
                           UInt32 inNumberFrames,
                           AudioBufferList* ioData) {
    // Use the global player instance
    AudioPlayer* player = g_player;
    if (!player) {
        return -1; // Error
    }
    
    // Get the current position
    unsigned int position = player->get_position();
    unsigned int dataPosition = position - sizeof(WavHeader);  // Adjust position to be relative to audio data
    
    // Get the audio data
    const std::vector<char>& audioData = player->get_audio_data();
    
    // Calculate bytes per frame
    int channels = player->get_header().numChannels;
    int bytesPerSample = player->get_header().bitsPerSample / 8;
    int bytesPerFrame = bytesPerSample * channels;
    
    // Calculate how many frames we can provide
    unsigned int bytesAvailable = audioData.size() - dataPosition;
    unsigned int framesAvailable = bytesAvailable / bytesPerFrame;
    
    // Calculate how many frames to render
    UInt32 framesToRender = std::min(inNumberFrames, framesAvailable);
    
    // Fill the output buffer with audio data
    float* outBuffer = reinterpret_cast<float*>(ioData->mBuffers[0].mData);
    
    // Read audio to buffer
    for (UInt32 i = 0; i < framesToRender; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            int idx = dataPosition + (i * bytesPerFrame) + (ch * bytesPerSample);
            int16_t sample = *reinterpret_cast<const int16_t*>(&audioData[idx]);
            outBuffer[i * channels + ch] = sample / 32768.0f;
        }
    }
    
    // Fill the rest with silence if done
    for (UInt32 i = framesToRender; i < inNumberFrames; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            outBuffer[i * channels + ch] = 0.0f;
        }
    }
    
    // Update the position
    unsigned int currentPos = player->get_position();
    player->set_position(currentPos + framesToRender * bytesPerFrame);
    
    // If we've reached the end of the audio data, stop playing
    if (framesToRender < inNumberFrames) {
        player->set_playing(false);
    }
    
    return noErr;
}

// Define the RenderCallback function to use our mock
#define RenderCallback MockRenderCallback

class AudioPlayerCallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        player = std::make_unique<AudioPlayer>();
        g_player = player.get();
    }
    
    void TearDown() override {
        g_player = nullptr;
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