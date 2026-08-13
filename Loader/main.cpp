#include <Windows.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <Aclapi.h>
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

// Extrai a HwMonCore.dll embutida como recurso RCDATA (id 101) no proprio exe.
// Permite rodar o exe sozinho em qualquer pasta — sem dependencia da DLL ao
// lado. Grava no temp com o mesmo nome aleatorio do CopyToTemp.
static std::wstring ExtractEmbeddedDll()
{
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!hRes) return L"";
    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return L"";
    void* pData = LockResource(hData);
    DWORD size = SizeofResource(nullptr, hRes);
    if (!pData || size == 0) return L"";

    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dest = std::wstring(tmp) + RandomFileName();

    HANDLE h = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return L"";
    DWORD written = 0;
    BOOL ok = WriteFile(h, pData, size, &written, nullptr);
    CloseHandle(h);
    if (!ok || written != size)
    {
        DeleteFileW(dest.c_str());
        return L"";
    }
    return dest;
}

static std::wstring LocalAppData()
{
    wchar_t buf[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf) != S_OK)
        return L"";
    return std::wstring(buf);
}

static void SecureRemoveAll(const std::wstring& dirPath);
static bool IsOurRandomTempName(const std::wstring& name);
static void CleanupEmulatorCrashDumps();
static void CleanupEmulatorWerReports();
static void CleanupEmulatorPrefetch();
static void CleanupShellMru(const std::wstring& keyPath, bool withSubkeys);
static void CleanupBam();
static void EnableAppCompatSilence();
static void RestoreAppCompatEngine();

static void RemoveCheatConfig()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    std::filesystem::path dir = std::filesystem::path(base) / L"HwMon";
    SecureRemoveAll(dir.wstring());
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

// Nomes/padroes do cheat — qualquer entrada de historico (arquivo, pasta,
// caminho digitado, PIDL) que referencie um deles e removida.
static const wchar_t* g_TraceNames[] = {
    L"HardwareMonitor", L"HwMonCore", L"ZmLoader", L"ZmInternal",
    L"HwMon", L"~Z",
};

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

// Nomes do cheat que nunca podem sobrar em lugar algum (dumps, WER, Recent).
static bool HasTraceName(const std::wstring& text)
{
    for (auto* n : g_TraceNames)
    {
        if (ContainsCI(text, n))
            return true;
    }
    return false;
}

// Dumps de crash do WER (C:\Users\<u>\AppData\Local\CrashDumps) — se o
// processo crashar (ex.: emulador derrubado), o WER salva
// HardwareMonitor.exe.<pid>.dmp aqui. Sobrescreve + apaga.
static void CleanupCrashDumps()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    std::wstring dir = base + L"\\CrashDumps";
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return;

    for (auto it = std::filesystem::directory_iterator(dir, ec);
         it != std::filesystem::directory_iterator(); ++it)
    {
        if (it->is_regular_file(ec))
        {
            std::wstring name = it->path().filename().wstring();
            if (HasTraceName(name) || IsOurRandomTempName(name))
                SecureDeleteFile(it->path().wstring());
        }
    }
}

// Relatorios do WER (ReportArchive/ReportQueue) — pastas
// "AppCrash_HardwareMonitor.exe_..." com o dump completo. Remove recursivo
// com sobrescrita de conteudo antes.
static void CleanupWerReports()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    for (const wchar_t* sub : { L"ReportArchive", L"ReportQueue" })
    {
        std::wstring dir = base + L"\\Microsoft\\Windows\\WER\\" + sub;
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec))
            continue;

        for (auto it = std::filesystem::directory_iterator(dir, ec);
             it != std::filesystem::directory_iterator(); ++it)
        {
            std::wstring name = it->path().filename().wstring();
            if (it->is_directory(ec) && HasTraceName(name))
                SecureRemoveAll(it->path().wstring());
        }
    }
}

// Atalhos de "Documentos Recentes" criados ao abrir o exe pelo Explorer.
static void CleanupRecent()
{
    wchar_t buf[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_RECENT, nullptr, SHGFP_TYPE_CURRENT, buf) != S_OK)
        return;

    std::wstring dir = std::wstring(buf) + L"\\";
    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW((dir + L"*.lnk").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (HasTraceName(fd.cFileName))
            SecureDeleteFile(dir + fd.cFileName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// Procura uma string UTF-16 (nome ASCII) dentro de um blob binario — usado
// para achar caminhos embutidos em PIDLs (RecentDocs/ComDlg32) sem precisar
// desmontar a estrutura.
static bool BlobContainsWide(const BYTE* data, DWORD len, const wchar_t* needle)
{
    if (!data || len < 2 || !needle || !*needle)
        return false;

    size_t n = wcslen(needle);
    if (n * 2 > len)
        return false;

    for (size_t i = 0; i + n <= len / 2; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < n; ++j)
        {
            wchar_t b = (wchar_t)(data[(i + j) * 2] | ((wchar_t)data[(i + j) * 2 + 1] << 8));
            wchar_t a = needle[j];
            if (a >= L'a' && a <= L'z') a = (wchar_t)(a - L'a' + L'A');
            if (b >= L'a' && b <= L'z') b = (wchar_t)(b - L'a' + L'A');
            if (b != a) { match = false; break; }
        }
        if (match)
            return true;
    }
    return false;
}

// Varre chaves de historico do Explorer (RecentDocs, TypedPaths, ComDlg32)
// e apaga apenas entradas que referenciam nomes/caminhos do cheat. Quando a
// chave tem MRUListEx, os indices das entradas apagadas sao removidos da
// lista. Se uma subchave (comSubkeys=true) ficar vazia, ela e removida.
static void CleanupShellMru(const std::wstring& keyPath, bool withSubkeys)
{
    HKEY hRoot = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ | KEY_WRITE, &hRoot) != ERROR_SUCCESS)
        return;

    auto processKey = [](HKEY hKey) -> bool
    {
        std::vector<std::wstring> toDelete;
        for (DWORD i = 0; ; ++i)
        {
            wchar_t vName[128]{};
            DWORD vLen = 128;
            DWORD type = 0;
            BYTE data[4096]{};
            DWORD dataLen = sizeof(data);
            LONG r = RegEnumValueW(hKey, i, vName, &vLen, nullptr, &type, data, &dataLen);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (r != ERROR_SUCCESS) continue;
            if (_wcsicmp(vName, L"MRUListEx") == 0) continue;

            bool hit = false;
            if (type == REG_BINARY)
            {
                for (auto* n : g_TraceNames)
                {
                    if (BlobContainsWide(data, dataLen, n)) { hit = true; break; }
                }
            }
            else if (type == REG_SZ)
            {
                std::wstring s((const wchar_t*)data, dataLen / sizeof(wchar_t));
                if (HasTraceName(s)) hit = true;
            }
            if (hit)
                toDelete.push_back(vName);
        }
        if (toDelete.empty())
            return false;

        // Rebuild MRUListEx sem os indices apagados
        BYTE mru[1024]{};
        DWORD mruLen = sizeof(mru);
        bool hasMru = (RegQueryValueExW(hKey, L"MRUListEx", nullptr, nullptr, mru, &mruLen) == ERROR_SUCCESS) && mruLen >= 4;

        std::vector<DWORD> keep;
        if (hasMru)
        {
            for (DWORD off = 0; off + 4 <= mruLen; off += 4)
            {
                DWORD idx = *(DWORD*)(mru + off);
                if (idx == 0xFFFFFFFF) break;
                bool del = false;
                for (const auto& v : toDelete)
                {
                    if (_wtoi(v.c_str()) == (int)idx) { del = true; break; }
                }
                if (!del) keep.push_back(idx);
            }
        }

        if (hasMru)
        {
            std::vector<BYTE> out;
            for (DWORD idx : keep)
            {
                BYTE e[4];
                memcpy(e, &idx, 4);
                out.insert(out.end(), e, e + 4);
            }
            DWORD term = 0xFFFFFFFF;
            BYTE tb[4];
            memcpy(tb, &term, 4);
            out.insert(out.end(), tb, tb + 4);
            if (keep.empty())
                RegDeleteValueW(hKey, L"MRUListEx");
            else
                RegSetValueExW(hKey, L"MRUListEx", 0, REG_BINARY, out.data(), (DWORD)out.size());
        }

        for (const auto& v : toDelete)
            RegDeleteValueW(hKey, v.c_str());

        // Retorna true se a chave ficou totalmente vazia (para remover a
        // subchave em chaves de nivel mais baixo)
        DWORD subCount = 0, valCount = 0;
        return RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &subCount, nullptr, nullptr, &valCount, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS
            && subCount == 0 && valCount == 0;
    };

    if (withSubkeys)
    {
        // Processa TAMBEM a raiz da chave — o Explorer guarda entradas no
        // valor raiz do RecentDocs (ex.: RecentDocs\75) alem das subchaves
        // por extensao (.exe, .rar...).
        processKey(hRoot);

        std::vector<std::wstring> subNames;
        for (DWORD i = 0; ; ++i)
        {
            wchar_t subName[256]{};
            DWORD subLen = 256;
            if (RegEnumKeyExW(hRoot, i, subName, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            subNames.push_back(subName);
        }

        for (const auto& name : subNames)
        {
            HKEY hSub = nullptr;
            if (RegOpenKeyExW(hRoot, name.c_str(), 0, KEY_READ | KEY_WRITE, &hSub) != ERROR_SUCCESS)
                continue;
            bool empty = processKey(hSub);
            RegCloseKey(hSub);
            if (empty)
                RegDeleteKeyW(hRoot, name.c_str());
        }
    }
    else
    {
        processKey(hRoot);
    }

    RegCloseKey(hRoot);
}

// Copias da DLL deixadas no TEMP por sessoes anteriores que morreram antes
// da limpeza (crash/kill do loader). Padrao de nome gerado por RandomFileName
// (~Z + 12 letras + .dll = 18 chars) — especifico demais para colidir com
// arquivo de usuario; apaga so os com mais de 10 minutos (nunca a copia da
// sessao atual, que nem existe quando isso roda no inicio).
static void CleanupLeftoverTempDll()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir(tmp);

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW((dir + L"~Z*.dll").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now{};
    now.HighPart = nowFt.dwHighDateTime;
    now.LowPart = nowFt.dwLowDateTime;
    const ULONGLONG kMaxAge = (ULONGLONG)10 * 60 * 10000000; // 10 minutos

    do
    {
        std::wstring name = fd.cFileName;
        if (name.size() != 18 || name.compare(0, 2, L"~Z") != 0)
            continue;

        bool letters = true;
        for (size_t i = 2; i < 14; ++i)
        {
            if (name[i] < L'a' || name[i] > L'z') { letters = false; break; }
        }
        if (!letters)
            continue;

        ULARGE_INTEGER age{};
        age.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        age.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (now.QuadPart > age.QuadPart && (now.QuadPart - age.QuadPart) < kMaxAge)
            continue;

        SecureDeleteFile(dir + name);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// Limpeza de seguranca rodada ANTES de tudo: apaga rastros de sessoes
// anteriores que morreram sem limpar (crash/kill), para que uma busca por
// rastros nunca encontre nada — mesmo um dump de crash antigo.
static void PreClean()
{
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    SecureDeleteFile(std::wstring(tmp) + L"HwMon.log");
    RemoveCheatConfig();
    CleanupRegistry();
    CleanupRunMRU();
    CleanupUserAssist();
    CleanupPrefetch();
    CleanupCrashDumps();
    CleanupWerReports();
    CleanupRecent();
    CleanupLeftoverTempDll();
    CleanupEmulatorCrashDumps();
    CleanupEmulatorWerReports();
    CleanupEmulatorPrefetch();
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs", true);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths", false);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU", true);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU", false);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\CIDSizeMRU", true);
    CleanupBam();
}

// Nome de arquivo gerado por nos no TEMP (~Z + 12 letras + '.') — usado
// para casar dumps de crash do MODULO injetado (o WER nomeia o dump pelo
// nome do arquivo da DLL, que e aleatorio, ex.: ~Zabcdefghijkl.1234.dmp).
static bool IsOurRandomTempName(const std::wstring& name)
{
    if (name.size() < 18)
        return false;
    if (name.compare(0, 2, L"~Z") != 0)
        return false;
    for (size_t i = 2; i < 14; ++i)
    {
        if (name[i] < L'a' || name[i] > L'z')
            return false;
    }
    return name[14] == L'.';
}

// O dump de crash do EMULADOR (HD-Player.exe.<pid>.dmp) lista os modulos
// carregados no processo — incluindo a DLL injetada. Sem o nome no arquivo,
// mas o conteudo delata; o WER tambem guarda copias em ReportArchive/Queue e
// o Prefetch do HD-PLAYER.EXE grava a lista de modulos. Remove tudo.
static bool HasEmulatorName(const std::wstring& text)
{
    static const wchar_t* names[] = { L"HD-Player", L"BstkRT", L"BstkVMM" };
    for (auto* n : names)
    {
        if (ContainsCI(text, n))
            return true;
    }
    return false;
}

static void CleanupEmulatorCrashDumps()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    std::wstring dir = base + L"\\CrashDumps";
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec))
        return;

    for (auto it = std::filesystem::directory_iterator(dir, ec);
         it != std::filesystem::directory_iterator(); ++it)
    {
        if (it->is_regular_file(ec) && HasEmulatorName(it->path().filename().wstring()))
            SecureDeleteFile(it->path().wstring());
    }
}

static void CleanupEmulatorWerReports()
{
    std::wstring base = LocalAppData();
    if (base.empty())
        return;

    for (const wchar_t* sub : { L"ReportArchive", L"ReportQueue" })
    {
        std::wstring dir = base + L"\\Microsoft\\Windows\\WER\\" + sub;
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec))
            continue;

        for (auto it = std::filesystem::directory_iterator(dir, ec);
             it != std::filesystem::directory_iterator(); ++it)
        {
            std::wstring name = it->path().filename().wstring();
            if (it->is_directory(ec) && HasEmulatorName(name))
                SecureRemoveAll(it->path().wstring());
        }
    }
}

static void CleanupEmulatorPrefetch()
{
    wchar_t winDir[MAX_PATH]{};
    if (!GetWindowsDirectoryW(winDir, MAX_PATH))
        return;

    std::wstring dir = std::wstring(winDir) + L"\\Prefetch\\";
    const wchar_t* bases[] = { L"HD-PLAYER.EXE-", L"HD-PLAYER_BG.EXE-", L"BSTKRT.DLL-" };
    for (auto* b : bases)
    {
        WIN32_FIND_DATAW fd{};
        HANDLE hFind = FindFirstFileW((dir + b + L"*.pf").c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            DeleteFileW((dir + fd.cFileName).c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
}

// ----------------------------------------------------------------
// HKLM "executou um exe" — BAM (Background Activity Moderator):
//   HKLM\SYSTEM\CurrentControlSet\Services\bam\State\UserSettings\<SID>
//   HKLM\SYSTEM\CurrentControlSet\Services\bam\State\CompactOS\<SID>
// Valores nomeados pelo caminho do processo. ACL SYSTEM-only: admin le,
// nao escreve — o dono da chave e' o grupo Administradores (do qual o
// processo elevado faz parte), entao temos WRITE_DAC implicito: trocamos
// a DACL temporariamente (ACE full p/ nos), escrevemos e restauramos.
// ----------------------------------------------------------------
static bool GetCurrentUserSid(PSID& outSid)
{
    outSid = nullptr;
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;

    DWORD len = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
    if (!len)
    {
        CloseHandle(hToken);
        return false;
    }

    std::vector<BYTE> buf(len);
    if (!GetTokenInformation(hToken, TokenUser, buf.data(), len, &len))
    {
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);

    TOKEN_USER* tu = reinterpret_cast<TOKEN_USER*>(buf.data());
    DWORD sidLen = GetLengthSid(tu->User.Sid);
    PSID copy = static_cast<PSID>(LocalAlloc(LPTR, sidLen));
    if (!copy)
        return false;
    CopySid(sidLen, copy, tu->User.Sid);
    outSid = copy;
    return true;
}

static PACL BuildAclWithSelf(PACL origDacl, PSID selfSid)
{
    DWORD extra = sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(selfSid) - sizeof(DWORD);
    DWORD size = 0;
    if (origDacl)
    {
        ACL_SIZE_INFORMATION info{};
        if (GetAclInformation(origDacl, &info, sizeof(info), AclSizeInformation))
            size = info.AclBytesInUse;
    }
    size += extra;

    PACL newAcl = static_cast<PACL>(LocalAlloc(LPTR, size));
    if (!newAcl)
        return nullptr;
    if (!InitializeAcl(newAcl, size, ACL_REVISION))
    {
        LocalFree(newAcl);
        return nullptr;
    }

    if (origDacl)
    {
        for (DWORD i = 0; i < origDacl->AceCount; ++i)
        {
            LPVOID pAce = nullptr;
            if (GetAce(origDacl, i, &pAce) && pAce)
                AddAce(newAcl, ACL_REVISION, MAXDWORD, pAce, (static_cast<PACE_HEADER>(pAce))->AceSize);
        }
    }

    if (!AddAccessAllowedAceEx(newAcl, ACL_REVISION, 0, KEY_ALL_ACCESS, selfSid))
    {
        LocalFree(newAcl);
        return nullptr;
    }
    return newAcl;
}

static void ScrubBamLeaf(const std::wstring& path)
{
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ | KEY_WRITE, &hKey);

    if (r == ERROR_ACCESS_DENIED)
    {
        // ---- Dance: o BAM da SET_VALUE apenas a SYSTEM/TrustedInstaller,
        // mas o dono da chave e' o grupo Administradores (do qual o processo
        // elevado faz parte) -> WRITE_DAC implicito. Trocamos a DACL
        // temporariamente (ACE full p/ nos), escrevemos e restauramos. ----
        HKEY hSec = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                          READ_CONTROL | WRITE_DAC, &hSec) != ERROR_SUCCESS)
            return;

        DWORD sdLen = 0;
        RegGetKeySecurity(hSec, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                          nullptr, &sdLen);
        std::vector<BYTE> sdBuf(sdLen);
        if (RegGetKeySecurity(hSec, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                              sdBuf.data(), &sdLen) != ERROR_SUCCESS)
        {
            RegCloseKey(hSec);
            return;
        }

        PSECURITY_DESCRIPTOR pSd = reinterpret_cast<PSECURITY_DESCRIPTOR>(sdBuf.data());
        BOOL daclPresent = FALSE;
        PACL pDacl = nullptr;
        BOOL daclDefaulted = FALSE;
        if (!GetSecurityDescriptorDacl(pSd, &daclPresent, &pDacl, &daclDefaulted))
        {
            RegCloseKey(hSec);
            return;
        }

        PSID selfSid = nullptr;
        if (!GetCurrentUserSid(selfSid))
        {
            RegCloseKey(hSec);
            return;
        }

        PACL newDacl = BuildAclWithSelf(pDacl, selfSid);
        LocalFree(selfSid);
        if (!newDacl)
        {
            RegCloseKey(hSec);
            return;
        }

        SECURITY_DESCRIPTOR sdNew = {};
        if (InitializeSecurityDescriptor(&sdNew, SECURITY_DESCRIPTOR_REVISION) &&
            SetSecurityDescriptorDacl(&sdNew, TRUE, newDacl, FALSE))
        {
            if (RegSetKeySecurity(hSec, DACL_SECURITY_INFORMATION, &sdNew) == ERROR_SUCCESS)
                r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0,
                                  KEY_READ | KEY_WRITE, &hKey);
        }

        // Restaura a DACL original imediatamente, aconteca o que acontecer
        RegSetKeySecurity(hSec, DACL_SECURITY_INFORMATION, pSd);
        LocalFree(newDacl);
        RegCloseKey(hSec);
        if (r != ERROR_SUCCESS)
            return;
    }

    if (hKey == nullptr)
        return;

    // BAM nomeia os valores com o caminho do processo — apaga os nossos
    for (DWORD i = 0; ; ++i)
    {
        wchar_t vName[1024]{};
        DWORD vLen = 1024;
        LONG vr = RegEnumValueW(hKey, i, vName, &vLen, nullptr, nullptr, nullptr, nullptr);
        if (vr == ERROR_NO_MORE_ITEMS)
            break;
        if (vr != ERROR_SUCCESS)
            continue;
        std::wstring name(vName, vLen);
        if (HasTraceName(name))
            RegDeleteValueW(hKey, vName);
    }

    RegCloseKey(hKey);
}

static void CleanupBam()
{
    const wchar_t* base = L"SYSTEM\\CurrentControlSet\\Services\\bam\\State";
    for (const wchar_t* branch : { L"UserSettings", L"CompactOS" })
    {
        std::wstring branchPath = std::wstring(base) + L"\\" + branch;
        HKEY hBranch = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, branchPath.c_str(), 0, KEY_READ, &hBranch) != ERROR_SUCCESS)
            continue;

        std::vector<std::wstring> sids;
        for (DWORD i = 0; ; ++i)
        {
            wchar_t sid[128]{};
            DWORD sidLen = 128;
            if (RegEnumKeyExW(hBranch, i, sid, &sidLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;
            sids.push_back(std::wstring(sid, sidLen));
        }
        RegCloseKey(hBranch);

        for (const auto& sid : sids)
            ScrubBamLeaf(branchPath + L"\\" + sid);
    }
}

// ----------------------------------------------------------------
// ShimCache (AppCompatCache) + Amcache: a engine de compatibilidade do
// Windows grava "quem executou". Política oficial (GPO) que desliga a
// engine: HKLM\SOFTWARE\Policies\Microsoft\Windows\AppCompat\
//   DisableEngine = 1 (DWORD)
// Ativada durante a sessao do cheat; removida no unload. Efeito total a
// partir do proximo boot (o kernel le a config ao iniciar).
// ----------------------------------------------------------------
static void EnableAppCompatSilence()
{
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        DWORD one = 1;
        RegSetValueExW(hKey, L"DisableEngine", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
        RegCloseKey(hKey);
    }
}

static void RestoreAppCompatEngine()
{
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat");
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

    // Log diagnostico criado pela DLL no TEMP (sobrescreve antes de apagar)
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    SecureDeleteFile(std::wstring(tmp) + L"HwMon.log");

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
    Say(L"  -clean             limpa rastros de sessoes anteriores e sai");
    Say(L"  (sem opcoes)       detecta automaticamente o processo com BstkVMM.dll");
    Say(L"Ao detectar o unload (bypass), limpa todos os rastros e se apaga sozinho.");
    return 1;
}

// Limpeza de rastros de UMA sessao (sem tocar em SelfDeleteExe nem na
// engine de compatibilidade — esses so rodam no encerramento definitivo,
// porque o loader residente pode re-injetar quantas vezes quiser).
static void SessionCleanup()
{
    RemoveCheatConfig();
    CleanupRegistry();
    CleanupRunMRU();
    CleanupUserAssist();
    CleanupPrefetch();
    CleanupCrashDumps();
    CleanupWerReports();
    CleanupRecent();
    CleanupLeftoverTempDll();
    CleanupEmulatorCrashDumps();
    CleanupEmulatorWerReports();
    CleanupEmulatorPrefetch();
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RecentDocs", true);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths", false);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\OpenSavePidlMRU", true);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\LastVisitedPidlMRU", false);
    CleanupShellMru(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ComDlg32\\CIDSizeMRU", true);
}

int wmain(int argc, wchar_t* argv[])
{
    EnableDebugPrivilege();

    // Remove rastros de sessoes anteriores (crash/kill sem limpeza) antes
    // de qualquer outra coisa.
    PreClean();

    // Modo clean: so limpa (inclusive BAM) e deixa a engine de compatibilidade
    // desligada — a proxima sessao do cheat nao grava ShimCache/Amcache.
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] && (wcscmp(argv[i], L"-clean") == 0 || wcscmp(argv[i], L"--clean") == 0))
        {
            EnableAppCompatSilence();
            CleanupBam();
            Say(L"Rastros limpos (modo clean). Engine de compatibilidade desligada.");
            return 0;
        }
    }

    // Silencio da engine de compatibilidade durante a sessao (ShimCache/
    // Amcache nao gravam novas execucoes; restaurado no unload).
    EnableAppCompatSilence();

    std::wstring dllSource = FindDllPath();
    if (dllSource.empty())
        dllSource = ExtractEmbeddedDll(); // fallback: DLL embutida como recurso no exe
    if (dllSource.empty())
    {
        Fail(L"ERRO: HwMonCore.dll nao encontrada (procure ao lado do exe ou em ..\\x64\\Release\\).");
        return 1;
    }

    // Hotkeys removidas deliberadamente: apos o bypass o loader NAO fica
    // residente — encerra de vez (limpa rastros + auto-delete do exe),
    // entao o processo desaparece por completo e nao fica "aparecendo".

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
            return PrintUsage(argv[0]);
    }

    // ---- Localiza o alvo ----
    DWORD pid = 0;
    if (!byName.empty())
    {
        Say(L"Procurando processo " + byName + L"...");
        for (int attempt = 0; attempt < 120 && !pid; ++attempt)
        {
            pid = FindTargetPidByName(byName);
            if (!pid) Sleep(1000);
        }
    }
    else if (targetPid != 0)
    {
        pid = targetPid;
        Say(L"Usando PID " + std::to_wstring(pid) + L"...");
    }
    else
    {
        Say(L"Procurando processo com BstkVMM.dll...");
        for (int attempt = 0; attempt < 120 && !pid; ++attempt)
        {
            pid = FindTargetPid();
            if (!pid) Sleep(1000);
        }
    }

    if (!pid)
    {
        Fail(L"ERRO: processo alvo nao encontrado.");
        CleanupBam();
        RestoreAppCompatEngine();
        SelfDeleteExe();
        return 1;
    }

    std::wstring dllPath = CopyToTemp(dllSource);
    if (dllPath.empty())
    {
        Fail(L"ERRO: falha ao copiar DLL para temp.");
        CleanupBam();
        RestoreAppCompatEngine();
        SelfDeleteExe();
        return 1;
    }

    HANDLE hUnloadEvent = CreateEventW(nullptr, TRUE, FALSE, L"HwMonEvt");
    if (!hUnloadEvent)
    {
        SecureDeleteFile(dllPath.c_str());
        Fail(L"ERRO: falha ao criar evento de unload.");
        CleanupBam();
        RestoreAppCompatEngine();
        SelfDeleteExe();
        return 1;
    }

    Say(L"Injetando em PID " + std::to_wstring(pid) + L"...");

    if (!InjectDll(pid, dllPath))
    {
        DWORD err = GetLastError();
        CloseHandle(hUnloadEvent);
        SecureDeleteFile(dllPath.c_str());
        Fail(L"ERRO: falha na injecao (erro " + std::to_wstring(err) + L"). Execute como administrador?");
        CleanupBam();
        RestoreAppCompatEngine();
        SelfDeleteExe();
        return 1;
    }

    Say(L"OK: DLL injetada.");

    // ---- Vigia o unload (bypass) ----
    HANDLE hTarget = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hTarget)
    {
        HANDLE waits[2] = { hUnloadEvent, hTarget };
        DWORD r = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (r == WAIT_OBJECT_0)
        {
            Say(L"Unload detectado (bypass). Limpando rastros da sessao...");
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

    // ---- Limpeza da sessao ----
    // Se o processo alvo crashou, o WER ainda esta escrevendo o dump —
    // espera para a limpeza conseguir apagar o arquivo recem-criado.
    Sleep(4000);

    SecureDeleteFile(dllPath.c_str());
    SessionCleanup();

    // ---- Encerramento definitivo: limpeza total + auto-delete ----
    RestoreAppCompatEngine();
    CleanupBam();
    SelfDeleteExe();

    Say(L"Rastros limpos. Exe removido.");
    return 0;
}
