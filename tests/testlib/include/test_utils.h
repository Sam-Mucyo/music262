#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../src/client/include/audioplayer.h"
#include "coreaudio_mocks.h"
#include <memory>
#include <fstream>
#include <vector>
#include <string>

// Base test fixture for AudioPlayer tests
class AudioPlayerTestBase : public ::testing::Test {
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
    std::string createTestWavFile(const std::string& filename = "test.wav", int dataSize = 1024) {
        std::ofstream file(filename, std::ios::binary);
        
        WavHeader header;
        header.riff[0] = 'R';
        header.riff[1] = 'I';
        header.riff[2] = 'F';
        header.riff[3] = 'F';
        header.fileSize = dataSize + sizeof(WavHeader) - 8;
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
        header.dataSize = dataSize;
        
        // Write header
        file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        
        // Write some dummy audio data
        std::vector<char> audioData(dataSize, 0);
        file.write(audioData.data(), audioData.size());
        
        file.close();
        return filename;
    }
    
    // Helper function to create an invalid file
    std::string createInvalidFile(const std::string& filename = "invalid.wav") {
        std::ofstream file(filename, std::ios::binary);
        file << "This is not a valid WAV file";
        file.close();
        return filename;
    }
    
    std::unique_ptr<AudioPlayer> player;
};
