#include "SecurityManager.h"
#include <windows.h>
#include <bcrypt.h>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace SafetySDK {

    std::string SecurityManager::hmacKey = "SafetyAPI-HMAC-Default-Key-2026";

    void SecurityManager::SetHmacKey(const std::string& key) {
        hmacKey = key;
    }

    std::string SecurityManager::GenerateNonce() {
        static std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, (int)chars.size() - 1);
        
        std::string nonce = "";
        for (int i = 0; i < 16; ++i) {
            nonce += chars[distribution(generator)];
        }
        return nonce;
    }

    long long SecurityManager::GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    std::string SecurityManager::CalculateSignature(const std::string& data, const std::string& nonce, long long timestamp) {
        std::stringstream ss;
        ss << data << ":" << nonce << ":" << timestamp;
        std::string message = ss.str();

        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        DWORD cbHashObject = 0, cbHash = 0, cbData = 0;
        std::vector<BYTE> pbHashObject;
        std::vector<BYTE> pbHash;

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
            return "";
        }

        if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        pbHashObject.resize(cbHashObject);
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        pbHash.resize(cbHash);
        if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, (PBYTE)hmacKey.c_str(), (ULONG)hmacKey.size(), 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        if (BCryptHashData(hHash, (PBYTE)message.c_str(), (ULONG)message.size(), 0) != 0) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) != 0) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::stringstream result;
        for (BYTE b : pbHash) {
            result << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        return result.str();
    }

    bool SecurityManager::VerifySignature(const std::string& data, const std::string& nonce, long long timestamp, const std::string& expectedSig) {
        std::string calculated = CalculateSignature(data, nonce, timestamp);
        return !calculated.empty() && calculated == expectedSig;
    }

    bool SecurityManager::CheckMemoryIntegrity() {
        // Basic detection of breakpoints or code alterations in memory space (mocked checking function headers)
        unsigned char* codePtr = (unsigned char*)&SecurityManager::CalculateSignature;
        if (codePtr[0] == 0xCC) {
            // INT 3 breakpoint detected
            return false;
        }
        return true;
    }

}
