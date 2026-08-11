#pragma once
#include "Includes.hpp"

class Utils {
public:
    static std::wstring RandomString(size_t Length);
    static ImVec2 CalcTextSize(ImFont* font, int size, const char* label);
    static bool IsProcessElevated();
    static bool EnableDebugPrivilege();
    static bool DisableDebugPrivilege();
};

class Console {
public:
    static void InitConsole();
    static void ShutdownConsole();

    //template<typename T>
    //static void Log(const T& message) {
    //    if constexpr (std::is_same_v<T, std::wstring>) {
    //        std::wcout << message << std::endl;
    //    }
    //    else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
    //        std::cout << std::to_string(message) << std::endl;
    //    }
    //    else {
    //        std::cout << message << std::endl;
    //    }
    //}

    //template<typename T, typename... Args>
    //static void Log(const T& first, const Args&... args) {
    //    if constexpr (std::is_same_v<T, std::wstring>) {
    //        std::wcout << first;
    //    }
    //    else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
    //        std::cout << std::to_string(first);
    //    }
    //    else {
    //        std::cout << first;
    //    }
    //    Log(args...);
    //}

    //template<typename T>
    //static void LogHex(const char* prefix, T value) {
    //    std::ostringstream oss;
    //    oss << prefix << "0x" << std::hex << (uint64_t)value;
    //    Log(oss.str());
    //}

    template<typename T>
    static void Log(const T& message) {
#ifdef _DEBUG
        if constexpr (std::is_same_v<T, std::wstring>) {
            std::wcout << message << std::endl;
        }
        else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
            std::cout << std::to_string(message) << std::endl;
        }
        else {
            std::cout << message << std::endl;
        }
#endif
    }

    template<typename T, typename... Args>
    static void Log(const T& first, const Args&... args) {
#ifdef _DEBUG
        if constexpr (std::is_same_v<T, std::wstring>) {
            std::wcout << first;
        }
        else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
            std::cout << std::to_string(first);
        }
        else {
            std::cout << first;
        }
        Log(args...);
#endif
    }

    template<typename T>
    static void LogHex(const char* prefix, T value) {
#ifdef _DEBUG
        std::ostringstream oss;
        oss << prefix << "0x" << std::hex << (uint64_t)value;
        Log(oss.str());
#endif
    }

};
