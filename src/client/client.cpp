#include "include/client.h"
#include <iostream>
#include <chrono>
#include <thread>

// --- Constructor ---
Client::Client(const std::vector<std::string>& client_ips, const std::string& server_addr)
    : server(server_addr) {
    connectToClients();
    connectToServer();
}

// --- Client Connection ---
void Client::connectToClients() {
    for (const auto& ip : active_clients) {
        getLatency(ip.first);
    }
}

void Client::connectToServer() {
    // TODO: Connect to central server
}

// --- Ping Handling ---
void Client::getLatency(const std::string& ip_address) {
    // TODO: Send PingRequest multiple times and measure average latency
}

// --- Request Parsing ---
void Client::parseRequest(const std::string& message) {
    // TODO: parse header, dispatch to appropriate handler
}

// --- Request Handlers ---
void Client::handlePingRequest(const PingRequest& req) {
    // TODO: send empty PingResponse immediately
}

void Client::handlePingResponse(const PingResponse& resp) {
    // TODO: measure latency and update active_clients
}

void Client::handleMusicRequest(const MusicRequest& req) {
    // TODO: handle start/pause/resume/stop based on fields in req
}

void Client::handleGetPositionRequest(const GetPositionRequest& req) {
    // TODO: respond with current player position
}

void Client::handleGetPositionResponse(const GetPositionResponse& resp) {
    // TODO: update or log external position
}

// --- Server communication ---
void Client::getSong(int song_number) {
    // TODO: download file from server, maybe store locally or load into AudioPlayer
}

// --- User command handler ---
void Client::userAction(const std::string& command) {
    if (command == "play") {
        // TODO: Send MusicRequest to all peers with wall_clock and position = 0
    } else if (command == "pause") {
        // TODO: Send MusicRequest with action = pause, current time & player position
    } else if (command == "resume") {
        // TODO: Send MusicRequest to resume playback at a wall-clock time
    } else if (command == "stop") {
        // TODO: Send stop MusicRequest to all clients
    } else if (command == "position") {
        // TODO: Send GetPositionRequest to a specified client
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
}