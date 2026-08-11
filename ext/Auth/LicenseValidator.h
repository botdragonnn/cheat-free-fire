#ifndef LICENSE_VALIDATOR_H
#define LICENSE_VALIDATOR_H

#include <windows.h>
#include <intrin.h>
#include <string>
#include <vector>
#include <ctime>
#include <cstdio>

// --- Base64 Decode fiel ao OpenSSL (sem OpenSSL) ---
inline std::vector<unsigned char> base64Decode(const std::string& input) {
    static const unsigned char decodeTable[256] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    };

    std::vector<unsigned char> out;
    out.reserve(input.size() * 3 / 4);

    unsigned int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (c > 127) continue;
        unsigned char d = decodeTable[c];
        if (d == 64) {
            if (c == '=') break;
            if (c == '\n' || c == '\r') continue;
            continue;
        }
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// --- XOR simples ---
inline std::string xorDecrypt(const std::vector<unsigned char>& data) {
    if (data.size() <= 16) return {};

    unsigned char key[16];
    memcpy(key, data.data(), 16);

    size_t cipherLen = data.size() - 16;
    const unsigned char* cipher = data.data() + 16;
    std::vector<unsigned char> plain(cipherLen);

    for (size_t i = 0; i < cipherLen; ++i) {
        plain[i] = cipher[i] ^ key[i % 16];
    }
    return std::string(reinterpret_cast<char*>(plain.data()), cipherLen);
}

// --- Parser de string UTC "YYYY-MM-DDTHH:MM:SS" ---
inline std::time_t ParseUtcTime(const std::string& str) {
    if (str.size() < 19) return -1;

    std::tm tm{};
    try {
        tm.tm_year = std::stoi(str.substr(0, 4)) - 1900;
        tm.tm_mon = std::stoi(str.substr(5, 2)) - 1;
        tm.tm_mday = std::stoi(str.substr(8, 2));
        tm.tm_hour = std::stoi(str.substr(11, 2));
        tm.tm_min = std::stoi(str.substr(14, 2));
        tm.tm_sec = std::stoi(str.substr(17, 2));
    }
    catch (...) {
        return -1;
    }
    return _mkgmtime(&tm);
}

// --- HWID simples (Volume Serial + CPUID) ---
inline std::string GetHardwareId() {
    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &volSerial, nullptr, nullptr, nullptr, 0);

    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 1);
    DWORD cpuSig = static_cast<DWORD>(cpuInfo[0]);

    char buf[64];
    sprintf_s(buf, "%08X-%08X", volSerial, cpuSig);
    return std::string(buf);
}

// --- Validacao principal ---
inline bool ValidateLicense(const std::string& licenseB64, bool& outIsPremium) {
    auto data = base64Decode(licenseB64);
    if (data.empty()) return false;

    auto payload = xorDecrypt(data);
    if (payload.empty()) return false;

    auto p1 = payload.find(':');
    auto p2 = payload.rfind(':');
    if (p1 == std::string::npos || p2 == std::string::npos || p1 == p2) return false;

    std::string hwidStr = payload.substr(0, p1);
    std::string expiryStr = payload.substr(p1 + 1, p2 - p1 - 1);
    std::string premStr = payload.substr(p2 + 1);

    outIsPremium = (premStr == "True");

    std::time_t expiry = ParseUtcTime(expiryStr);
    std::time_t now = std::time(nullptr);
    if (expiry < 0 || now > expiry) return false;

    auto currentHwid = GetHardwareId();
    if (hwidStr != currentHwid) return false;

    return true;
}

#endif // LICENSE_VALIDATOR_H
