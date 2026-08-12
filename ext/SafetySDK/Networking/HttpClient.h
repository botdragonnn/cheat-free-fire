#pragma once
#include <string>
#include <vector>
#include <map>

namespace SafetySDK {

    struct HttpResponse {
        int statusCode = 0;
        std::string body;
        std::map<std::string, std::string> headers;
    };

    class HttpClient {
    private:
        std::wstring baseDomain;
        int port = 443;
        bool useHttps = true;
        std::string token;
        std::string discordId;
        std::string licenseKey;
        std::string productHash;
        std::wstring userAgent = L"SafetySDK-Client/1.0";

    public:
        HttpClient(const std::wstring& domain, int port = 443, bool https = true);
        ~HttpClient();

        void SetToken(const std::string& token);
        std::string GetToken() const;

        void SetAuthDetails(const std::string& dId, const std::string& lKey, const std::string& pHash);

        HttpResponse Request(
            const std::string& method,
            const std::string& path,
            const std::string& body = "",
            const std::map<std::string, std::string>& headers = {}
        );
    };

}
