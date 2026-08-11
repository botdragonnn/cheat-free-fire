#pragma once
#include <string>

namespace SafetySDK {

    class SecurityManager {
    private:
        static std::string hmacKey;

    public:
        static void SetHmacKey(const std::string& key);
        static std::string CalculateSignature(const std::string& data, const std::string& nonce, long long timestamp);
        static bool VerifySignature(const std::string& data, const std::string& nonce, long long timestamp, const std::string& expectedSig);
        static bool CheckMemoryIntegrity();
        static std::string GenerateNonce();
        static long long GetCurrentTimestamp();
    };

}
