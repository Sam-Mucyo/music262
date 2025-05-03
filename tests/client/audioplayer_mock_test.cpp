#include "../testlib/include/test_utils.h"

// Use the centralized CoreAudio mocks

// Use the base test fixture for AudioPlayer tests
class AudioPlayerTest : public AudioPlayerTestBase {};

TEST_F(AudioPlayerTest, LoadAndPlay) {
    // Create a test WAV file
    std::string testFilePath = createTestWavFile();
    
    // Load the file
    EXPECT_TRUE(player->load(testFilePath));
    
    // Play the file
    player->play();
    EXPECT_TRUE(player->isPlaying());
    
    // Clean up
    std::remove(testFilePath.c_str());
}

TEST_F(AudioPlayerTest, PauseAndResume) {
    // Create a test WAV file
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
    // Create a test WAV file
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
    // Create test WAV files
    std::string testFilePath1 = "test1.wav";
    std::string testFilePath2 = "test2.wav";
    
    // Create first test file
    std::ofstream file1(testFilePath1, std::ios::binary);
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
    file1.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    std::vector<char> audioData1(1024, 0);
    file1.write(audioData1.data(), audioData1.size());
    file1.close();
    
    // Create second test file
    std::ofstream file2(testFilePath2, std::ios::binary);
    file2.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    std::vector<char> audioData2(1024, 0);
    file2.write(audioData2.data(), audioData2.size());
    file2.close();
    
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

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 