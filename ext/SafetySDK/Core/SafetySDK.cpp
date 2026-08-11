#include "SafetySDK.h"
#include <windows.h>
#include <shellapi.h>
#include <ext/Auth/json.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

namespace SafetySDK {

    SafetySDKClient::SafetySDKClient(const std::wstring& domain, int port, bool useHttps)
        : client(domain, port, useHttps) {}

    SafetySDKClient::~SafetySDKClient() {}

    bool SafetySDKClient::Login(const std::string& licenseKey, const std::string& discordId, const std::string& productHash, std::string& outError) {
        try {
            DeviceFingerprint fp = DeviceCollector::Collect();
            std::string hwid = DeviceCollector::GenerateHWIDHash(fp);
            std::string hwidDetails = DeviceCollector::SerializeFingerprint(fp);

            json payload = {
                {"licenseKey", licenseKey},
                {"discordId", discordId},
                {"productHash", productHash},
                {"hwid", hwid},
                {"hwid_details", json::parse(hwidDetails)}
            };

            HttpResponse res = client.Request("POST", "/v1/auth/login", payload.dump());

            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    if (responseData["data"].contains("token") && !responseData["data"]["token"].is_null()) {
                        std::string token = responseData["data"]["token"].get<std::string>();
                        client.SetToken(token);
                    }
                    client.SetAuthDetails(discordId, licenseKey, productHash);
                    authenticated = true;

                    currentLicense.key = licenseKey;
                    if (responseData["data"].contains("expiresAt") && !responseData["data"]["expiresAt"].is_null()) {
                        currentLicense.expiresAt = responseData["data"]["expiresAt"].get<std::string>();
                    }

                    if (responseData["data"].contains("user") && !responseData["data"]["user"].is_null()) {
                        auto& jsUser = responseData["data"]["user"];
                        if (jsUser.contains("id") && !jsUser["id"].is_null()) {
                            currentUser.id = jsUser["id"].get<std::string>();
                        }
                        if (jsUser.contains("username") && !jsUser["username"].is_null()) {
                            currentUser.username = jsUser["username"].get<std::string>();
                        }
                        if (jsUser.contains("avatarUrl") && !jsUser["avatarUrl"].is_null()) {
                            currentUser.avatarUrl = jsUser["avatarUrl"].get<std::string>();
                        }
                    }

                    if (responseData["data"].contains("discordId") && !responseData["data"]["discordId"].is_null()) {
                        currentUser.discordId = responseData["data"]["discordId"].get<std::string>();
                    } else {
                        currentUser.discordId = discordId;
                    }

                    if (responseData["data"].contains("roles") && !responseData["data"]["roles"].is_null() && responseData["data"]["roles"].is_array() && !responseData["data"]["roles"].empty()) {
                        currentUser.role = responseData["data"]["roles"][0].get<std::string>();
                    } else if (responseData["data"].contains("user") && !responseData["data"]["user"].is_null() && responseData["data"]["user"].contains("role") && !responseData["data"]["user"]["role"].is_null()) {
                        currentUser.role = responseData["data"]["user"]["role"].get<std::string>();
                    }

                    if (responseData["data"].contains("user") && !responseData["data"]["user"].is_null() && responseData["data"]["user"].contains("permissions")) {
                        for (auto& perm : responseData["data"]["user"]["permissions"]) {
                            currentUser.permissions.push_back(perm.get<std::string>());
                        }
                    }

                    if (responseData["data"].contains("sessionId") && !responseData["data"]["sessionId"].is_null()) {
                        activeSessionId = responseData["data"]["sessionId"].get<std::string>();
                    }

                    return true;
                } else {
                    outError = responseData["message"].get<std::string>();
                }
            } else {
                if (!res.body.empty()) {
                    try {
                        json errData = json::parse(res.body);
                        outError = errData["message"].get<std::string>();
                    } catch (...) {
                        outError = "HTTP Error: " + std::to_string(res.statusCode);
                    }
                } else {
                    outError = "HTTP Error: " + std::to_string(res.statusCode);
                }
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::Register(const std::string& username, const std::string& discordId, const std::string& licenseKey, std::string& outError) {
        try {
            DeviceFingerprint fp = DeviceCollector::Collect();
            std::string hwid = DeviceCollector::GenerateHWIDHash(fp);
            std::string hwidDetails = DeviceCollector::SerializeFingerprint(fp);

            json payload = {
                {"username", username},
                {"discordId", discordId},
                {"licenseKey", licenseKey},
                {"hwid", hwid},
                {"hwid_details", json::parse(hwidDetails)}
            };

            HttpResponse res = client.Request("POST", "/v1/auth/register", payload.dump());

            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    return true;
                } else {
                    outError = responseData["message"].get<std::string>();
                }
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::Logout() {
        if (!authenticated) return true;
        try {
            client.Request("POST", "/v1/auth/logout");
        } catch (...) {}
        client.SetToken("");
        authenticated = false;
        activeSessionId = "";
        return true;
    }

    bool SafetySDKClient::RefreshSession(std::string& outError) {
        try {
            HttpResponse res = client.Request("POST", "/v1/auth/refresh");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    std::string token = responseData["data"]["token"].get<std::string>();
                    client.SetToken(token);
                    return true;
                } else {
                    outError = responseData["message"].get<std::string>();
                }
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::ValidateSession(std::string& outError) {
        try {
            DeviceFingerprint fp = DeviceCollector::Collect();
            std::string hwid = DeviceCollector::GenerateHWIDHash(fp);
            
            json payload = {
                {"sessionId", activeSessionId},
                {"hwid", hwid}
            };

            HttpResponse res = client.Request("POST", "/v1/client/heartbeat", payload.dump());
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    return true;
                } else {
                    outError = responseData["message"].get<std::string>();
                }
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::GetProfile(UserProfile& outProfile, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/profile");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    outProfile.id = responseData["data"]["id"].get<std::string>();
                    outProfile.username = responseData["data"]["username"].get<std::string>();
                    outProfile.discordId = responseData["data"]["discordId"].get<std::string>();
                    outProfile.role = responseData["data"]["role"].get<std::string>();
                    return true;
                } else {
                    outError = responseData["message"].get<std::string>();
                }
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::UpdateProfile(const std::string& username, std::string& outError) {
        try {
            json payload = { {"username", username} };
            HttpResponse res = client.Request("PATCH", "/v1/client/profile", payload.dump());
            if (res.statusCode == 200) {
                return true;
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    std::vector<char> SafetySDKClient::GetAvatar(const std::string& discordId, std::string& outError) {
        std::vector<char> buffer;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/avatar/" + discordId);
            if (res.statusCode == 200) {
                buffer.assign(res.body.begin(), res.body.end());
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return buffer;
    }

    bool SafetySDKClient::ActivateLicense(const std::string& key, std::string& outError) {
        try {
            json payload = { {"key", key} };
            HttpResponse res = client.Request("POST", "/v1/client/licenses/activate", payload.dump());
            if (res.statusCode == 200) {
                return true;
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::ValidateLicense(const std::string& key, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/licenses/validate?key=" + key);
            if (res.statusCode == 200) {
                return true;
            } else {
                outError = "HTTP Error: " + std::to_string(res.statusCode);
            }
        } catch (const std::exception& e) {
            outError = std::string("Exception: ") + e.what();
        }
        return false;
    }

    bool SafetySDKClient::GetLicense(const std::string& key, LicenseInfo& outLicense, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/licenses/" + key);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    outLicense.key = responseData["data"]["key"].get<std::string>();
                    outLicense.productId = responseData["data"]["productId"].get<std::string>();
                    outLicense.status = responseData["data"]["status"].get<std::string>();
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    std::vector<LicenseInfo> SafetySDKClient::GetLicenses(std::string& outError) {
        std::vector<LicenseInfo> licenses;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/licenses");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                for (auto& item : responseData["data"]) {
                    LicenseInfo lic;
                    lic.key = item["key"].get<std::string>();
                    lic.status = item["status"].get<std::string>();
                    licenses.push_back(lic);
                }
            }
        } catch (...) {}
        return licenses;
    }

    std::vector<ProductInfo> SafetySDKClient::GetProducts(std::string& outError) {
        std::vector<ProductInfo> products;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/products");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                for (auto& item : responseData["data"]) {
                    ProductInfo prod;
                    prod.id = item["id"].get<std::string>();
                    prod.name = item["name"].get<std::string>();
                    prod.hash = item["hash"].get<std::string>();
                    products.push_back(prod);
                }
            }
        } catch (...) {}
        return products;
    }

    bool SafetySDKClient::GetProduct(const std::string& id, ProductInfo& outProduct, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/products/" + id);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                outProduct.id = responseData["data"]["id"].get<std::string>();
                outProduct.name = responseData["data"]["name"].get<std::string>();
                return true;
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::GetProductByHash(const std::string& hash, ProductInfo& outProduct, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/products/hash/" + hash);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                outProduct.id = responseData["data"]["id"].get<std::string>();
                outProduct.name = responseData["data"]["name"].get<std::string>();
                return true;
            }
        } catch (...) {}
        return false;
    }

    std::vector<std::string> SafetySDKClient::GetRoles(std::string& outError) {
        return { currentUser.role };
    }

    bool SafetySDKClient::HasRole(const std::string& roleName) {
        return currentUser.role == roleName;
    }

    std::vector<std::string> SafetySDKClient::GetFeatures(std::string& outError) {
        return currentUser.permissions;
    }

    bool SafetySDKClient::HasFeature(const std::string& featureName) {
        for (const auto& f : currentUser.permissions) {
            if (f == featureName) return true;
        }
        return false;
    }

    bool SafetySDKClient::CheckForUpdates(const std::string& currentVersion, std::string& outDownloadUrl, std::string& outLatestVersion, bool& mandatory, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/updates?version=" + currentVersion);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>() && responseData["data"].contains("update")) {
                    outDownloadUrl = responseData["data"]["update"]["downloadUrl"].get<std::string>();
                    outLatestVersion = responseData["data"]["update"]["version"].get<std::string>();
                    mandatory = responseData["data"]["update"]["mandatory"].get<bool>();
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::DownloadUpdate(const std::string& downloadUrl, const std::wstring& savePath, std::function<void(double, double)> progressCallback) {
        // Mock download update or stream update via standard download helper
        return DownloadFile(downloadUrl, savePath, progressCallback);
    }

    bool SafetySDKClient::InstallUpdate(const std::wstring& updatePath) {
        // Runs external installer or shell processes
        ShellExecuteW(NULL, L"open", updatePath.c_str(), NULL, NULL, SW_SHOW);
        ExitProcess(0);
        return true;
    }

    bool SafetySDKClient::DownloadFile(const std::string& fileId, const std::wstring& savePath, std::function<void(double, double)> progressCallback) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/downloads/" + fileId);
            if (res.statusCode == 200) {
                std::ofstream file(savePath, std::ios::binary);
                if (file.is_open()) {
                    file.write(res.body.data(), res.body.size());
                    file.close();
                    if (progressCallback) {
                        progressCallback((double)res.body.size(), (double)res.body.size());
                    }
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    std::vector<char> SafetySDKClient::DownloadBinary(const std::string& binaryId, std::string& outError) {
        std::vector<char> buffer;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/downloads/" + binaryId);
            if (res.statusCode == 200) {
                buffer.assign(res.body.begin(), res.body.end());
            }
        } catch (...) {}
        return buffer;
    }

    bool SafetySDKClient::DownloadDll(const std::string& dllId, const std::wstring& savePath, std::function<void(double, double)> progressCallback) {
        return DownloadFile(dllId, savePath, progressCallback);
    }

    bool SafetySDKClient::GetVariable(const std::string& name, std::string& outValue, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/variables/" + name);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    outValue = responseData["data"]["value"].get<std::string>();
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::SetVariable(const std::string& name, const std::string& value, std::string& outError) {
        try {
            json payload = { {"value", value} };
            HttpResponse res = client.Request("POST", "/v1/client/variables/" + name, payload.dump());
            if (res.statusCode == 200) {
                return true;
            }
        } catch (...) {}
        return false;
    }

    std::vector<VariableInfo> SafetySDKClient::GetVariables(std::string& outError) {
        std::vector<VariableInfo> variables;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/variables");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                for (auto& item : responseData["data"]) {
                    VariableInfo var;
                    var.name = item["name"].get<std::string>();
                    var.value = item["value"].get<std::string>();
                    variables.push_back(var);
                }
            }
        } catch (...) {}
        return variables;
    }

    bool SafetySDKClient::GetConfig(const std::string& name, std::string& outData, std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/configs/" + name);
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                if (responseData["success"].get<bool>()) {
                    outData = responseData["data"]["configData"].dump();
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    std::vector<ConfigInfo> SafetySDKClient::GetConfigs(std::string& outError) {
        std::vector<ConfigInfo> configs;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/configs");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                for (auto& item : responseData["data"]) {
                    ConfigInfo conf;
                    conf.name = item["name"].get<std::string>();
                    configs.push_back(conf);
                }
            }
        } catch (...) {}
        return configs;
    }

    std::vector<NotificationInfo> SafetySDKClient::GetNotifications(std::string& outError) {
        std::vector<NotificationInfo> notifications;
        try {
            HttpResponse res = client.Request("GET", "/v1/client/notifications");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                for (auto& item : responseData["data"]) {
                    NotificationInfo notif;
                    notif.id = item["id"].get<std::string>();
                    notif.title = item["title"].get<std::string>();
                    notif.message = item["message"].get<std::string>();
                    notif.read = item["read"].get<bool>();
                    notifications.push_back(notif);
                }
            }
        } catch (...) {}
        return notifications;
    }

    bool SafetySDKClient::MarkAsRead(const std::string& notificationId, std::string& outError) {
        try {
            HttpResponse res = client.Request("POST", "/v1/client/notifications/" + notificationId + "/read");
            if (res.statusCode == 200) {
                return true;
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::CheckBlacklist(std::string& outError) {
        try {
            HttpResponse res = client.Request("GET", "/v1/client/blacklist/check");
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                return responseData["data"]["blacklisted"].get<bool>();
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::CreateSession(std::string& outError) {
        try {
            DeviceFingerprint fp = DeviceCollector::Collect();
            std::string hwid = DeviceCollector::GenerateHWIDHash(fp);
            std::string hwidDetails = DeviceCollector::SerializeFingerprint(fp);

            json payload = {
                {"hwid", hwid},
                {"hwid_details", json::parse(hwidDetails)}
            };

            HttpResponse res = client.Request("POST", "/v1/client/sessions", payload.dump());
            if (res.statusCode == 200) {
                json responseData = json::parse(res.body);
                activeSessionId = responseData["data"]["sessionId"].get<std::string>();
                return true;
            }
        } catch (...) {}
        return false;
    }

    bool SafetySDKClient::EndSession() {
        if (activeSessionId.empty()) return true;
        try {
            client.Request("DELETE", "/v1/client/sessions/" + activeSessionId);
        } catch (...) {}
        activeSessionId = "";
        return true;
    }

}
