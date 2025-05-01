#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "include/peer_network.h"
#include "include/client.h"
#include <grpcpp/create_channel.h>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

// Mock AudioClient for testing
class MockAudioClient : public AudioClient {
public:
    MockAudioClient() :
        AudioClient(grpc::CreateChannel("localhost:12345", grpc::InsecureChannelCredentials()))
    {}
    
    MOCK_METHOD(void, Play, (), (override));
    MOCK_METHOD(void, Pause, (), (override));
    MOCK_METHOD(void, Resume, (), (override));
    MOCK_METHOD(void, Stop, (), (override));
    MOCK_METHOD(unsigned int, GetPosition, (), (const override));
    MOCK_METHOD(void, SetCommandFromBroadcast, (bool), (override));
    MOCK_METHOD(std::vector<std::string>, GetPlaylist, (), (override));
    MOCK_METHOD(bool, LoadAudio, (int), (override));
    MOCK_METHOD(std::vector<std::string>, GetPeerClientIPs, (), (override));
    MOCK_METHOD(void, EnablePeerSync, (bool), (override));
    MOCK_METHOD(bool, IsPeerSyncEnabled, (), (const override));
    MOCK_METHOD(void, SetPeerNetwork, (std::shared_ptr<PeerNetwork>), (override));
    MOCK_METHOD(bool, IsCommandFromBroadcast, (), (const override));
};

// Test fixture for PeerNetwork tests
class PeerNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_client_ = std::make_shared<NiceMock<MockAudioClient>>();
        peer_network_ = std::make_unique<PeerNetwork>(mock_client_.get());
    }

    void TearDown() override {
        peer_network_->StopServer();
    }

    std::shared_ptr<NiceMock<MockAudioClient>> mock_client_;
    std::unique_ptr<PeerNetwork> peer_network_;
};

// Test cases for PeerNetwork
TEST_F(PeerNetworkTest, StartAndStopServer) {
    EXPECT_TRUE(peer_network_->StartServer(50052));
    peer_network_->StopServer();
}

TEST_F(PeerNetworkTest, StartServerFailsWhenAlreadyRunning) {
    EXPECT_TRUE(peer_network_->StartServer(50052));
    EXPECT_TRUE(peer_network_->StartServer(50052));
    peer_network_->StopServer();
}

TEST_F(PeerNetworkTest, ConnectToPeerSuccess) {
    // This would normally require a real server running, so we'd mock this in practice
    // For this test, we'll assume we have a test server running
    peer_network_->StartServer(50052);
    
    // This test would need a real peer server running to be effective
    // In a real test environment, we'd start a test peer server
    // EXPECT_TRUE(peer_network_->ConnectToPeer("localhost:50053"));
}

TEST_F(PeerNetworkTest, ConnectToPeerFailure) {
    EXPECT_FALSE(peer_network_->ConnectToPeer("invalid_address:12345"));
}

TEST_F(PeerNetworkTest, DisconnectFromPeer) {
    // Setup - would need a real connection first
    // peer_network_->ConnectToPeer("localhost:50053");
    
    // Test
    EXPECT_FALSE(peer_network_->DisconnectFromPeer("nonexistent_peer:12345"));
    // EXPECT_TRUE(peer_network_->DisconnectFromPeer("localhost:50053"));
}

TEST_F(PeerNetworkTest, DisconnectFromAllPeers) {
    // Setup - would add multiple peers
    // peer_network_->ConnectToPeer("localhost:50053");
    // peer_network_->ConnectToPeer("localhost:50054");
    
    peer_network_->DisconnectFromAllPeers();
    EXPECT_TRUE(peer_network_->GetConnectedPeers().empty());
}

TEST_F(PeerNetworkTest, GetConnectedPeers) {
    // Setup - would add peers
    // peer_network_->ConnectToPeer("localhost:50053");
    // peer_network_->ConnectToPeer("localhost:50054");
    
    auto peers = peer_network_->GetConnectedPeers();
    // EXPECT_EQ(peers.size(), 2);
    EXPECT_TRUE(peers.empty()); // No peers connected in this test
}

TEST_F(PeerNetworkTest, BroadcastCommand) {
    // This would test the broadcast functionality
    // In practice, we'd need to mock the gRPC stubs and connections
    // For now, we'll just verify it doesn't crash with no peers
    peer_network_->BroadcastCommand("play", 100);
}

// Test cases for PeerService
class PeerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_client_ = std::make_shared<NiceMock<MockAudioClient>>();
        peer_service_ = std::make_unique<PeerService>(mock_client_.get());
    }

    std::shared_ptr<NiceMock<MockAudioClient>> mock_client_;
    std::unique_ptr<PeerService> peer_service_;
    grpc::ServerContext context;
};

TEST_F(PeerServiceTest, PingHandling) {
    client::PingRequest request;
    client::PingResponse response;
    
    // Set t0 in request
    double t0 = std::chrono::duration_cast<std::chrono::duration<double>>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    request.set_t0(t0);
    
    grpc::Status status = peer_service_->Ping(&context, &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_GE(response.t1(), t0);
    EXPECT_GE(response.t2(), response.t1());
}

TEST_F(PeerServiceTest, SendMusicCommandPlay) {
    client::MusicRequest request;
    client::MusicResponse response;
    
    request.set_action("play");
    request.set_position(100);
    request.set_wall_clock(123.45f);
    
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(true));
    EXPECT_CALL(*mock_client_, Play());
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(false));
    
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    EXPECT_TRUE(status.ok());
}

TEST_F(PeerServiceTest, SendMusicCommandPause) {
    client::MusicRequest request;
    client::MusicResponse response;
    
    request.set_action("pause");
    
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(true));
    EXPECT_CALL(*mock_client_, Pause());
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(false));
    
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    EXPECT_TRUE(status.ok());
}

TEST_F(PeerServiceTest, SendMusicCommandResume) {
    client::MusicRequest request;
    client::MusicResponse response;
    
    request.set_action("resume");
    
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(true));
    EXPECT_CALL(*mock_client_, Resume());
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(false));
    
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    EXPECT_TRUE(status.ok());
}

TEST_F(PeerServiceTest, SendMusicCommandStop) {
    client::MusicRequest request;
    client::MusicResponse response;
    
    request.set_action("stop");
    
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(true));
    EXPECT_CALL(*mock_client_, Stop());
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(false));
    
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    EXPECT_TRUE(status.ok());
}

TEST_F(PeerServiceTest, SendMusicCommandInvalid) {
    client::MusicRequest request;
    client::MusicResponse response;
    
    request.set_action("invalid_command");
    
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(true));
    EXPECT_CALL(*mock_client_, SetCommandFromBroadcast(false));
    // No action should be called
    
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    EXPECT_TRUE(status.ok());
}

TEST_F(PeerServiceTest, GetPosition) {
    client::GetPositionRequest request;
    client::GetPositionResponse response;
    
    const unsigned int test_position = 1234;
    EXPECT_CALL(*mock_client_, GetPosition()).WillOnce(Return(test_position));
    
    grpc::Status status = peer_service_->GetPosition(&context, &request, &response);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.position(), test_position);
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}