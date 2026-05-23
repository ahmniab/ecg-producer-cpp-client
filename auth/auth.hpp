#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

enum AuthJobStatus
{
    PENDING,
    SUCCESS,
    FAILED
};

class Auth
{
public:
    Auth(const std::string &server_address);
    ~Auth();
    std::string getAccessToken();
    bool isAuthenticated();
    bool startAuthentication();
    std::string getVerificationUri();
    std::string getUserCode();
    AuthJobStatus getAuthenticationStatus();

private:
    std::string server_address_;
    std::string access_token_;
    std::string refresh_token_;
    nlohmann::json auth_device_Response_;
    std::atomic<AuthJobStatus> auth_status_;
    long long auth_expiry_time_;
    std::string name_;
    std::string email_;
    nlohmann::json user_info_;
    std::thread auth_worker_;

    nlohmann::json sendAuthenticationRequest();
    nlohmann::json pollAuthenticationStatus(const std::string &device_code);
    nlohmann::json refreshToken();
    nlohmann::json getUserInfo();
    bool saveTokens();
};