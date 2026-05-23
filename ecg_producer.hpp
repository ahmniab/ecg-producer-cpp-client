#pragma once
#include <memory>
#include <string>

#include "streamer/streamer.h"
#include "auth/auth.hpp"
class ECGProducer
{
public:
    ECGProducer(const std::string &server_address, const std::string &auth_server);
    ~ECGProducer();

    bool isAuthenticated();
    bool startAuthentication();
    std::string getVerificationUri();
    std::string getUserCode();
    AuthJobStatus getAuthenticationStatus();
    bool isConnected() const;

    ConnectionResult connect();
    void disconnect();
    bool initECGStream();
    void sendSample(float sample);

private:
    Auth auth_;
    std::string server_address_;
    std::unique_ptr<Streamer> streamer_;
};