#include "auth.hpp"

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace
{
    constexpr const char *kClientId = "cpp-ui";
    constexpr const char *kScope = "openid offline_access";
    constexpr const char *kRefreshFileName = "refresh-token.json";
    constexpr const char *kUserInfoFileName = "userinfo.json";

    size_t writeResponse(char *contents, size_t size, size_t nmemb, void *userp)
    {
        auto *response = static_cast<std::string *>(userp);
        response->append(contents, size * nmemb);
        return size * nmemb;
    }

    std::string trimTrailingSlash(std::string value)
    {
        while (!value.empty() && value.back() == '/')
        {
            value.pop_back();
        }
        return value;
    }

    std::string urlEncode(CURL *curl, const std::string &value)
    {
        char *encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
        if (!encoded)
        {
            return {};
        }

        std::string result(encoded);
        curl_free(encoded);
        return result;
    }

    std::filesystem::path configDirectory()
    {
        const char *home = std::getenv("HOME");
        if (home != nullptr && *home != '\0')
        {
            return std::filesystem::path(home) / ".client-streamer";
        }

        return std::filesystem::temp_directory_path() / ".client-streamer";
    }

    std::filesystem::path refreshTokenPath()
    {
        return configDirectory() / kRefreshFileName;
    }

    std::filesystem::path userInfoPath()
    {
        return configDirectory() / kUserInfoFileName;
    }

    bool readJsonFile(const std::filesystem::path &path, nlohmann::json &value)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        try
        {
            file >> value;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool writeJsonFile(const std::filesystem::path &path, const nlohmann::json &value)
    {
        std::error_code error_code;
        std::filesystem::create_directories(path.parent_path(), error_code);
        if (error_code)
        {
            return false;
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open())
        {
            return false;
        }

        file << value.dump(2);
        return file.good();
    }

    class CurlHandle
    {
    public:
        CurlHandle()
            : handle_(curl_easy_init())
        {
        }

        ~CurlHandle()
        {
            if (handle_ != nullptr)
            {
                curl_easy_cleanup(handle_);
            }
        }

        CURL *get() const
        {
            return handle_;
        }

        explicit operator bool() const
        {
            return handle_ != nullptr;
        }

    private:
        CURL *handle_;
    };

    nlohmann::json performJsonRequest(const std::string &url,
                                      const std::string &method,
                                      const std::string &body,
                                      const std::vector<std::string> &headers)
    {
        CurlHandle curl;
        if (!curl)
        {
            return {};
        }

        std::string response_body;
        struct curl_slist *header_list = nullptr;

        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeResponse);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);

        for (const auto &header : headers)
        {
            header_list = curl_slist_append(header_list, header.c_str());
        }

        if (header_list != nullptr)
        {
            curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list);
        }

        if (method == "POST")
        {
            curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }

        CURLcode code = curl_easy_perform(curl.get());

        long http_code = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

        if (header_list != nullptr)
        {
            curl_slist_free_all(header_list);
        }

        if (code != CURLE_OK || http_code < 200 || http_code >= 300)
        {
            return {};
        }

        try
        {
            return nlohmann::json::parse(response_body);
        }
        catch (...)
        {
            return {};
        }
    }

} // namespace

Auth::Auth(const std::string &server_address)
    : server_address_(trimTrailingSlash(server_address)), auth_status_(FAILED), auth_expiry_time_(0)
{
}

Auth::~Auth()
{
    if (auth_worker_.joinable())
    {
        auth_worker_.join();
    }
}

std::string Auth::getAccessToken()
{
    if (access_token_.empty())
    {
        isAuthenticated();
    }

    return access_token_;
}

bool Auth::isAuthenticated()
{
    nlohmann::json refresh_file;
    if (!readJsonFile(refreshTokenPath(), refresh_file))
    {
        auth_status_.store(FAILED);
        return false;
    }

    refresh_token_ = refresh_file.value("refresh_token", "");
    if (refresh_token_.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    nlohmann::json refreshed = refreshToken();
    if (refreshed.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    access_token_ = refreshed.value("access_token", "");
    if (access_token_.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    if (refreshed.contains("refresh_token") && !refreshed.value("refresh_token", "").empty())
    {
        refresh_token_ = refreshed.value("refresh_token", refresh_token_);
    }

    nlohmann::json user_info = getUserInfo();
    if (user_info.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    user_info_ = user_info;
    name_ = user_info.value("name", "");
    email_ = user_info.value("email", "");
    auth_status_.store(SUCCESS);
    saveTokens();
    return true;
}

bool Auth::startAuthentication()
{
    if (auth_worker_.joinable())
    {
        auth_worker_.join();
    }

    auth_status_.store(PENDING);
    auth_device_Response_ = sendAuthenticationRequest();
    if (auth_device_Response_.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    const std::string device_code = auth_device_Response_.value("device_code", "");
    if (device_code.empty())
    {
        auth_status_.store(FAILED);
        return false;
    }

    auth_worker_ = std::thread([this, device_code]()
                               {
        nlohmann::json token_response = pollAuthenticationStatus(device_code);
        if (token_response.empty())
        {
            auth_status_.store(FAILED);
            return;
        }

        access_token_ = token_response.value("access_token", "");
        refresh_token_ = token_response.value("refresh_token", "");
        auth_expiry_time_ = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count() + token_response.value("expires_in", 0);

        if (access_token_.empty() || refresh_token_.empty())
        {
            auth_status_.store(FAILED);
            return;
        }

        nlohmann::json user_info = getUserInfo();
        if (user_info.empty())
        {
            auth_status_.store(FAILED);
            return;
        }

        user_info_ = user_info;
        name_ = user_info.value("name", "");
        email_ = user_info.value("email", "");
        saveTokens();
        auth_status_.store(SUCCESS); });

    return true;
}

std::string Auth::getVerificationUri()
{
    return auth_device_Response_.value("verification_uri", "");
}

std::string Auth::getUserCode()
{
    return auth_device_Response_.value("user_code", "");
}

AuthJobStatus Auth::getAuthenticationStatus()
{
    return auth_status_.load();
}

nlohmann::json Auth::sendAuthenticationRequest()
{
    CurlHandle curl;
    if (!curl)
    {
        return {};
    }

    const std::string body = "client_id=" + urlEncode(curl.get(), kClientId) +
                             "&scope=" + urlEncode(curl.get(), kScope);
    const std::string url = server_address_ + "/protocol/openid-connect/auth/device";

    return performJsonRequest(url,
                              "POST",
                              body,
                              {"Content-Type: application/x-www-form-urlencoded",
                               "Accept: application/json"});
}

nlohmann::json Auth::pollAuthenticationStatus(const std::string &device_code)
{
    const int expires_in = auth_device_Response_.value("expires_in", 600);
    int interval = auth_device_Response_.value("interval", 5);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in);
    const std::string url = server_address_ + "/protocol/openid-connect/token";

    while (std::chrono::steady_clock::now() < deadline)
    {
        CurlHandle curl;
        if (!curl)
        {
            return {};
        }

        const std::string body = "grant_type=" +
                                 urlEncode(curl.get(), "urn:ietf:params:oauth:grant-type:device_code") +
                                 "&device_code=" + urlEncode(curl.get(), device_code) +
                                 "&client_id=" + urlEncode(curl.get(), kClientId);

        nlohmann::json response = performJsonRequest(url,
                                                     "POST",
                                                     body,
                                                     {"Content-Type: application/x-www-form-urlencoded",
                                                      "Accept: application/json"});
        if (!response.empty())
        {
            if (response.contains("error"))
            {
                const std::string error = response.value("error", "");
                if (error == "authorization_pending")
                {
                    std::this_thread::sleep_for(std::chrono::seconds(interval));
                    continue;
                }

                if (error == "slow_down")
                {
                    interval += 5;
                    std::this_thread::sleep_for(std::chrono::seconds(interval));
                    continue;
                }

                return {};
            }

            return response;
        }

        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }

    return {};
}

nlohmann::json Auth::refreshToken()
{
    if (refresh_token_.empty())
    {
        return {};
    }

    CurlHandle curl;
    if (!curl)
    {
        return {};
    }

    const std::string body = "grant_type=" +
                             urlEncode(curl.get(), "refresh_token") +
                             "&refresh_token=" + urlEncode(curl.get(), refresh_token_) +
                             "&client_id=" + urlEncode(curl.get(), kClientId);
    const std::string url = server_address_ + "/protocol/openid-connect/token";

    return performJsonRequest(url,
                              "POST",
                              body,
                              {"Content-Type: application/x-www-form-urlencoded",
                               "Accept: application/json"});
}

nlohmann::json Auth::getUserInfo()
{
    if (access_token_.empty())
    {
        return {};
    }

    const std::string url = server_address_ + "/protocol/openid-connect/userinfo";
    return performJsonRequest(url,
                              "GET",
                              "",
                              {"Accept: application/json",
                               "Authorization: Bearer " + access_token_});
}

bool Auth::saveTokens()
{
    nlohmann::json refresh_file = {
        {"access_token", access_token_},
        {"refresh_token", refresh_token_},
        {"expires_at", auth_expiry_time_},
        {"server_address", server_address_}};

    return writeJsonFile(refreshTokenPath(), refresh_file) && writeJsonFile(userInfoPath(), user_info_);
}