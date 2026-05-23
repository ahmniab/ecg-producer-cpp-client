#include "ecg_producer.hpp"

ECGProducer::ECGProducer(const std::string &server_address, const std::string &auth_server)
    : auth_(auth_server), server_address_(server_address)
{
}

ECGProducer::~ECGProducer()
{
    disconnect();
}

bool ECGProducer::isAuthenticated()
{
    return auth_.isAuthenticated();
}

bool ECGProducer::startAuthentication()
{
    return auth_.startAuthentication();
}

std::string ECGProducer::getVerificationUri()
{
    return auth_.getVerificationUri();
}

std::string ECGProducer::getUserCode()
{
    return auth_.getUserCode();
}

AuthJobStatus ECGProducer::getAuthenticationStatus()
{
    return auth_.getAuthenticationStatus();
}

bool ECGProducer::isConnected() const
{
    return streamer_ != nullptr && streamer_->isConnected();
}

ConnectionResult ECGProducer::connect()
{
    if (isConnected())
    {
        return {true, 0, ""};
    }

    if (!auth_.isAuthenticated())
    {
        return {false, 2, "Authentication failed or token unavailable"};
    }

    const std::string token = auth_.getAccessToken();
    if (token.empty())
    {
        return {false, 2, "Missing authorization token"};
    }

    streamer_ = std::make_unique<Streamer>(server_address_, token);
    ConnectionResult result = streamer_->connect();
    if (!result.success)
    {
        streamer_.reset();
    }

    return result;
}

void ECGProducer::disconnect()
{
    if (streamer_ != nullptr)
    {
        streamer_->disconnect();
        streamer_.reset();
    }
}

bool ECGProducer::initECGStream()
{
    return connect().success;
}

void ECGProducer::sendSample(float sample)
{
    if (!isConnected())
    {
        return;
    }

    streamer_->sendSample(sample);
}
