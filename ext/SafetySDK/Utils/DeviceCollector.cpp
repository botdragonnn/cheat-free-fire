#include "DeviceCollector.h"
#include <windows.h>
#include <comdef.h>
#include <wbemidl.h>
#include <bcrypt.h>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "bcrypt.lib")

namespace SafetySDK {

    // Helper to trim strings
    static std::string TrimCopy(const std::string& str) {
        if (str.empty()) return "";
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    std::string DeviceCollector::QueryWMI(const std::wstring& wmiClass, const std::wstring& wmiProperty) {
        std::string result = "";
        
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
        bool coInit = SUCCEEDED(hr);
        
        IWbemLocator* pLoc = nullptr;
        IWbemServices* pSvc = nullptr;

        hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (SUCCEEDED(hr)) {
            hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
            if (SUCCEEDED(hr)) {
                CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

                IEnumWbemClassObject* pEnumerator = nullptr;
                std::wstring wql = L"SELECT " + wmiProperty + L" FROM " + wmiClass;
                
                hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql.c_str()),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

                if (SUCCEEDED(hr) && pEnumerator) {
                    IWbemClassObject* pObj = nullptr;
                    ULONG ret = 0;
                    if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &ret) == S_OK && pObj) {
                        VARIANT vt{};
                        VariantInit(&vt);
                        if (SUCCEEDED(pObj->Get(wmiProperty.c_str(), 0, &vt, 0, 0))) {
                            if (vt.vt == VT_BSTR && vt.bstrVal) {
                                _bstr_t b(vt.bstrVal);
                                result = (const char*)b;
                            }
                        }
                        VariantClear(&vt);
                        pObj->Release();
                    }
                    pEnumerator->Release();
                }
                pSvc->Release();
            }
            pLoc->Release();
        }

        if (coInit) {
            CoUninitialize();
        }
        
        return TrimCopy(result);
    }

    std::string DeviceCollector::GetRegistryValue(HKEY hKeyParent, const std::wstring& subKey, const std::wstring& valueName) {
        HKEY hKey;
        std::string result = "";
        if (RegOpenKeyExW(hKeyParent, subKey.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            wchar_t value[256] = { 0 };
            DWORD size = sizeof(value) - sizeof(wchar_t);
            if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, (LPBYTE)value, &size) == ERROR_SUCCESS) {
                _bstr_t b(value);
                result = (const char*)b;
            }
            RegCloseKey(hKey);
        }
        return TrimCopy(result);
    }

    DeviceFingerprint DeviceCollector::Collect() {
        DeviceFingerprint fp;
        fp.motherboardSerial = QueryWMI(L"Win32_BaseBoard", L"SerialNumber");
        fp.cpuId = QueryWMI(L"Win32_Processor", L"ProcessorId");
        fp.biosSerial = QueryWMI(L"Win32_BIOS", L"SerialNumber");
        fp.diskSerial = QueryWMI(L"Win32_PhysicalMedia", L"SerialNumber");
        fp.systemUuid = QueryWMI(L"Win32_ComputerSystemProduct", L"UUID");
        fp.machineGuid = GetRegistryValue(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");
        return fp;
    }

    std::string DeviceCollector::GenerateHWIDHash(const DeviceFingerprint& fp) {
        std::string raw = fp.cpuId + "|" + fp.motherboardSerial + "|" + fp.biosSerial + "|" + fp.diskSerial + "|" + fp.machineGuid + "|" + fp.systemUuid;
        raw = TrimCopy(raw);

        // Compute SHA-256 hash using BCrypt
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        DWORD cbHashObject = 0, cbHash = 0, cbData = 0;
        std::vector<BYTE> pbHashObject;
        std::vector<BYTE> pbHash;

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
            return "unknown";
        }

        if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "unknown";
        }

        pbHashObject.resize(cbHashObject);
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "unknown";
        }

        pbHash.resize(cbHash);
        if (BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "unknown";
        }

        if (BCryptHashData(hHash, (PBYTE)raw.c_str(), (ULONG)raw.size(), 0) != 0) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "unknown";
        }

        if (BCryptFinishHash(hHash, pbHash.data(), cbHash, 0) != 0) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "unknown";
        }

        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::stringstream ss;
        for (BYTE b : pbHash) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        }
        return ss.str();
    }

    std::string DeviceCollector::SerializeFingerprint(const DeviceFingerprint& fp) {
        // Build JSON representation
        std::stringstream ss;
        ss << "{"
           << "\"cpuId\":\"" << fp.cpuId << "\","
           << "\"motherboardSerial\":\"" << fp.motherboardSerial << "\","
           << "\"biosSerial\":\"" << fp.biosSerial << "\","
           << "\"diskSerial\":\"" << fp.diskSerial << "\","
           << "\"machineGuid\":\"" << fp.machineGuid << "\","
           << "\"systemUuid\":\"" << fp.systemUuid << "\""
           << "}";
        return ss.str();
    }

}
