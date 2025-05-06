// #include "client/include/client.h"
// #include "client/include/audio_service_interface.h"
// #include "client/include/peer_service_interface.h"
// #include "../testlib/include/test_utils.h"

// #include <gtest/gtest.h>
// #include <gmock/gmock.h>

// #include <memory>
// #include <string>
// #include <vector>

// // Mock implementation of the AudioServiceInterface for testing
// class MockAudioService : public music262::AudioServiceInterface {
// public:
//     MOCK_METHOD(std::vector<std::string>, GetPlaylist, (), (override));
//     MOCK_METHOD(bool, LoadAudio, (int song_num, music262::AudioChunkCallback callback), (override));
//     MOCK_METHOD(std::vector<std::string>, GetPeerClientIPs, (), (override));
//     MOCK_METHOD(bool, IsServerConnected, (), (override));
// };

// // Test fixture for AudioClient tests
// class AudioClientTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         // Create mock audio service
//         mock_audio_service = std::make_unique<::testing::NiceMock<MockAudioService>>();
//         mock_audio_service_ptr = mock_audio_service.get();
        
//         // Create client with mock service
//         client = std::make_unique<AudioClient>(std::move(mock_audio_service));
//     }

//     void TearDown() override {
//         client.reset();
//     }

//     // Helper function to set up for audio loading tests
//     void SetupLoadAudioTest(bool service_success) {
//         // Setup mock audio service to call the callback with test data when LoadAudio is called
//         ON_CALL(*mock_audio_service_ptr, LoadAudio(testing::_, testing::_))
//             .WillByDefault([service_success](int song_num, int channel_idx, music262::AudioChunkCallback callback) {
//                 if (service_success) {
//                     // Call the callback with some test data
//                     std::vector<char> test_data(1024, 'A');
//                     callback(test_data);
//                 }
//                 return service_success;
//             });
//     }

//     // Mocks
//     std::unique_ptr<MockAudioService> mock_audio_service;
//     MockAudioService* mock_audio_service_ptr;
    
//     // Class under test
//     std::unique_ptr<AudioClient> client;
// };

// // Test GetPlaylist functionality
// TEST_F(AudioClientTest, GetPlaylist) {
//     std::vector<std::string> expected_playlist = {"song1.wav", "song2.wav"};
    
//     // Setup expectation
//     EXPECT_CALL(*mock_audio_service_ptr, GetPlaylist())
//         .WillOnce(testing::Return(expected_playlist));
    
//     // Call the method under test
//     auto playlist = client->GetPlaylist();
    
//     // Verify results
//     EXPECT_EQ(playlist, expected_playlist);
// }

// // Test IsServerConnected functionality
// TEST_F(AudioClientTest, IsServerConnected) {
//     // Setup expectations for connected and disconnected states
//     EXPECT_CALL(*mock_audio_service_ptr, IsServerConnected())
//         .WillOnce(testing::Return(true))
//         .WillOnce(testing::Return(false));
    
//     // Call the method under test and verify results
//     EXPECT_TRUE(client->IsServerConnected());
//     EXPECT_FALSE(client->IsServerConnected());
// }

// // Test GetPeerClientIPs functionality
// TEST_F(AudioClientTest, GetPeerClientIPs) {
//     std::vector<std::string> expected_ips = {"192.168.1.1:50052", "192.168.1.2:50052"};
    
//     // Setup expectation
//     EXPECT_CALL(*mock_audio_service_ptr, GetPeerClientIPs())
//         .WillOnce(testing::Return(expected_ips));
    
//     // Call the method under test
//     auto ips = client->GetPeerClientIPs();
    
//     // Verify results
//     EXPECT_EQ(ips, expected_ips);
// }

// // Test that AudioData gets populated when the service returns success
// TEST_F(AudioClientTest, AudioDataPopulatedWhenServiceSucceeds) {
//     int test_song_num = 1;
//     int test_channel_idx = -1;
    
//     // Setup mock to return success and provide data
//     SetupLoadAudioTest(true);
    
//     // Call the method under test
//     client->LoadAudio(test_song_num, test_channel_idx);
    
//     // Verify that audio data was populated, even though AudioPlayer::loadFromMemory will fail
//     EXPECT_FALSE(client->GetAudioData().empty());
// }

// // Test LoadAudio failure case
// TEST_F(AudioClientTest, LoadAudioFailure) {
//     int test_song_num = 1;
//     int test_channel_idx = -1;
    
//     // Setup mock to return failure
//     SetupLoadAudioTest(false);
    
//     // Call the method under test
//     bool result = client->LoadAudio(test_song_num, test_channel_idx);
//     // Verify results
//     EXPECT_FALSE(result);
//     EXPECT_TRUE(client->GetAudioData().empty());
// }

// // Test peer sync flag functionality
// TEST_F(AudioClientTest, PeerSyncFlagControl) {
//     // Default should be disabled
//     EXPECT_FALSE(client->IsPeerSyncEnabled());
    
//     // Enable peer sync
//     client->EnablePeerSync(true);
//     EXPECT_TRUE(client->IsPeerSyncEnabled());
    
//     // Disable peer sync
//     client->EnablePeerSync(false);
//     EXPECT_FALSE(client->IsPeerSyncEnabled());
// }

// // Test command from broadcast flag
// TEST_F(AudioClientTest, CommandFromBroadcastFlag) {
//     // Default should be false
//     EXPECT_FALSE(client->IsCommandFromBroadcast());
    
//     // Set the command from broadcast flag
//     client->SetCommandFromBroadcast(true);
//     EXPECT_TRUE(client->IsCommandFromBroadcast());
    
//     // Reset the flag
//     client->SetCommandFromBroadcast(false);
//     EXPECT_FALSE(client->IsCommandFromBroadcast());
// }

// int main(int argc, char** argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }