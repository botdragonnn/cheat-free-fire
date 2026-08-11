#pragma once
#include "../Networking/HttpClient.h"
#include "../Utils/DeviceCollector.h"
#include "../Security/SecurityManager.h"
#include <string>
#include <vector>
#include <functional>

namespace SafetySDK {

    struct UserProfile {
        std::string id;
        std::string username;
        std::string discordId;
        std::string role;
        std::string avatarUrl;
        std::vector<std::string> permissions;
    };

    struct ProductInfo {
        std::string id;
        std::string name;
        std::string hash;
        bool hwidLock = false;
    };

    struct LicenseInfo {
        std::string key;
        std::string productId;
        std::string status;
        std::string expiresAt;
        std::string activatedAt;
        bool paused = false;
        bool banned = false;
    };

    struct VariableInfo {
        std::string name;
        std::string value;
    };

    struct ConfigInfo {
        std::string name;
        std::string data;
    };

    struct NotificationInfo {
        std::string id;
        std::string title;
        std::string message;
        bool read = false;
    };

    class SafetySDKClient {
    private:
        HttpClient client;
        UserProfile currentUser;
        LicenseInfo currentLicense;
        std::string activeSessionId;
        bool authenticated = false;

    public:
        SafetySDKClient(const std::wstring& domain, int port = 443, bool useHttps = true);
        ~SafetySDKClient();

        // Authentication
        bool Login(const std::string& licenseKey, const std::string& discordId, const std::string& productHash, std::string& outError);
        bool Register(const std::string& username, const std::string& discordId, const std::string& licenseKey, std::string& outError);
        bool Logout();
        bool RefreshSession(std::string& outError);
        bool ValidateSession(std::string& outError);

        // Users
        bool GetProfile(UserProfile& outProfile, std::string& outError);
        bool UpdateProfile(const std::string& username, std::string& outError);
        std::vector<char> GetAvatar(const std::string& discordId, std::string& outError);

        // Licenses
        bool ActivateLicense(const std::string& key, std::string& outError);
        bool ValidateLicense(const std::string& key, std::string& outError);
        bool GetLicense(const std::string& key, LicenseInfo& outLicense, std::string& outError);
        std::vector<LicenseInfo> GetLicenses(std::string& outError);

        // Products
        std::vector<ProductInfo> GetProducts(std::string& outError);
        bool GetProduct(const std::string& id, ProductInfo& outProduct, std::string& outError);
        bool GetProductByHash(const std::string& hash, ProductInfo& outProduct, std::string& outError);

        // Roles & Features
        std::vector<std::string> GetRoles(std::string& outError);
        bool HasRole(const std::string& roleName);
        std::vector<std::string> GetFeatures(std::string& outError);
        bool HasFeature(const std::string& featureName);

        // Updates
        bool CheckForUpdates(const std::string& currentVersion, std::string& outDownloadUrl, std::string& outLatestVersion, bool& mandatory, std::string& outError);
        bool DownloadUpdate(const std::string& downloadUrl, const std::wstring& savePath, std::function<void(double, double)> progressCallback);
        bool InstallUpdate(const std::wstring& updatePath);

        // Downloads / Files
        bool DownloadFile(const std::string& fileId, const std::wstring& savePath, std::function<void(double, double)> progressCallback);
        std::vector<char> DownloadBinary(const std::string& binaryId, std::string& outError);
        bool DownloadDll(const std::string& dllId, const std::wstring& savePath, std::function<void(double, double)> progressCallback);

        // Variables
        bool GetVariable(const std::string& name, std::string& outValue, std::string& outError);
        bool SetVariable(const std::string& name, const std::string& value, std::string& outError);
        std::vector<VariableInfo> GetVariables(std::string& outError);

        // Configs
        bool GetConfig(const std::string& name, std::string& outData, std::string& outError);
        std::vector<ConfigInfo> GetConfigs(std::string& outError);

        // Notifications
        std::vector<NotificationInfo> GetNotifications(std::string& outError);
        bool MarkAsRead(const std::string& notificationId, std::string& outError);

        // Blacklist & Sessions
        bool CheckBlacklist(std::string& outError);
        bool CreateSession(std::string& outError);
        bool EndSession();

        // Getters
        bool IsAuthenticated() const { return authenticated; }
        std::string GetActiveSessionId() const { return activeSessionId; }
        UserProfile GetCurrentUser() const { return currentUser; }
        LicenseInfo GetCurrentLicense() const { return currentLicense; }
    };

}
