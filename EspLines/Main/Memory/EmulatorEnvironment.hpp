#pragma once
#include <string>
#include <cstdint>

enum class ABIType { Unknown, X86_64, X86 };
enum class EmulatorType { Unknown, Msi4, BlueStacks4, BlueStacks5, BlueStacks5Beta, Bluestacks5China, Msi5 };

class EmulatorEnvironment {
public:
    void DetectEmulator();
    void QueryVersion();
    void DetectABI();
    void Refresh();

    EmulatorType Emulator() const { return emulator_; }
    ABIType Abi() const { return abi_; }
    const std::string& Version() const { return version_; }

    std::string EmulatorName() const;
    static const char* ABIToString(ABIType abi);

private:
    EmulatorType emulator_ = EmulatorType::Unknown;
    ABIType abi_ = ABIType::Unknown;
    std::string version_;
};

class KernelOffsetSelector {
public:
    static bool ApplyInitTaskOffsets(const EmulatorEnvironment& env, uint32_t& kva_init_task_32, uint64_t& kva_init_task_64);
};

EmulatorEnvironment& GetEmulatorEnv();