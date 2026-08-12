#define DISCORD_DISABLE_IO_THREAD
#include "DiscordRPC.hpp"
#include <discord_rpc.h>
#include <Windows.h>
#include <winhttp.h>
#include <wincodec.h>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <ctime>
#include <XorStr.hpp>

#include <src/DynamicStub/DynamicStub.hpp>

#ifdef DISCORD_DISABLE_IO_THREAD
extern "C" void Discord_UpdateConnection(void);
#endif

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")

static bool s_initialized = false;
static bool s_lastEnabled = false;
static int64_t g_RpcStartTimestamp = 0;

static HANDLE s_RpcThreadHandle = nullptr;
static HANDLE s_StopEvent = nullptr;

static std::atomic<bool> s_WorkRunning = false;
static std::atomic<bool> s_ShouldStop = false;

static std::string                g_DiscordUserId;
static std::string                g_DiscordUsername;
static std::string                g_DiscordAvatarHash;
static std::vector<unsigned char> g_AvatarPixels;
static int                        g_AvatarWidth = 0;
static int                        g_AvatarHeight = 0;
static std::atomic<bool>          g_AvatarStarted = false;
static std::atomic<bool>          g_AvatarReady = false;


// =======================================================
// Função auxiliar para HTTP GET via WinHTTP
// =======================================================
static bool HttpGetToMemory(const std::wstring& host,
    const std::wstring& path,
    std::vector<unsigned char>& out)
{
    HINTERNET hSession = WinHttpOpen(
        XorStr(L"Mozilla/5.0"),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT,
        0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        XorStr(L"GET"),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool ok = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD dwSize = 0;
        do
        {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || !dwSize)
                break;

            size_t oldSize = out.size();
            out.resize(oldSize + dwSize);

            DWORD dwRead = 0;
            if (!WinHttpReadData(hRequest, out.data() + oldSize, dwSize, &dwRead))
            {
                out.resize(oldSize);
                break;
            }

            if (dwRead < dwSize)
                out.resize(oldSize + dwRead);

        } while (dwSize > 0);

        ok = !out.empty();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

// =======================================================
// Decodificação de imagem PNG → RGBA via WIC
// =======================================================
static bool DecodeImageWIC(const std::vector<unsigned char>& data,
    std::vector<unsigned char>& outRgba,
    int& outW,
    int& outH)
{
    if (data.empty())
        return false;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInit = SUCCEEDED(hr);

    IWICImagingFactory* pFactory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr) || !pFactory)
    {
        if (coInit) CoUninitialize();
        return false;
    }

    IWICStream* pStream = nullptr;
    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream)
    {
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pStream->InitializeFromMemory((WICInProcPointer)data.data(), (DWORD)data.size());
    if (FAILED(hr))
    {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr) || !pDecoder)
    {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame)
    {
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IWICFormatConverter* pConv = nullptr;
    hr = pFactory->CreateFormatConverter(&pConv);
    if (FAILED(hr) || !pConv)
    {
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pConv->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
        nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        pConv->Release();
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    UINT w = 0, h = 0;
    pConv->GetSize(&w, &h);
    if (w == 0 || h == 0)
    {
        pConv->Release();
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    outRgba.resize((size_t)w * h * 4);
    pConv->CopyPixels(nullptr, w * 4, (UINT)outRgba.size(), outRgba.data());

    outW = (int)w;
    outH = (int)h;

    pConv->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();
    if (coInit) CoUninitialize();

    return true;
}


static void DownloadAvatarThread()
{
    if (g_DiscordUserId.empty() || g_DiscordAvatarHash.empty())
        return;

    // Avatar animado tem hash com prefixo "a_" -> CDN exige .gif; senao .png
    bool animated = g_DiscordAvatarHash.size() > 2 &&
                    g_DiscordAvatarHash[0] == 'a' &&
                    g_DiscordAvatarHash[1] == '_';
    std::string ext = animated ? XorStr(".gif") : XorStr(".png");
    std::string path = std::string(XorStr("/avatars/")) + g_DiscordUserId + "/" + g_DiscordAvatarHash + ext + XorStr("?size=64");
    std::wstring wpath(path.begin(), path.end());

    // Baixa direto do CDN do Discord (sem depender de backend proprio)
    for (int attempt = 0; attempt < 3; attempt++)
    {
        std::vector<unsigned char> pngBytes;
        if (HttpGetToMemory(XorStr(L"cdn.discordapp.com"), wpath, pngBytes) && !pngBytes.empty())
        {
            std::vector<unsigned char> rgba;
            int w = 0, h = 0;

            if (DecodeImageWIC(pngBytes, rgba, w, h))
            {
                g_AvatarWidth = w;
                g_AvatarHeight = h;
                g_AvatarPixels.swap(rgba);

                g_AvatarReady = true;
                return;
            }
        }

        Sleep(1000);
    }
}

static void EnsureAvatarDownloadStarted()
{
    bool expected = false;
    if (g_AvatarStarted.compare_exchange_strong(expected, true))
        std::thread(DownloadAvatarThread).detach();
}

// =======================================================
// Callbacks e inicialização
// =======================================================
static void ClearRPC()
{
    if (!s_initialized)
        return;

    Discord_ClearPresence();

#ifdef DISCORD_DISABLE_IO_THREAD
    for (int i = 0; i < 10; i++)
    {
        Discord_UpdateConnection();
        Discord_RunCallbacks();
        Sleep(10);
    }
#endif

    Discord_Shutdown();
    s_initialized = false;
}

static void OnDiscordReady(const DiscordUser* user)
{
    if (!user)
        return;

    if (user->userId)
        g_DiscordUserId = user->userId;

    if (user->username)
        g_DiscordUsername = user->username;

    if (user->avatar)
        g_DiscordAvatarHash = user->avatar;

    if (!g_DiscordUserId.empty() && !g_DiscordAvatarHash.empty())
        EnsureAvatarDownloadStarted();
}

static void InitializeStartTimestamp()
{
    if (g_RpcStartTimestamp == 0)
        g_RpcStartTimestamp = static_cast<int64_t>(std::time(nullptr));
}

static void InitializeIfNeeded()
{
    if (s_initialized)
        return;

    InitializeStartTimestamp();

    DiscordEventHandlers handlers{};
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = OnDiscordReady;

    // autoRegister=0: NAO cria chave no registro (HKCU\Software\Classes\
    // discord-<appid>). O protocolo de "Join Game" nao e usado — so presenca,
    // nome e avatar — entao nada do cheat fica no regedit.
    Discord_Initialize(XorStr("1439325737629913380"), &handlers, 0, nullptr);
    s_initialized = true;

    DiscordRichPresence presence{};
    memset(&presence, 0, sizeof(presence));

    // Strings precisam viver até Discord_UpdatePresence ler —
    // std::string copia os bytes antes do temporário XorStr morrer
    std::string sDetails = XorStr("O melhor cheat para Free Fire!🔥");
    std::string sState = XorStr("100% Antiban🛡️ | Atualizado📦");
    std::string sLargeKey = XorStr("zimol");
    std::string sLargeText = XorStr("Zimo Internal");
    std::string sSmallKey = XorStr("freefire");
    std::string sSmallText = XorStr("Free Fire");
    std::string sBtn1Label = XorStr("💻ZIMO STORE🛠️");
    std::string sBtn1Url = XorStr("https://discord.gg/zimostore");
    std::string sBtn2Label = XorStr("💰Compre Aqui💸");
    std::string sBtn2Url = XorStr("https://discord.gg/TUYRRHwrSM");

    presence.startTimestamp = g_RpcStartTimestamp;
    presence.details = sDetails.c_str();
    presence.state = sState.c_str();
    presence.largeImageKey = sLargeKey.c_str();
    presence.largeImageText = sLargeText.c_str();
    presence.smallImageKey = sSmallKey.c_str();
    presence.smallImageText = sSmallText.c_str();
    presence.button1Label = sBtn1Label.c_str();
    presence.button1Url = sBtn1Url.c_str();
    presence.button2Label = sBtn2Label.c_str();
    presence.button2Url = sBtn2Url.c_str();

    Discord_UpdatePresence(&presence);
}

// =======================================================
// Thread principal do RPC via DynamicStub
// =======================================================
static DWORD WINAPI RpcThreadProc(LPVOID)
{
    InitializeIfNeeded();
    s_WorkRunning = true;

    while (!s_ShouldStop)
    {
#ifdef DISCORD_DISABLE_IO_THREAD
        Discord_UpdateConnection();
#endif
        Discord_RunCallbacks();
        Sleep(50);
    }

    ClearRPC();
    s_WorkRunning = false;

    return 0;
}

static void CleanupOldThread()
{
    if (!s_RpcThreadHandle)
        return;

    DWORD exitCode = 0;
    if (GetExitCodeThread(s_RpcThreadHandle, &exitCode))
    {
        if (exitCode != STILL_ACTIVE)
        {
            CloseHandle(s_RpcThreadHandle);
            s_RpcThreadHandle = nullptr;
        }
    }
}

static void StartRpcWorker()
{
    if (s_WorkRunning)
        return;

    CleanupOldThread();

    s_ShouldStop = false;

    if (!s_StopEvent)
        s_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(s_StopEvent);

    s_RpcThreadHandle = DynamicStub::CreateThreadWithDynamicStub(RpcThreadProc, nullptr, 0, nullptr);

    if (!s_RpcThreadHandle)
    {
        s_WorkRunning = false;
        return;
    }
}

static void RequestStopWorker()
{
    if (!s_WorkRunning)
        return;

    s_ShouldStop = true;
    if (s_StopEvent)
        SetEvent(s_StopEvent);

    for (int i = 0; i < 20 && s_WorkRunning; i++)
    {
        Sleep(50);
    }

    if (s_WorkRunning && s_RpcThreadHandle)
    {
        TerminateThread(s_RpcThreadHandle, 0);
        s_WorkRunning = false;
    }

    if (s_RpcThreadHandle)
    {
        CloseHandle(s_RpcThreadHandle);
        s_RpcThreadHandle = nullptr;
    }
}

// =======================================================
// Interface pública
// =======================================================
namespace DiscordRPC
{
    void Tick(bool enabled)
    {
        if (enabled && !s_lastEnabled)
        {
            StartRpcWorker();
            s_lastEnabled = true;
        }
        else if (!enabled && s_lastEnabled)
        {
            RequestStopWorker();
            s_lastEnabled = false;
        }
    }

    void Shutdown()
    {
        s_lastEnabled = false;
        RequestStopWorker();

        if (s_StopEvent)
        {
            CloseHandle(s_StopEvent);
            s_StopEvent = nullptr;
        }

        ClearRPC();
    }

    bool GetAvatarData(AvatarData& out)
    {
        if (!g_AvatarReady)
            return false;

        if (g_AvatarPixels.empty() || g_AvatarWidth <= 0 || g_AvatarHeight <= 0)
            return false;

        out.width = g_AvatarWidth;
        out.height = g_AvatarHeight;
        out.pixels = g_AvatarPixels.data();
        return true;
    }

    bool GetUsername(std::string& out)
    {
        if (g_DiscordUsername.empty())
            return false;
        out = g_DiscordUsername;
        return true;
    }
}