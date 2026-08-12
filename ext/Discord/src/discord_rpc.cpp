#define DISCORD_DISABLE_IO_THREAD
#include "discord_rpc.h"

#include "backoff.h"
#include "discord_register.h"
#include "msg_queue.h"
#include "rpc_connection.h"
#include "serialization.h"

#include <Windows.h>
#include <intrin.h>
#include <new>
#include <cstring>
#include <cstdint>

constexpr size_t MaxMessageSize{ 16 * 1024 };
constexpr size_t MessageQueueSize{ 8 };
constexpr size_t JoinQueueSize{ 8 };

struct QueuedMessage {
    size_t length;
    char buffer[MaxMessageSize];

    void Copy(const QueuedMessage& other)
    {
        length = other.length;
        if (length) {
            memcpy(buffer, other.buffer, length);
        }
    }
};

struct User {
    char userId[32];
    char username[344];
    char discriminator[8];
    char avatar[128];
};

// ------------------ CRT-friendly primitives (no std::mutex / no std::atomic) ------------------

struct SpinLock {
    volatile LONG state;
};

static __forceinline void SpinLock_Lock(SpinLock* l)
{
    // simple pause-based spin
    while (InterlockedCompareExchange(&l->state, 1, 0) != 0) {
        for (int i = 0; i < 64; ++i) {
            _mm_pause();
        }
    }
}

static __forceinline void SpinLock_Unlock(SpinLock* l)
{
    InterlockedExchange(&l->state, 0);
}

struct SpinGuard {
    SpinLock* l;
    explicit SpinGuard(SpinLock* lock) : l(lock) { SpinLock_Lock(l); }
    ~SpinGuard() { SpinLock_Unlock(l); }
};

static __forceinline bool Flag_TestAndClear(volatile LONG* f)
{
    return InterlockedExchange((volatile LONG*)f, 0) != 0;
}
static __forceinline bool Flag_Exchange(volatile LONG* f, bool v)
{
    return InterlockedExchange((volatile LONG*)f, v ? 1 : 0) != 0;
}
static __forceinline void Flag_Set(volatile LONG* f, bool v)
{
    InterlockedExchange((volatile LONG*)f, v ? 1 : 0);
}

// ---------------------------------------------------------------------------------------------

static RpcConnection* Connection{ nullptr };
static DiscordEventHandlers QueuedHandlers{};
static DiscordEventHandlers Handlers{};

static volatile LONG WasJustConnected{ 0 };
static volatile LONG WasJustDisconnected{ 0 };
static volatile LONG GotErrorMessage{ 0 };
static volatile LONG WasJoinGame{ 0 };
static volatile LONG WasSpectateGame{ 0 };
static volatile LONG UpdatePresence{ 0 };

static char JoinGameSecret[256];
static char SpectateGameSecret[256];

static int LastErrorCode{ 0 };
static char LastErrorMessage[256];
static int LastDisconnectErrorCode{ 0 };
static char LastDisconnectErrorMessage[256];

// replace std::mutex with POD spinlocks
static SpinLock PresenceLock{ 0 };
static SpinLock HandlerLock{ 0 };

static QueuedMessage QueuedPresence{};
static MsgQueue<QueuedMessage, MessageQueueSize> SendQueue;
static MsgQueue<User, JoinQueueSize> JoinAskQueue;
static User connectedUser;

// backoff from 0.5 sec to 1 min
static Backoff ReconnectTimeMs(500, 60 * 1000);

// instead of chrono
static ULONGLONG NextConnectTick = 0;

static int Pid{ 0 };
static int Nonce{ 1 };

// ---------------- IO thread holder (kept, but DISCORD_DISABLE_IO_THREAD makes it no-op) -------

#ifdef DISCORD_DISABLE_IO_THREAD
extern "C" DISCORD_EXPORT void Discord_UpdateConnection(void);
class IoThreadHolder {
public:
    void Start() {}
    void Stop() {}
    void Notify() {}
};
#else
// If you ever compile without DISCORD_DISABLE_IO_THREAD, you can implement a Win32 thread/event
// version here to avoid std::thread/condition_variable too.
static void Discord_UpdateConnection(void);
class IoThreadHolder {
public:
    void Start() {}
    void Stop() {}
    void Notify() {}
};
#endif

static IoThreadHolder g_IoThread;
static IoThreadHolder* IoThread = &g_IoThread;

static void UpdateReconnectTime()
{
    NextConnectTick = GetTickCount64() + (ULONGLONG)ReconnectTimeMs.nextDelay();
}

#ifdef DISCORD_DISABLE_IO_THREAD
extern "C" DISCORD_EXPORT void Discord_UpdateConnection(void)
#else
static void Discord_UpdateConnection(void)
#endif
{
    if (!Connection) {
        return;
    }

    if (!Connection->IsOpen()) {
        if (GetTickCount64() >= NextConnectTick) {
            UpdateReconnectTime();
            Connection->Open();
        }
    }
    else {
        // reads
        for (;;) {
            JsonDocument message;
            if (!Connection->Read(message)) {
                break;
            }

            const char* evtName = GetStrMember(&message, "evt");
            const char* nonce = GetStrMember(&message, "nonce");

            if (nonce) {
                if (evtName && strcmp(evtName, "ERROR") == 0) {
                    auto data = GetObjMember(&message, "data");
                    LastErrorCode = GetIntMember(data, "code");
                    StringCopy(LastErrorMessage, GetStrMember(data, "message", ""));
                    Flag_Set(&GotErrorMessage, true);
                }
            }
            else {
                if (evtName == nullptr) {
                    continue;
                }

                auto data = GetObjMember(&message, "data");

                if (strcmp(evtName, "ACTIVITY_JOIN") == 0) {
                    auto secret = GetStrMember(data, "secret");
                    if (secret) {
                        StringCopy(JoinGameSecret, secret);
                        Flag_Set(&WasJoinGame, true);
                    }
                }
                else if (strcmp(evtName, "ACTIVITY_SPECTATE") == 0) {
                    auto secret = GetStrMember(data, "secret");
                    if (secret) {
                        StringCopy(SpectateGameSecret, secret);
                        Flag_Set(&WasSpectateGame, true);
                    }
                }
                else if (strcmp(evtName, "ACTIVITY_JOIN_REQUEST") == 0) {
                    auto user = GetObjMember(data, "user");
                    auto userId = GetStrMember(user, "id");
                    auto username = GetStrMember(user, "username");
                    auto avatar = GetStrMember(user, "avatar");
                    auto joinReq = JoinAskQueue.GetNextAddMessage();
                    if (userId && username && joinReq) {
                        StringCopy(joinReq->userId, userId);
                        StringCopy(joinReq->username, username);
                        auto discriminator = GetStrMember(user, "discriminator");
                        if (discriminator) {
                            StringCopy(joinReq->discriminator, discriminator);
                        }
                        if (avatar) {
                            StringCopy(joinReq->avatar, avatar);
                        }
                        else {
                            joinReq->avatar[0] = 0;
                        }
                        JoinAskQueue.CommitAdd();
                    }
                }
            }
        }

        // writes
        if (Flag_TestAndClear(&UpdatePresence) && QueuedPresence.length) {
            QueuedMessage local;
            {
                SpinGuard g(&PresenceLock);
                local.Copy(QueuedPresence);
            }

            if (!Connection->Write(local.buffer, local.length)) {
                // if we fail to send, requeue
                {
                    SpinGuard g(&PresenceLock);
                    QueuedPresence.Copy(local);
                }
                Flag_Set(&UpdatePresence, true);
            }
        }

        while (SendQueue.HavePendingSends()) {
            auto qmessage = SendQueue.GetNextSendMessage();
            Connection->Write(qmessage->buffer, qmessage->length);
            SendQueue.CommitSend();
        }
    }
}

static void SignalIOActivity()
{
    if (IoThread != nullptr) {
        IoThread->Notify();
    }
}

static bool RegisterForEvent(const char* evtName)
{
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
            JsonWriteSubscribeCommand(qmessage->buffer, sizeof(qmessage->buffer), Nonce++, evtName);
        SendQueue.CommitAdd();
        SignalIOActivity();
        return true;
    }
    return false;
}

static bool DeregisterForEvent(const char* evtName)
{
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
            JsonWriteUnsubscribeCommand(qmessage->buffer, sizeof(qmessage->buffer), Nonce++, evtName);
        SendQueue.CommitAdd();
        SignalIOActivity();
        return true;
    }
    return false;
}

extern "C" DISCORD_EXPORT void Discord_Initialize(const char* applicationId,
    DiscordEventHandlers* handlers,
    int autoRegister,
    const char* optionalSteamId)
{
    if (autoRegister) {
        if (optionalSteamId && optionalSteamId[0]) {
            Discord_RegisterSteamGame(applicationId, optionalSteamId);
        }
        else {
            Discord_Register(applicationId, nullptr);
        }
    }

    Pid = GetProcessId();

    {
        SpinGuard g(&HandlerLock);

        if (handlers) {
            QueuedHandlers = *handlers;
        }
        else {
            QueuedHandlers = {};
        }

        Handlers = {};
    }

    if (Connection) {
        return;
    }

    Connection = RpcConnection::Create(applicationId);

    Connection->onConnect = [](JsonDocument& readyMessage) {
        Discord_UpdateHandlers(&QueuedHandlers);

        if (QueuedPresence.length > 0) {
            Flag_Set(&UpdatePresence, true);
            SignalIOActivity();
        }

        auto data = GetObjMember(&readyMessage, "data");
        auto user = GetObjMember(data, "user");
        auto userId = GetStrMember(user, "id");
        auto username = GetStrMember(user, "username");
        auto avatar = GetStrMember(user, "avatar");
        if (userId && username) {
            StringCopy(connectedUser.userId, userId);
            StringCopy(connectedUser.username, username);
            auto discriminator = GetStrMember(user, "discriminator");
            if (discriminator) {
                StringCopy(connectedUser.discriminator, discriminator);
            }
            if (avatar) {
                StringCopy(connectedUser.avatar, avatar);
            }
            else {
                connectedUser.avatar[0] = 0;
            }
        }

        Flag_Set(&WasJustConnected, true);
        ReconnectTimeMs.reset();
        };

    Connection->onDisconnect = [](int err, const char* message) {
        LastDisconnectErrorCode = err;
        StringCopy(LastDisconnectErrorMessage, message);
        Flag_Set(&WasJustDisconnected, true);
        UpdateReconnectTime();
        };

    // initialize reconnect tick immediately (avoid chrono)
    UpdateReconnectTime();

    IoThread->Start();
}

extern "C" DISCORD_EXPORT void Discord_Shutdown(void)
{
    if (!Connection) {
        return;
    }

    Connection->onConnect = nullptr;
    Connection->onDisconnect = nullptr;

    {
        SpinGuard g(&HandlerLock);
        Handlers = {};
        QueuedHandlers = {};
    }

    {
        SpinGuard g(&PresenceLock);
        QueuedPresence.length = 0;
    }
    Flag_Set(&UpdatePresence, false);

    if (IoThread != nullptr) {
        IoThread->Stop();
    }

    RpcConnection::Destroy(Connection);
    Connection = nullptr;
}

extern "C" DISCORD_EXPORT void Discord_UpdatePresence(const DiscordRichPresence* presence)
{
    {
        SpinGuard g(&PresenceLock);
        QueuedPresence.length = JsonWriteRichPresenceObj(
            QueuedPresence.buffer, sizeof(QueuedPresence.buffer), Nonce++, Pid, presence);
        Flag_Set(&UpdatePresence, true);
    }
    SignalIOActivity();
}

extern "C" DISCORD_EXPORT void Discord_ClearPresence(void)
{
    Discord_UpdatePresence(nullptr);
}

extern "C" DISCORD_EXPORT void Discord_Respond(const char* userId, /* DISCORD_REPLY_ */ int reply)
{
    if (!Connection || !Connection->IsOpen()) {
        return;
    }
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
            JsonWriteJoinReply(qmessage->buffer, sizeof(qmessage->buffer), userId, reply, Nonce++);
        SendQueue.CommitAdd();
        SignalIOActivity();
    }
}

extern "C" DISCORD_EXPORT void Discord_RunCallbacks(void)
{
    if (!Connection) {
        return;
    }

    bool wasDisconnected = Flag_TestAndClear(&WasJustDisconnected);
    bool isConnected = Connection->IsOpen();

    if (isConnected) {
        SpinGuard g(&HandlerLock);
        if (wasDisconnected && Handlers.disconnected) {
            Handlers.disconnected(LastDisconnectErrorCode, LastDisconnectErrorMessage);
        }
    }

    if (Flag_TestAndClear(&WasJustConnected)) {
        SpinGuard g(&HandlerLock);
        if (Handlers.ready) {
            DiscordUser du{ connectedUser.userId,
                           connectedUser.username,
                           connectedUser.discriminator,
                           connectedUser.avatar };
            Handlers.ready(&du);
        }
    }

    if (Flag_TestAndClear(&GotErrorMessage)) {
        SpinGuard g(&HandlerLock);
        if (Handlers.errored) {
            Handlers.errored(LastErrorCode, LastErrorMessage);
        }
    }

    if (Flag_TestAndClear(&WasJoinGame)) {
        SpinGuard g(&HandlerLock);
        if (Handlers.joinGame) {
            Handlers.joinGame(JoinGameSecret);
        }
    }

    if (Flag_TestAndClear(&WasSpectateGame)) {
        SpinGuard g(&HandlerLock);
        if (Handlers.spectateGame) {
            Handlers.spectateGame(SpectateGameSecret);
        }
    }

    while (JoinAskQueue.HavePendingSends()) {
        auto req = JoinAskQueue.GetNextSendMessage();
        {
            SpinGuard g(&HandlerLock);
            if (Handlers.joinRequest) {
                DiscordUser du{ req->userId, req->username, req->discriminator, req->avatar };
                Handlers.joinRequest(&du);
            }
        }
        JoinAskQueue.CommitSend();
    }

    if (!isConnected) {
        SpinGuard g(&HandlerLock);
        if (wasDisconnected && Handlers.disconnected) {
            Handlers.disconnected(LastDisconnectErrorCode, LastDisconnectErrorMessage);
        }
    }
}

extern "C" DISCORD_EXPORT void Discord_UpdateHandlers(DiscordEventHandlers* newHandlers)
{
    if (newHandlers) {
#define HANDLE_EVENT_REGISTRATION(handler_name, event)              \
    if (!Handlers.handler_name && newHandlers->handler_name) {      \
        RegisterForEvent(event);                                    \
    }                                                               \
    else if (Handlers.handler_name && !newHandlers->handler_name) { \
        DeregisterForEvent(event);                                  \
    }

        SpinGuard g(&HandlerLock);
        HANDLE_EVENT_REGISTRATION(joinGame, "ACTIVITY_JOIN")
            HANDLE_EVENT_REGISTRATION(spectateGame, "ACTIVITY_SPECTATE")
            HANDLE_EVENT_REGISTRATION(joinRequest, "ACTIVITY_JOIN_REQUEST")

#undef HANDLE_EVENT_REGISTRATION

            Handlers = *newHandlers;
    }
    else {
        SpinGuard g(&HandlerLock);
        Handlers = {};
    }
    return;
}
