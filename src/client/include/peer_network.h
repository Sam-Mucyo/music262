#ifndef PEER_NETWORK_H
#define PEER_NETWORK_H

// #include "include/client.h"
#include "proto/audio_sync.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class PeerNetwork {
public:
    // Make constructor public and explicit
    explicit PeerNetwork(AudioClient* client);
    
    // Delete copy constructor and assignment operator
    PeerNetwork(const PeerNetwork&) = delete;
    PeerNetwork& operator=(const PeerNetwork&) = delete;
    
    ~PeerNetwork();

    // Server management
    bool StartServer(int port);
    void StopServer();
    void ForceShutdown();

    // Peer connection management
    bool ConnectToPeer(const std::string& peer_address);
    bool DisconnectFromPeer(const std::string& peer_address);
    void DisconnectFromAllPeers();
    std::vector<std::string> GetConnectedPeers() const;

    // Peer communication
    bool SendPingToPeer(const std::string& peer_address);
    void BroadcastCommand(const std::string& action, int position);

private:
    // Private member variables
    AudioClient* client_;
    bool server_running_;
    int server_port_;
    int rtt;  // Round-trip time in ms

    // gRPC server members
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<class PeerService> service_;
    std::thread server_thread_;

    // Peer connections
    std::unordered_map<std::string, std::unique_ptr<client::ClientHandler::Stub>> peer_stubs_;
    std::unordered_map<std::string, double> peer_offsets_;
    mutable std::mutex peers_mutex_;
};

#endif // PEER_NETWORK_H