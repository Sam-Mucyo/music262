#include "client/include/peer_network.h"
#include "client/include/peer_service_interface.h"
#include "client/include/client.h"
#include "../testlib/include/test_utils.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// Mock implementation of the PeerServiceInterface for testing
class MockPeerService : public music262::PeerServiceInterface {
public:
    MOCK_METHOD(bool, Ping, (const std::string& peer_address, int64_t& t1, int64_t& t2), (override));
    MOCK_METHOD(bool, Gossip, (const std::string& peer_address, const std::vector<std::string>& peer_list), (override));
    MOCK_METHOD(bool, SendMusicCommand, (const std::string& peer_address, const std::string& action, int position, int64_t wait_time_ms, int song_num), (override));
    MOCK_METHOD(bool, GetPosition, (const std::string& peer_address, int& position), (override));
    MOCK_METHOD(bool, Exit, (const std::string& peer_address), (override));
};

// Mock implementation of AudioServiceInterface for testing
class MockAudioService : public music262::AudioServiceInterface {
public:
    MOCK_METHOD(std::vector<std::string>, GetPlaylist, (), (override));
    MOCK_METHOD(bool, LoadAudio, (int song_num, music262::AudioChunkCallback callback), (override));
    MOCK_METHOD(std::vector<std::string>, GetPeerClientIPs, (), (override));
    MOCK_METHOD(bool, IsServerConnected, (), (override));
};

// Test fixture for PeerNetwork tests
class PeerNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create mock peer service with strict expectations (will fail if unexpected calls happen)
        mock_peer_service = std::make_unique<testing::StrictMock<MockPeerService>>();
        mock_peer_service_ptr = mock_peer_service.get();
        
        // Create the AudioClient with a mock service
        mock_audio_service = std::make_unique<testing::NiceMock<MockAudioService>>();
        audio_client = std::make_unique<AudioClient>(std::move(mock_audio_service));
        
        // Create the PeerNetwork with our mock service
        peer_network = std::make_unique<PeerNetwork>(
            audio_client.get(), 
            std::move(mock_peer_service)
        );
    }

    void TearDown() override {
        // Wait for any async operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Clean up resources
        peer_network.reset();
        audio_client.reset();
        
        // Verify all mock expectations have been satisfied
        testing::Mock::VerifyAndClearExpectations(mock_peer_service_ptr);
    }

    // Helper function to set up ping test with success/failure
    void SetupPingTest(bool success, int64_t t1_value = 1000, int64_t t2_value = 2000) {
        ON_CALL(*mock_peer_service_ptr, Ping(testing::_, testing::_, testing::_))
            .WillByDefault([success, t1_value, t2_value](
                const std::string& peer_address, int64_t& t1, int64_t& t2) {
                if (success) {
                    t1 = t1_value;
                    t2 = t2_value;
                }
                return success;
            });
    }

    // Mocks and the class under test
    MockPeerService* mock_peer_service_ptr;
    std::unique_ptr<testing::NiceMock<MockAudioService>> mock_audio_service;
    std::unique_ptr<AudioClient> audio_client;
    std::unique_ptr<PeerNetwork> peer_network;
    std::unique_ptr<testing::StrictMock<MockPeerService>> mock_peer_service;
};

// Test ConnectToPeer functionality with successful connection
TEST_F(PeerNetworkTest, ConnectToPeerSuccess) {
    std::string test_peer = "192.168.1.1:50052";
    
    // Setup ping to succeed
    SetupPingTest(true);
    
    // Expect the Ping method to be called exactly once with the correct address
    EXPECT_CALL(*mock_peer_service_ptr, Ping(test_peer, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    // Must expect Exit to be called in BroadcastExit during destruction
    EXPECT_CALL(*mock_peer_service_ptr, Exit(test_peer))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    bool result = peer_network->ConnectToPeer(test_peer);
    
    // Verify results
    EXPECT_TRUE(result);
    
    // Verify peer is in connected peers list
    auto peers = peer_network->GetConnectedPeers();
    EXPECT_THAT(peers, testing::Contains(test_peer));
}

// Test ConnectToPeer functionality with failed connection
TEST_F(PeerNetworkTest, ConnectToPeerFailure) {
    std::string test_peer = "192.168.1.1:50052";
    
    // Setup ping to fail
    SetupPingTest(false);
    
    // Expect the Ping method to be called exactly once and return false
    EXPECT_CALL(*mock_peer_service_ptr, Ping(test_peer, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(false));
    
    // Call the method under test
    bool result = peer_network->ConnectToPeer(test_peer);
    
    // Verify results
    EXPECT_FALSE(result);
    
    // Verify peer is NOT in connected peers list
    auto peers = peer_network->GetConnectedPeers();
    EXPECT_THAT(peers, testing::Not(testing::Contains(test_peer)));
}

// Test DisconnectFromPeer functionality
TEST_F(PeerNetworkTest, DisconnectFromPeer) {
    std::string test_peer = "192.168.1.1:50052";
    
    // First connect to a peer
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping(test_peer, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    peer_network->ConnectToPeer(test_peer);
    
    // Disconnect from the peer
    bool result = peer_network->DisconnectFromPeer(test_peer);
    
    // Verify results
    EXPECT_TRUE(result);
    
    // Verify peer is not in connected peers list
    auto peers = peer_network->GetConnectedPeers();
    EXPECT_THAT(peers, testing::Not(testing::Contains(test_peer)));
}

// Test DisconnectFromAllPeers functionality
TEST_F(PeerNetworkTest, DisconnectFromAllPeers) {
    // Connect to multiple peers first
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.1:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.2:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    peer_network->ConnectToPeer("192.168.1.1:50052");
    peer_network->ConnectToPeer("192.168.1.2:50052");
    
    // Verify we have 2 peers connected
    EXPECT_EQ(peer_network->GetConnectedPeers().size(), 2);
    
    // Call the method under test
    peer_network->DisconnectFromAllPeers();
    
    // Verify all peers are disconnected
    EXPECT_TRUE(peer_network->GetConnectedPeers().empty());
}

// Test BroadcastGossip functionality
TEST_F(PeerNetworkTest, BroadcastGossip) {
    // Connect to multiple peers first
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.1:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.2:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    peer_network->ConnectToPeer("192.168.1.1:50052");
    peer_network->ConnectToPeer("192.168.1.2:50052");
    
    // Reset expectations for clear verification
    testing::Mock::VerifyAndClearExpectations(mock_peer_service_ptr);
    
    // Setup expectations for the BroadcastGossip call
    // We expect Gossip to be called exactly once for each peer
    EXPECT_CALL(*mock_peer_service_ptr, Gossip("192.168.1.1:50052", testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
        
    EXPECT_CALL(*mock_peer_service_ptr, Gossip("192.168.1.2:50052", testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    // Also expect CalculateAverageOffset to measure network timing
    // Note: This tests that CalculateAverageOffset actually calls Ping
    EXPECT_CALL(*mock_peer_service_ptr, Ping(testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    
    // Must expect Exit to be called in BroadcastExit during destruction
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.1:50052"))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.2:50052"))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    peer_network->BroadcastGossip();
    
    // Expectations will be verified when test exits
}

// Test BroadcastLoad functionality
TEST_F(PeerNetworkTest, BroadcastLoad) {
    // Connect to multiple peers first
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.1:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.2:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    peer_network->ConnectToPeer("192.168.1.1:50052");
    peer_network->ConnectToPeer("192.168.1.2:50052");
    
    // Reset expectations for clear verification
    testing::Mock::VerifyAndClearExpectations(mock_peer_service_ptr);
    
    int test_song_num = 3;
    
    // Expect SendMusicCommand to be called exactly once for each peer with "load" action
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.1:50052", "load", 0, 0, test_song_num))
        .Times(1)
        .WillOnce(testing::Return(true));
        
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.2:50052", "load", 0, 0, test_song_num))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    // Must expect Exit to be called in BroadcastExit during destruction
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.1:50052"))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.2:50052"))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    bool result = peer_network->BroadcastLoad(test_song_num);
    
    // Verify results
    EXPECT_TRUE(result);
    
    // Expectations will be verified when test exits
}

// Test BroadcastLoad with partial failure
TEST_F(PeerNetworkTest, BroadcastLoadPartialFailure) {
    // Connect to multiple peers first
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.1:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.2:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    peer_network->ConnectToPeer("192.168.1.1:50052");
    peer_network->ConnectToPeer("192.168.1.2:50052");
    
    // Reset expectations for clear verification
    testing::Mock::VerifyAndClearExpectations(mock_peer_service_ptr);
    
    int test_song_num = 3;
    
    // Expect SendMusicCommand to succeed for first peer but fail for second
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.1:50052", "load", 0, 0, test_song_num))
        .Times(1)
        .WillOnce(testing::Return(true));
        
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.2:50052", "load", 0, 0, test_song_num))
        .Times(1)
        .WillOnce(testing::Return(false));
    
    // Must expect Exit to be called in BroadcastExit during destruction
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.1:50052"))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.2:50052"))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    bool result = peer_network->BroadcastLoad(test_song_num);
    
    // Verify it fails because not all peers succeeded
    EXPECT_FALSE(result);
    
    // Expectations will be verified when test exits
}

// Test that can detect the bug in BroadcastCommand
TEST_F(PeerNetworkTest, BroadcastCommandSendsToAllPeers) {
    // Connect to multiple peers first
    SetupPingTest(true);
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.1:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Ping("192.168.1.2:50052", testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(true));
    
    peer_network->ConnectToPeer("192.168.1.1:50052");
    peer_network->ConnectToPeer("192.168.1.2:50052");
    
    // Reset expectations for clear verification
    testing::Mock::VerifyAndClearExpectations(mock_peer_service_ptr);
    
    // Expect CalculateAverageOffset to be called to measure network timing
    EXPECT_CALL(*mock_peer_service_ptr, Ping(testing::_, testing::_, testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(testing::Return(true));
    
    // Expect SendMusicCommand to be called for each peer with play action
    // These expectations MUST be met or the test will fail, which will catch the bug
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.1:50052", "play", 0, testing::_, -1))
        .Times(1)  // Enforce that this must be called exactly once
        .WillOnce(testing::Return(true));
        
    EXPECT_CALL(*mock_peer_service_ptr, 
                SendMusicCommand("192.168.1.2:50052", "play", 0, testing::_, -1))
        .Times(1)  // Enforce that this must be called exactly once
        .WillOnce(testing::Return(true));
    
    // Must expect Exit to be called in BroadcastExit during destruction
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.1:50052"))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_peer_service_ptr, Exit("192.168.1.2:50052"))
        .WillOnce(testing::Return(true));
    
    // Call the method under test
    peer_network->BroadcastCommand("play", 0);
    
    // Since we're using threads, wait for them to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // GMock will automatically verify that all expected calls were made,
    // and the test will fail if not
}

// Test server port functionality
TEST_F(PeerNetworkTest, GetServerPort) {
    // Set up server port
    int test_port = 50052;
    peer_network->StartServer(test_port);
    
    // Verify we can get the port
    EXPECT_EQ(peer_network->GetServerPort(), test_port);
    
    // Clean up
    peer_network->StopServer();
}

// Test StartServer and StopServer functionality
TEST_F(PeerNetworkTest, StartAndStopServer) {
    int test_port = 50053;
    
    // Start should succeed
    EXPECT_TRUE(peer_network->StartServer(test_port));
    
    // Server port should be set
    EXPECT_EQ(peer_network->GetServerPort(), test_port);
    
    // Stop server
    peer_network->StopServer();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}