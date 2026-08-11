#pragma once
#include <windows.h>
#include <string>

namespace SafetySDK {

    struct DeviceFingerprint {
        std::string cpuId;
        std::string motherboardSerial;
        std::string biosSerial;
        std::string diskSerial;
        std::string machineGuid;
        std::string systemUuid;
    };

    class DeviceCollector {
    private:
        static std::string QueryWMI(const std::wstring& wmiClass, const std::wstring& wmiProperty);
        static std::string GetRegistryValue(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName);

    public:
        static DeviceFingerprint Collect();
        static std::string GenerateHWIDHash(const DeviceFingerprint& fp);
        static std::string SerializeFingerprint(const DeviceFingerprint& fp);
    };

}
