#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <Windows.h>
#endif

#define CMD_MAGIC   0x444D435A

#define CMD_NONE    0
#define CMD_STREAM  1
#define CMD_UNLOAD  2

#pragma pack(push, 1)
typedef struct _SHARED_COMMAND_BUFFER {
    ULONG Magic;
    ULONG CommandType;
    UINT64 Param1;
    ULONG Param2;
    ULONG Processed;
    ULONG DllReady;
    SIZE_T DllImageSize;
    ULONG Reserved[2];
} SHARED_COMMAND_BUFFER, * PSHARED_COMMAND_BUFFER;
#pragma pack(pop)

extern "C" __declspec(dllexport) extern volatile PVOID g_CommandBufferPtr;

// =========================================================================
// SHARED_WEB_STATE - mirror de g_Globals controlado pelo painel web
// Driver escreve Features, DLL (HwMonCore) lê no loop e aplica em g_Globals.
// Authenticated: DLL seta 1 apos ValidateLicense(); driver recusa /cfg e
// /cmd de feature toggles se 0.
// =========================================================================

#define WEB_MAGIC   0x42455742  // 'BWEB'

// PendingAction values
#define WEB_ACTION_NONE     0
#define WEB_ACTION_SAVE_CFG 1
#define WEB_ACTION_LOAD_CFG 2
#define WEB_ACTION_RESTART  3

#pragma pack(push, 1)
typedef struct _WEB_FEATURES {
    // ---- Aimbot ----
    ULONG Aimbot_Enabled;
    ULONG Aimbot_Fov;               // 0-360
    ULONG Aimbot_MaxDistance;       // 0-200
    ULONG Aimbot_Target;            // 0=Neck 1=Legit
    ULONG Aimbot_IgnoreBots;
    ULONG Aimbot_IgnoreKnocked;
    ULONG Aimbot_VisibleCheck;
    ULONG Aimbot_Pull;              // aimmagnect
    ULONG Aimbot_Ghost;
    ULONG Aimbot_Show_Fov;          // Misc.Screen.ShowAimbotFov
    ULONG Aimbot_KeyBind;           // VK code (0x01=LBUTTON default)

    // ---- Silent ----
    ULONG Silent_Enabled;
    ULONG Silent_Fov;               // 0-360
    ULONG Silent_MaxDistance;       // 0-200
    ULONG Silent_Show_Fov;          // Misc.Screen.ShowSilentFov

    // ---- ESP / Visuals ----
    ULONG ESP_Enemy;
    ULONG ESP_ShowTeam;
    ULONG ESP_Watermark;
    ULONG ESP_Box;
    ULONG ESP_BoxFilled;
    ULONG ESP_ShowName;
    ULONG ESP_HealthBar;
    ULONG ESP_Distance;
    ULONG ESP_Skeleton;
    ULONG ESP_Weapon;
    ULONG ESP_SnapLines;
    ULONG ESP_RenderDistance;       // 0-140
    ULONG ESP_Thickness;            // 1-30 (valor*10, representa 0.1-3.0)
    ULONG ESP_TextSize;             // 10-20 direto

    // ---- Chams ----
    ULONG Chams_Enabled;
    ULONG Chams_AggressiveMode;

    // ---- Exploits ----
    ULONG Exp_AimLock2x;
    ULONG Exp_AimbotAwm;
    ULONG Exp_NoRecoil;
    ULONG Exp_RecoilControl;        // 0-100
    ULONG Exp_FastMedkit;
    ULONG Exp_TelaParada;
    ULONG Exp_AtributarArma;
    ULONG Exp_AtributarArmaLevel;
    ULONG Exp_Aimlock;
    ULONG Exp_MoreDamage;
    ULONG Exp_FireDelay;
    ULONG Exp_BugarPixel;
    ULONG Exp_Precision;
    ULONG Exp_BackJump;
    ULONG Exp_SocoLonge;
    ULONG Exp_SpinBot;
    ULONG Exp_SpinSpeed;            // 10-50 (valor*10, representa 1.0-5.0)

    // ---- General ----
    ULONG Gen_ThreadDelay;          // 30-240
    ULONG Gen_StreamMode;           // espelho de General.CaptureBypass

    // ---- One-shot actions (save/load/restart) ----
    ULONG PendingAction;            // WEB_ACTION_*
    ULONG PendingActionSeq;         // driver incrementa
    ULONG PendingActionAcked;       // DLL ecoa

    // ---- Auth via web (validacao offline roda em user-mode na DLL) ----
    CHAR  PendingLoginKey[512];     // driver escreve a license base64
    ULONG PendingLoginSeq;          // driver incrementa quando entrega nova key
    ULONG PendingLoginAcked;        // DLL ecoa quando terminou de validar
    ULONG PendingLoginResult;       // 0=pending, 1=ok, 2=fail
    ULONG PendingLoginPremium;      // 0/1 (bit de premium retornado)

    // HWID publicado pela DLL ao iniciar (GetHardwareId do LicenseValidator)
    CHAR  HwidString[64];
    ULONG HwidSet;                  // 0=ainda nao preencheu, 1=ok
} WEB_FEATURES, * PWEB_FEATURES;

typedef struct _SHARED_WEB_STATE {
    ULONG Magic;                    // WEB_MAGIC
    ULONG Authenticated;            // DLL seta 1 apos login ok
    ULONG ConfigSeq;                // driver incrementa a cada write
    ULONG FeatureBitsOut;           // (reservado) status DLL->driver
    WEB_FEATURES Features;
} SHARED_WEB_STATE, * PSHARED_WEB_STATE;
#pragma pack(pop)

extern "C" __declspec(dllexport) extern volatile PVOID g_WebStatePtr;
