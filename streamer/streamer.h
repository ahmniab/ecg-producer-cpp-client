#pragma once

#include <grpcpp/grpcpp.h>
#include "generated/ecg.grpc.pb.h"

typedef struct
{
    bool success;
    int error_code;
    std::string error_message;
} ConnectionResult;

class Streamer
{
public:
    Streamer(std::string server_address);
    Streamer(std::string server_address, std::string token);

    ConnectionResult connect();
    void disconnect();

    void sendSample(float sample);

    bool isConnected() const { return is_connected_; }

    ~Streamer();

private:
    grpc::ClientContext context_;
    grpc::Status status_;
    std::unique_ptr<ecg::ECGService::Stub> stub_;
    std::unique_ptr<grpc::ClientWriter<ecg::ECGSample>> writer_;
    std::shared_ptr<grpc::Channel> channel_;
    ecg::StreamAck ack_;
    std::string server_address_;
    std::string token_;
    bool is_connected_ = false;
};