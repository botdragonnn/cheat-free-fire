#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <iostream>
#include <string>
#include <vector>
#include <cwchar>
#include <cwctype>
#include <filesystem>

// Aplicativo silencioso (sem console): status so via OutputDebugString
// (invisivel sem debugger), erros via popup.
static void Say(const std::wstring& s)
{
    OutputDebugStringW((s + L"\n").c_str());
}

static void Fail(const std::wstring& s)
{
    MessageBoxW(nullptr, s.c_str(), L"Hardware Monitor", MB_OK | MB_ICONERROR);
    Say(s);
}

static void EnableDebugPrivilege()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return;

    LUID luid{};
    if (LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid))
    {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }

    CloseHandle(hToken);
}

static std::wstring ExecutableDirectory()
{
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return p.parent_path().wstring();
}

static std::wstring FindDllPath()
{
    std::wstring exeDir = ExecutableDirectory();

    std::vector<std::wstring> candidates;
    candidates.push_back(exeDir + L"\\HwMonCore.dll");

    // Sobe os diretorios-pai a partir da pasta do exe procurando por
    // "x64\Release\HwMonCore.dll" (ex.: Loader\x64\Release -> raiz do projeto).
    std::filesystem::path dir(exeDir);
    for (int i = 0; i < 6; ++i)
    {
        std::filesystem::path p = dir / L"x64" / L"Release" / L"HwMonCore.dll";
        candidates.push_back(p.wstring());
        dir = dir.parent_path();
    }

    for (const auto& c : candidates)
    {
        if (std::filesystem::exists(c))
            return c;
    }

    return L"";
}

static std::wstring RandomFileName()
{
    std::wstring name = L"~Z";
    for (int i = 0; i < 12; ++i)
        name += (wchar_t)(L'a' + (GetTickCount64() % 26));
    name += L".dll";
    return name;
}

static std::wstring CopyToTemp(const std::wstring& source)
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);

    std::wstring dest = std::wstring(tmp) + RandomFileName();
    if (!CopyFileW(source.c_str(), dest.c_str(), FALSE))
        return L"";
    return dest;
}

static std::wstring LocalAppData()
{
    wchar_t buf[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) != S_OK)
        return L"";
    return std::wstring(buf);
}

static void RemoveCheatConfig()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    std::filesystem::path dir = std::filesystem::path(base) / L"HwMon";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// Sobrescreve o conteudo do arquivo (2 passadas: zeros + 0xFF) ANTES de
// deletar — impede recuperacao com ferramentas de undelete/recovery.
static void SecureDeleteFile(const std::wstring& path)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size{};
        if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (1LL << 30))
        {
            std::vector<char> zeros(1 << 20, 0);
            std::vector<char> ffs(1 << 20, (char)0xFF);

            for (int pass = 0; pass < 2; ++pass)
            {
                const char* pat = pass == 0 ? zeros.data() : ffs.data();
                LARGE_INTEGER pos{};
                SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);

                LONGLONG remaining = size.QuadPart;
                while (remaining > 0)
                {
                    DWORD chunk = (DWORD)min((LONGLONG)(1 << 20), remaining);
                    DWORD written = 0;
                    if (!WriteFile(h, pat, chunk, &written, nullptr) || written == 0)
                        break;
                    remaining -= written;
                }
                FlushFileBuffers(h);
            }
        }
        CloseHandle(h);
    }
    DeleteFileW(path.c_str());
}

// Remove recursivamente uma pasta sobrescrevendo todos os arquivos antes.
static void SecureRemoveAll(const std::wstring& dirPath)
{
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec))
        return;

    for (auto it = std::filesystem::recursive_directory_iterator(dirPath, ec);
         it != std::filesystem::recursive_directory_iterator(); ++it)
    {
        if (it->is_regular_file(ec))
            SecureDeleteFile(it->path().wstring());
    }

    std::filesystem::remove_all(dirPath, ec);
}

static void CleanupKnownFiles(const std::wstring& dir)
{
    // Remove apenas arquivos conhecidos do cheat — nunca toca em pastas ou
    // arquivos arbitrarios (se o usuario copiou o exe para outro lugar).
    const wchar_t* knownFiles[] = {
        L"HwMonCore.dll", L"HwMonCore.pdb", L"HwMonCore.map", L"HwMonCore.iobj",
        L"HwMonCore.ipdb", L"HardwareMonitor.pdb", L"HardwareMonitor.exe.recipe",
        L"HardwareMonitor.iobj", L"HardwareMonitor.ipdb", L"HardwareMonitor.map",
        L"main.obj", L"vc143.pdb",
        L"ZmInternal.dll", L"ZmInternal.pdb", L"ZmInternal.map", L"ZmInternal.iobj",
        L"ZmInternal.ipdb", L"ZmLoader.pdb", L"ZmLoader.exe.recipe", L"ZmLoader.iobj",
        L"ZmLoader.ipdb", L"ZmLoader.map",
        L"ZmLoader.vcxproj.FileListAbsolute.txt",
    };
    for (auto* f : knownFiles)
        SecureDeleteFile(dir + L"\\" + f);
}

static void CleanupRegistry()
{
    // Chave de protocolo do Discord RPC que versoes antigas da DLL criavam.
    // autoRegister agora e 0 (nada e criado), mas remove qualquer chave que
    // tenha sobrado de sessoes anteriores.
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\discord-1439325737629913380");
}

static bool ContainsCI(const std::wstring& hay, const std::wstring& needle)
{
    if (needle.empty()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i)
    {
        size_t j = 0;
        while (j < needle.size() && towlower(hay[i + j]) == towlower(needle[j])) ++j;
        if (j == needle.size()) return true;
    }
    return false;
}

static std::wstring CurrentExeName()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeName = exePath;
    auto slash = exeName.find_last_of(L'\\');
    if (slash != std::wstring::npos) exeName = exeName.substr(slash + 1);
    return exeName;
}

// Remove apenas as entradas do historico "Executar" que apontam para o exe
// do cheat (nunca as de outros programas).
static void CleanupRunMRU()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU",
                      0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    std::wstring exeName = CurrentExeName();
    std::vector<std::wstring> toDelete;

    for (DWORD i = 0; ; ++i)
    {
        wchar_t valueName[64]{};
        DWORD nameLen = 64;
        wchar_t data[1024]{};
        DWORD dataLen = sizeof(data);
        DWORD type = 0;

        LONG r = RegEnumValueW(hKey, i, valueName, &nameLen, nullptr, &type,
                               reinterpret_cast<BYTE*>(data), &dataLen);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (type != REG_SZ) continue;
        if (_wcsicmp(valueName, L"MRUList") == 0) continue;

        std::wstring s(data, dataLen / sizeof(wchar_t));
        if (ContainsCI(s, exeName))
            toDelete.push_back(valueName);
    }

    if (toDelete.empty())
    {
        RegCloseKey(hKey);
        return;
    }

    // Ajusta a MRUList removendo as letras das entradas apagadas
    wchar_t mruBuf[64]{};
    DWORD sz = sizeof(mruBuf);
    if (RegQueryValueExW(hKey, L"MRUList", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(mruBuf), &sz) == ERROR_SUCCESS)
    {
        std::wstring filtered;
        for (const wchar_t* p = mruBuf; *p; ++p)
        {
            bool keep = true;
            for (const auto& v : toDelete)
            {
                if (!v.empty() && v[0] == *p) { keep = false; break; }
            }
            if (keep) filtered += *p;
        }

        if (filtered.empty())
            RegDeleteValueW(hKey, L"MRUList");
        else
            RegSetValueExW(hKey, L"MRUList", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(filtered.c_str()),
                           (DWORD)((filtered.size() + 1) * sizeof(wchar_t)));
    }

    for (const auto& v : toDelete)
        RegDeleteValueW(hKey, v.c_str());

    RegCloseKey(hKey);
}

// Remove apenas as entradas de "programas executados" (UserAssist) cujo nome
// decodificado (ROT13) referencia o exe do cheat.
static void CleanupUserAssist()
{
    HKEY hRoot = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist",
                      0, KEY_READ, &hRoot) != ERROR_SUCCESS)
        return;

    std::wstring exeName = CurrentExeName();

    for (DWORD i = 0; ; ++i)
    {
        wchar_t guid[64]{};
        DWORD guidLen = 64;
        if (RegEnumKeyExW(hRoot, i, guid, &guidLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        std::wstring countPath =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist\\" +
            std::wstring(guid) + L"\\Count";

        HKEY hCount = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, countPath.c_str(), 0, KEY_READ | KEY_WRITE, &hCount) != ERROR_SUCCESS)
            continue;

        std::vector<std::wstring> toDelete;
        for (DWORD v = 0; ; ++v)
        {
            wchar_t vName[512]{};
            DWORD vLen = 512;
            if (RegEnumValueW(hCount, v, vName, &vLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            // Valores UserAssist tem o nome ROT13 do caminho (UEME_...)
            std::wstring decoded = vName;
            for (auto& c : decoded)
            {
                if (c >= L'A' && c <= L'Z') c = (wchar_t)(L'A' + ((c - L'A' + 13) % 26));
                else if (c >= L'a' && c <= L'z') c = (wchar_t)(L'a' + ((c - L'a' + 13) % 26));
            }

            if (ContainsCI(decoded, exeName))
                toDelete.push_back(vName);
        }

        for (const auto& v : toDelete)
            RegDeleteValueW(hCount, v.c_str());

        RegCloseKey(hCount);
    }

    RegCloseKey(hRoot);
}

// Remove os arquivos de Prefetch do exe (ZMLOADER.EXE-*.pf). Se falhar por
// estar em uso, o cmd atrasado de SelfDeleteExe remove apos o processo sair.
static void CleanupPrefetch()
{
    wchar_t winDir[MAX_PATH]{};
    if (!GetWindowsDirectoryW(winDir, MAX_PATH)) return;

    std::wstring pfBase = CurrentExeName();
    for (auto& c : pfBase) c = (wchar_t)towupper(c);

    std::wstring dir = std::wstring(winDir) + L"\\Prefetch\\";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW((dir + pfBase + L"-*.pf").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        DeleteFileW((dir + fd.cFileName).c_str());
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

static void SelfDeleteExe()
{
    std::wstring exeDir = ExecutableDirectory();

    // Se a pasta do exe for a pasta de build deste projeto (marcador do
    // vcxproj presente), limpa tambem os artefatos da DLL na raiz do projeto
    // (x64\Release) e agenda a remocao da pasta do exe inteira.
    // OBS: o check tem que vir ANTES do CleanupKnownFiles — o marcador
    // FileListAbsolute esta na lista de arquivos conhecidos e seria deletado,
    // derrubando o check e impedindo o rmdir da pasta.
    bool isProjectBuildDir = false;
    std::error_code ec;
    if (std::filesystem::exists(exeDir + L"\\ZmLoader.vcxproj.FileListAbsolute.txt", ec))
        isProjectBuildDir = true;

    // Artefatos do cheat na pasta do exe (dll, pdb, obj, map, tlog...)
    CleanupKnownFiles(exeDir);

    // Log diagnostico criado pela DLL no TEMP
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    DeleteFileW((std::wstring(tmp) + L"HwMon.log").c_str());

    if (isProjectBuildDir)
    {
        std::filesystem::path p(exeDir);
        CleanupKnownFiles((p.parent_path().parent_path() / L"x64" / L"Release").wstring());
    }

    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring pfBase = CurrentExeName();
    for (auto& c : pfBase) c = (wchar_t)towupper(c);

    // Auto-delecao do exe em background. "cd /d %TEMP%" para o cmd nao ficar
    // com a pasta do exe como CWD (senao o rmdir falharia por pasta em uso).
    // O PowerShell roda DEPOIS do processo sair e sobrescreve o exe e os
    // Prefetch (2 passadas: zeros + 0xFF) antes do del — o exe em execucao
    // so pode ser sobrescrito apos o processo terminar.
    std::wstring ps =
        L"powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        L"try{$fs=[IO.File]::Open('" + std::wstring(exePath) + L"',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite);"
        L"$n=$fs.Length;"
        L"for($k=0;$k -lt 2;$k++){"
        L"$fs.Position=0;"
        L"$b=New-Object byte[] 1048576;"
        L"if($k){for($i=0;$i -lt $b.Length;$i++){$b[$i]=255}}"
        L"$left=$n;"
        L"while($left){$c=[Math]::Min($b.Length,$left);$fs.Write($b,0,$c);$left-=$c}"
        L"$fs.Flush()"
        L"}"
        L"$fs.Close()}catch{};"
        L"Get-ChildItem -Path $env:WINDIR\\Prefetch -Filter '" + pfBase + L"-*.pf' -ErrorAction SilentlyContinue | ForEach-Object {"
        L"try{$f=[IO.File]::Open($_.FullName,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite);$m=$f.Length;"
        L"for($k=0;$k -lt 2;$k++){$f.Position=0;$b=New-Object byte[] 1048576;if($k){for($i=0;$i -lt $b.Length;$i++){$b[$i]=255}}"
        L"$left=$m;while($left){$c=[Math]::Min($b.Length,$left);$f.Write($b,0,$c);$left-=$c}$f.Flush()}$f.Close()}catch{};"
        L"Remove-Item -Force -LiteralPath $_.FullName -ErrorAction SilentlyContinue"
        L"}\"";

    std::wstring cmd = L"cmd.exe /c cd /d \"%TEMP%\" & ping -n 3 127.0.0.1 >nul & " + ps +
                       L" & del /f /q \"" + std::wstring(exePath) + L"\"" +
                       L" & del /f /q \"%WINDIR%\\Prefetch\\" + pfBase + L"-*.pf\"";
    if (isProjectBuildDir)
        cmd += L" & rmdir /s /q \"" + exeDir + L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

static DWORD FindTargetPidByName(const std::wstring& name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
    {
        std::wstring processName = pe.szExeFile;
        for (auto& c : processName)
            c = (wchar_t)towlower(c);
        std::wstring lowerName = name;
        for (auto& c : lowerName)
            c = (wchar_t)towlower(c);

        if (processName == lowerName)
        {
            CloseHandle(snap);
            return pe.th32ProcessID;
        }
    }

    CloseHandle(snap);
    return 0;
}

static bool ProcessHasModule(DWORD pid, const wchar_t* moduleName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
    {
        if (_wcsicmp(me.szModule, moduleName) == 0)
        {
            CloseHandle(snap);
            return true;
        }
    }

    CloseHandle(snap);
    return false;
}

static DWORD FindTargetPid()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
    {
        if (pe.th32ProcessID == GetCurrentProcessId())
            continue;

        if (ProcessHasModule(pe.th32ProcessID, L"BstkVMM.dll"))
        {
            CloseHandle(snap);
            return pe.th32ProcessID;
        }
    }

    CloseHandle(snap);
    return 0;
}

static bool InjectDll(DWORD pid, const std::wstring& dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess)
        return false;

    size_t pathSize = (dllPath.size() + 1) * sizeof(wchar_t);

    void* remotePath = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath)
    {
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remotePath, dllPath.c_str(), pathSize, nullptr))
    {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    LPTHREAD_START_ROUTINE pLoadLibraryW =
        reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(hKernel, "LoadLibraryW"));
    if (!pLoadLibraryW)
    {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibraryW, remotePath, 0, nullptr);
    if (!hThread)
    {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return exitCode != 0;
}

static int PrintUsage(const std::wstring& exeName)
{
    Say(L"Uso: " + exeName + L" [opcoes]");
    Say(L"  -name <processo>   injeta no processo pelo nome do exe");
    Say(L"  -pid <id>          injeta no processo pelo PID");
    Say(L"  (sem opcoes)       detecta automaticamente o processo com BstkVMM.dll");
    return 1;
}

int wmain(int argc, wchar_t* argv[])
{
    EnableDebugPrivilege();

    std::wstring dllSource = FindDllPath();
    if (dllSource.empty())
    {
        Fail(L"ERRO: HwMonCore.dll nao encontrada (procure ao lado do exe ou em ..\\x64\\Release\\).");
        return 1;
    }

    std::wstring dllPath = CopyToTemp(dllSource);
    if (dllPath.empty())
    {
        Fail(L"ERRO: falha ao copiar DLL para temp.");
        return 1;
    }

    HANDLE hUnloadEvent = CreateEventW(nullptr, TRUE, FALSE, L"HwMonEvt");
    if (!hUnloadEvent)
    {
        DeleteFileW(dllPath.c_str());
        Fail(L"ERRO: falha ao criar evento de unload.");
        return 1;
    }

    DWORD targetPid = 0;
    std::wstring byName;

    for (int i = 1; i < argc; ++i)
    {
        std::wstring arg = argv[i];
        if ((arg == L"-name" || arg == L"--name") && i + 1 < argc)
            byName = argv[++i];
        else if ((arg == L"-pid" || arg == L"--pid") && i + 1 < argc)
            targetPid = (DWORD)_wtoi(argv[++i]);
        else
        {
            CloseHandle(hUnloadEvent);
            DeleteFileW(dllPath.c_str());
            return PrintUsage(argv[0]);
        }
    }

    if (byName.empty() && targetPid == 0)
    {
        Say(L"Procurando processo com BstkVMM.dll...");
        for (int attempt = 0; attempt < 120; ++attempt)
        {
            targetPid = FindTargetPid();
            if (targetPid)
                break;
            Sleep(1000);
        }

        if (!targetPid)
        {
            CloseHandle(hUnloadEvent);
            DeleteFileW(dllPath.c_str());
            Fail(L"ERRO: nenhum processo com BstkVMM.dll encontrado (emulador nao esta rodando?).");
            return 1;
        }
    }
    else if (byName.empty() && targetPid != 0)
    {
        Say(L"Usando PID " + std::to_wstring(targetPid) + L"...");
    }
    else if (!byName.empty())
    {
        Say(L"Procurando processo " + byName + L"...");
        for (int attempt = 0; attempt < 120; ++attempt)
        {
            targetPid = FindTargetPidByName(byName);
            if (targetPid)
                break;
            Sleep(1000);
        }

        if (!targetPid)
        {
            CloseHandle(hUnloadEvent);
            DeleteFileW(dllPath.c_str());
            Fail(L"ERRO: processo " + byName + L" nao encontrado.");
            return 1;
        }
    }

    Say(L"Injetando em PID " + std::to_wstring(targetPid) + L"...");

    if (!InjectDll(targetPid, dllPath))
    {
        DWORD err = GetLastError();
        CloseHandle(hUnloadEvent);
        DeleteFileW(dllPath.c_str());
        Fail(L"ERRO: falha na injecao (erro " + std::to_wstring(err) + L"). Execute como administrador?");
        return 1;
    }

    Say(L"OK: DLL injetada. Aguardando unload para limpar rastros...");

    HANDLE hTarget = OpenProcess(SYNCHRONIZE, FALSE, targetPid);
    if (hTarget)
    {
        HANDLE waits[2] = { hUnloadEvent, hTarget };
        DWORD r = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (r == WAIT_OBJECT_0)
        {
            Say(L"Unload detectado. Limpando rastros...");
            Sleep(500);
        }
        else
        {
            Say(L"Processo alvo encerrado. Limpando rastros...");
        }

        CloseHandle(hTarget);
    }
    else
    {
        Say(L"Processo alvo inacessivel. Limpando rastros...");
    }

    CloseHandle(hUnloadEvent);

    DeleteFileW(dllPath.c_str());
    RemoveCheatConfig();
    CleanupRegistry();
    CleanupRunMRU();
    CleanupUserAssist();
    CleanupPrefetch();
    SelfDeleteExe();

    Say(L"Rastros limpos.");
    return 0;
}
