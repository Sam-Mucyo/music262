#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "client_handler.grpc.pb.h" // Generated Protobuf header

// Forward declarations for Protobuf message types
class PingRequest;
class PingResponse;
class MusicRequest;
class MusicResponse;
class GetPositionRequest;
class GetPositionResponse;

class Client {
public:
    // Constructor
    Client(const std::vector<std::string>& client_ips, const std::string& server_addr);

    // Latency & Connection
    void connectToClients();
    void connectToServer();
    void getLatency(const std::string& ip_address);

    // Handle incoming message logic
    void parseRequest(const std::string& message); // from a client
    void handlePingRequest(const PingRequest& req);
    void handleMusicRequest(const MusicRequest& req);
    void handleGetPositionRequest(const GetPositionRequest& req);
    void handlePingResponse(const PingResponse& resp);
    void handleGetPositionResponse(const GetPositionResponse& resp);

    // Client-side user interaction (send to peers)
    void userAction(const std::string& command);

    // Download song from server
    void getSong(int song_number);

private:
    std::unordered_map<std::string, float> active_clients; // ip -> latency
    std::string server;
};
