#pragma once
#include <vector>
#include <string>
#include <unordered_map>

class Namegun {
public:
    struct GunInfo {
        std::string name;
        std::string icon;
        bool isSpecial = false;
        bool hasLevels = false;
    };

    static void Init();
    static std::string GetGunName(int gunId);
    static std::string GetGunIcon(int gunId);
    static std::string GetBaseName(const std::string& fullName);
    static bool HasIcon(int gunId);

private:
    static std::vector<GunInfo> GunData;
};