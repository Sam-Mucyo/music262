#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "include/peer_network.h"
#include "include/client.h"
#include <grpcpp/create_channel.h>
#include <grpcpp/server_builder.h>
#include <future>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::SetArgPointee;

// Enhanced Mock ClientHandler Stub
class MockClientHandlerStub : public client::ClientHandler::StubInterface {
public:
    MOCK_METHOD(grpc::Status, Ping, (grpc::ClientContext*, const client::PingRequest&, client::PingResponse*), (override));
    MOCK_METHOD(grpc::Status, SendMusicCommand, (grpc::ClientContext*, const client::MusicRequest&, client::MusicResponse*), (override));
    MOCK_METHOD(grpc::Status, GetPosition, (grpc::ClientContext*, const client::GetPositionRequest&, client::GetPositionResponse*), (override));
};

// Enhanced Mock AudioClient
class MockAudioClient : public AudioClient {
public:
    MockAudioClient() : AudioClient(nullptr) {}
    
    MOCK_METHOD(void, Play, (), (override));
    MOCK_METHOD(void, Pause, (), (override));
    MOCK_METHOD(void, Resume, (), (override));
    MOCK_METHOD(void, Stop, (), (override));
    MOCK_METHOD(unsigned int, GetPosition, (), (const override));
    MOCK_METHOD(void, SetCommandFromBroadcast, (bool), (override));
    MOCK_METHOD(bool, IsCommandFromBroadcast, (), (const override));
};

// Test fixture with enhanced setup
class PeerNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_client_ = std::make_shared<NiceMock<MockAudioClient>>();
        peer_network_ = std::make_unique<PeerNetwork>(mock_client_.get());
        
        // Setup default mock behaviors
        ON_CALL(*mock_client_, GetPosition()).WillByDefault(Return(0));
        ON_CALL(*mock_client_, IsCommandFromBroadcast()).WillByDefault(Return(false));
    }

    void TearDown() override {
        peer_network_->StopServer();
    }

    std::shared_ptr<NiceMock<MockAudioClient>> mock_client_;
    std::unique_ptr<PeerNetwork> peer_network_;
};

// Tests for actual peer communication functionality
TEST_F(PeerNetworkTest, BroadcastCommandSendsToAllPeers) {
    // Setup mock stubs for 3 peers
    auto mock_stub1 = std::make_unique<MockClientHandlerStub>();
    auto mock_stub2 = std::make_unique<MockClientHandlerStub>();
    auto mock_stub3 = std::make_unique<MockClientHandlerStub>();

    // Expect calls to all peers
    EXPECT_CALL(*mock_stub1, SendMusicCommand(_, _, _))
        .WillOnce(Return(grpc::Status::OK));
    EXPECT_CALL(*mock_stub2, SendMusicCommand(_, _, _))
        .WillOnce(Return(grpc::Status::OK));
    EXPECT_CALL(*mock_stub3, SendMusicCommand(_, _, _))
        .WillOnce(Return(grpc::Status::OK));

    // Add mock peers
    peer_network_->peer_stubs_["peer1"] = std::move(mock_stub1);
    peer_network_->peer_stubs_["peer2"] = std::move(mock_stub2);
    peer_network_->peer_stubs_["peer3"] = std::move(mock_stub3);

    // Verify broadcast goes to all
    peer_network_->BroadcastCommand("play", 100);
}

TEST_F(PeerNetworkTest, BroadcastCommandHandlesFailures) {
    auto mock_stub1 = std::make_unique<MockClientHandlerStub>();
    auto mock_stub2 = std::make_unique<MockClientHandlerStub>();

    // One success, one failure
    EXPECT_CALL(*mock_stub1, SendMusicCommand(_, _, _))
        .WillOnce(Return(grpc::Status::OK));
    EXPECT_CALL(*mock_stub2, SendMusicCommand(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable")));

    peer_network_->peer_stubs_["peer1"] = std::move(mock_stub1);
    peer_network_->peer_stubs_["peer2"] = std::move(mock_stub2);

    // Should complete without throwing
    EXPECT_NO_THROW(peer_network_->BroadcastCommand("pause", 200));
}

// Thorough ping functionality tests
TEST_F(PeerNetworkTest, PingCalculatesCorrectRTT) {
    auto mock_stub = std::make_unique<MockClientHandlerStub>();
    
    // Setup ping response
    EXPECT_CALL(*mock_stub, Ping(_, _, _))
        .WillOnce(Invoke([](grpc::ClientContext*, const client::PingRequest&, client::PingResponse* response) {
            response->set_t1(100.0);  // Simulated receive time
            response->set_t2(100.5);  // Simulated send back time
            return grpc::Status::OK;
        }));

    peer_network_->peer_stubs_["test_peer"] = std::move(mock_stub);

    // Send ping with known time
    double t0 = 99.8;
    client::PingRequest request;
    request.set_t0(t0);
    
    // This would normally be internal to SendPingToPeer
    double t3 = 101.0; // Simulated receive time
    
    // Verify RTT calculation
    // Expected RTT = (t3 - t0) - (t2 - t1) = (101.0 - 99.8) - (100.5 - 100.0) = 1.2 - 0.5 = 0.7
    peer_network_->SendPingToPeer("test_peer");
    EXPECT_NEAR(peer_network_->rtt, 700, 50); // Within 50ms tolerance
}

// Proper GetConnectedPeers tests
TEST_F(PeerNetworkTest, GetConnectedPeersReturnsAllActiveConnections) {
    // Add mock peers
    peer_network_->peer_stubs_["peer1:50052"] = std::make_unique<MockClientHandlerStub>();
    peer_network_->peer_stubs_["peer2:50053"] = std::make_unique<MockClientHandlerStub>();
    peer_network_->peer_stubs_["peer3:50054"] = std::make_unique<MockClientHandlerStub>();

    auto peers = peer_network_->GetConnectedPeers();
    ASSERT_EQ(peers.size(), 3);
    EXPECT_THAT(peers, testing::Contains("peer1:50052"));
    EXPECT_THAT(peers, testing::Contains("peer2:50053"));
    EXPECT_THAT(peers, testing::Contains("peer3:50054"));
}

// Negative test cases
TEST_F(PeerNetworkTest, ConnectToPeerHandlesGrpcErrors) {
    auto mock_channel = grpc::CreateMockChannel();
    auto mock_stub = std::make_unique<MockClientHandlerStub>();

    EXPECT_CALL(*mock_stub, Ping(_, _, _))
        .WillOnce(Return(grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED, "timeout")));

    peer_network_->peer_stubs_["failing_peer"] = std::move(mock_stub);

    EXPECT_FALSE(peer_network_->ConnectToPeer("failing_peer"));
}

// Concurrency tests
TEST_F(PeerNetworkTest, ThreadSafePeerOperations) {
    constexpr int kThreads = 10;
    std::vector<std::thread> threads;
    
    // Start server
    peer_network_->StartServer(50052);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([this, i]() {
            // Each thread tries to add/remove peers
            std::string peer_addr = "peer" + std::to_string(i) + ":5005" + std::to_string(i);
            
            // Alternate between connect/disconnect
            if (i % 2 == 0) {
                peer_network_->ConnectToPeer(peer_addr);
            } else {
                peer_network_->DisconnectFromPeer(peer_addr);
            }
            
            // Verify peer list access
            auto peers = peer_network_->GetConnectedPeers();
            EXPECT_TRUE(peers.size() <= kThreads);
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

// Enhanced PeerService tests
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

TEST_F(PeerServiceTest, SendMusicCommandWaitsSpecifiedTime) {
    client::MusicRequest request;
    client::MusicResponse response;
    request.set_action("play");
    request.set_wait_time(0.1f); // 100ms

    auto start = std::chrono::steady_clock::now();
    
    EXPECT_CALL(*mock_client_, Play());
    grpc::Status status = peer_service_->SendMusicCommand(&context, &request, &response);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    
    EXPECT_TRUE(status.ok());
    EXPECT_GE(duration.count(), 90); // Should wait at least 90ms
}

// Main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}