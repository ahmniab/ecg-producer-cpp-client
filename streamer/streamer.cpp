#include "streamer.h"
#include <chrono>
#include <iostream>
#include <thread>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

Streamer::Streamer(std::string server_address)
{
    server_address_ = server_address;
    is_connected_ = false;
}

Streamer::Streamer(std::string server_address, std::string token)
{
    server_address_ = server_address;
    token_ = token;
    is_connected_ = false;
}

Streamer::~Streamer()
{
    disconnect();
}

ConnectionResult Streamer::connect()
{
    if (is_connected_)
        return {true, 0, ""};

    if (token_.empty())
    {
        return {false, 2, "Missing authorization token"};
    }

    context_.AddMetadata("authorization", "Bearer " + token_);

    channel_ = grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials());

    // Wait briefly for the channel to become READY (server reachable)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto state = channel_->GetState(true);
        if (state == GRPC_CHANNEL_READY)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto final_state = channel_->GetState(false);
    if (final_state != GRPC_CHANNEL_READY)
    {
        return {false, 1, "Channel not ready or server unreachable"};
    }

    stub_ = ecg::ECGService::NewStub(channel_);

    writer_ = stub_->StreamECG(&context_, &ack_);
    is_connected_ = true;
    return {true, 0, ""};
}

void Streamer::disconnect()
{
    if (is_connected_)
    {
        writer_->WritesDone();
        status_ = writer_->Finish();
        if (!status_.ok())
        {
            std::cerr << "gRPC Stream failed: " << status_.error_message() << std::endl;
        }
        else
        {
            std::cout << "Stream completed successfully. Received ack for " << ack_.received_samples() << " samples." << std::endl;
        }
        is_connected_ = false;
    }
}

void Streamer::sendSample(float sample)
{
    if (!is_connected_)
    {
        std::cerr << "Streamer is not connected." << std::endl;
        return;
    }

    ecg::ECGSample ecg_sample;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::cout << "timestamp: " << timestamp << std::endl;
    ecg_sample.set_timestamp(timestamp);
    ecg_sample.set_voltage(sample);

    if (!writer_->Write(ecg_sample))
    {
        std::cerr << "Failed to write sample to stream." << std::endl;
        is_connected_ = false;
    }
}
