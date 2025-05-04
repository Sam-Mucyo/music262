#ifndef PEER_SERVICE_H
#define PEER_SERVICE_H

#include <grpcpp/grpcpp.h>
#include "client.h"
#include "logger.h"
#include "../proto/audio_sync.grpc.pb.h" 

class PeerNetwork;  // Forward declaration

class PeerService final : public client::ClientHandler::Service {
 public:
  explicit PeerService(AudioClient* client);
  
  grpc::Status Ping(grpc::ServerContext* context,
                    const client::PingRequest* request,
                    client::PingResponse* response) override;
  
  grpc::Status Gossip(grpc::ServerContext* context,
                     const client::GossipRequest* request,
                     client::GossipResponse* response) override;
  
  grpc::Status SendMusicCommand(grpc::ServerContext* context,
                               const client::MusicRequest* request,
                               client::MusicResponse* response) override;
  

  grpc::Status GetPosition(grpc::ServerContext* context,
                          const client::GetPositionRequest* request,
                          client::GetPositionResponse* response) override;

 private:
  AudioClient* client_;
};

#endif  // PEER_SERVICE_H