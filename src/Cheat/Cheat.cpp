#include "Cheat.hpp"
#include "Notify/Notify.hpp"
#include <Main/Draw/Draw.hpp>
#include <Main/Draw/Silent.hpp>
#include <Main/Memory/Memory.hpp>
#include <thread>
#include <iostream>
#include <Windows.h>
#include <XorStr.hpp>
#include "FullScreenFix.hpp"
#include "SharedMemory.h"
#include "saveconfig.cpp"
// Auth saiu da DLL - driver valida direto em kernel.
// #include <ext/Auth/LicenseValidator.h>
#include <DynamicStub/DynamicStub.hpp>
#include <bypass/closehandle.cpp>
#include <VehCpuHook/VehCpuHook.hpp>
#include <Chams/Chams.hpp>
#include <VehPGDHook/Vehpageguardhook.hpp>

extern HMODULE g_hModule;
extern HANDLE g_MainFinishedEvent;
extern HANDLE g_MainThreadHandle;

extern "C" __declspec(dllexport) volatile PVOID g_CommandBufferPtr = nullptr;
extern "C" __declspec(dllexport) volatile PVOID g_WebStatePtr = nullptr;

static volatile LONG g_UnloadRequested = 0;


void SetStreamMode( HWND hwnd, bool enable )
{
	PVOID ptr = ( PVOID ) g_CommandBufferPtr;
	if ( !ptr ) return;

	PSHARED_COMMAND_BUFFER pCmd = ( PSHARED_COMMAND_BUFFER ) ptr;
	if ( pCmd->Magic != CMD_MAGIC ) return;

	int retries = 0;
	while ( pCmd->CommandType != CMD_NONE && pCmd->Processed == 0 && retries < 100 )
	{
		Sleep( 10 );
		retries++;
	}

	pCmd->Processed = 0;
	pCmd->Param1 = ( UINT64 ) ( ULONG_PTR ) hwnd;
	pCmd->Param2 = enable ? 0x11 : 0x00;
	MemoryBarrier( );
	pCmd->CommandType = CMD_STREAM;
}

bool ProcessDriverCommands( HWND overlayHwnd, bool& captureBypassState )
{
	PVOID ptr = ( PVOID ) g_CommandBufferPtr;
	if ( !ptr ) return false;

	PSHARED_COMMAND_BUFFER pCmd = ( PSHARED_COMMAND_BUFFER ) ptr;
	if ( pCmd->Magic != CMD_MAGIC ) return false;
	if ( pCmd->Processed == 1 ) return false;
	if ( pCmd->CommandType == CMD_NONE ) return false;

	switch ( pCmd->CommandType )
	{
	case CMD_STREAM:
		if ( pCmd->Param1 == 0 )
		{
			pCmd->Param1 = ( UINT64 ) ( ULONG_PTR ) overlayHwnd;
			MemoryBarrier( );

			captureBypassState = ( pCmd->Param2 == 0x11 );
			g_Globals.General.CaptureBypass = captureBypassState;

			return true;
		}
		return false;

	case CMD_UNLOAD:
		InterlockedExchange( &g_UnloadRequested, 1 );
		pCmd->Processed = 1;
		return true;

	default:
		return false;
	}
}

// Aplica features recebidas pelo painel web em g_Globals.
// Chamado todo frame. Soh tem efeito se Authenticated=1 e Magic confere.
void ApplyWebFeatures( )
{
	PVOID ptr = ( PVOID ) g_WebStatePtr;
	if ( !ptr ) return;

	PSHARED_WEB_STATE p = ( PSHARED_WEB_STATE ) ptr;
	if ( p->Magic != WEB_MAGIC ) return;
	// Quando nao autenticado, ignora escritas do driver - g_Globals segue
	// o que a Interface local (ou o LoadConfig) setou.
	if ( !p->Authenticated ) return;

	const WEB_FEATURES& f = p->Features;

	// Aimbot
	g_Globals.AimBot.Enabled         = f.Aimbot_Enabled != 0;
	g_Globals.AimBot.Fov             = ( int ) f.Aimbot_Fov;
	g_Globals.AimBot.MaxDistance     = ( int ) f.Aimbot_MaxDistance;
	g_Globals.AimBot.Target          = ( int ) f.Aimbot_Target;
	g_Globals.AimBot.IgnoreBots      = f.Aimbot_IgnoreBots != 0;
	g_Globals.AimBot.IgnoreKnocked   = f.Aimbot_IgnoreKnocked != 0;
	g_Globals.AimBot.VisibleCheck    = f.Aimbot_VisibleCheck != 0;
	g_Globals.AimBot.aimmagnect      = f.Aimbot_Pull != 0;
	g_Globals.AimBot.ghost           = f.Aimbot_Ghost != 0;
	g_Globals.Misc.Screen.ShowAimbotFov = f.Aimbot_Show_Fov != 0;
	// Guard: so sobrescreve se o driver ja setou algo (0 = buffer ainda virgem)
	if ( f.Aimbot_KeyBind != 0 )
		g_Globals.AimBot.KeyBind = ( int ) f.Aimbot_KeyBind;

	// Silent
	g_Globals.Silent.Enabled         = f.Silent_Enabled != 0;
	g_Globals.Silent.Fov             = ( int ) f.Silent_Fov;
	g_Globals.Silent.MaxDistance     = ( int ) f.Silent_MaxDistance;
	g_Globals.Misc.Screen.ShowSilentFov = f.Silent_Show_Fov != 0;

	// ESP / Visuals
	g_Globals.Visuals.ESP.Enemy        = f.ESP_Enemy != 0;
	g_Globals.Visuals.ESP.ShowTeam     = f.ESP_ShowTeam != 0;
	g_Globals.Visuals.ESP.Watermark    = f.ESP_Watermark != 0;
	g_Globals.Visuals.ESP.Box          = f.ESP_Box != 0;
	// Forca style valido quando liga pelo web (default do Globals eh 0=None).
	if ( g_Globals.Visuals.ESP.Box && g_Globals.Visuals.ESP.BoxStyle == 0 )
		g_Globals.Visuals.ESP.BoxStyle = 1; // Full

	g_Globals.Visuals.ESP.BoxFilled    = f.ESP_BoxFilled != 0;
	g_Globals.Visuals.ESP.ShowName     = f.ESP_ShowName != 0;

	g_Globals.Visuals.ESP.HealthBar    = f.ESP_HealthBar != 0;
	if ( g_Globals.Visuals.ESP.HealthBar && g_Globals.Visuals.ESP.HealthBarStyle == 0 )
		g_Globals.Visuals.ESP.HealthBarStyle = 1; // Left

	g_Globals.Visuals.ESP.Distance     = f.ESP_Distance != 0;
	g_Globals.Visuals.ESP.Skeleton     = f.ESP_Skeleton != 0;

	g_Globals.Visuals.ESP.Weapon       = f.ESP_Weapon != 0;
	if ( g_Globals.Visuals.ESP.Weapon && g_Globals.Visuals.ESP.WeaponStyle == 0 )
		g_Globals.Visuals.ESP.WeaponStyle = 1; // Text

	g_Globals.Visuals.ESP.SnapLines    = f.ESP_SnapLines != 0;
	if ( g_Globals.Visuals.ESP.SnapLines && g_Globals.Visuals.ESP.SnapLinesPos == 0 )
		g_Globals.Visuals.ESP.SnapLinesPos = 1; // Top
	g_Globals.Visuals.ESP.RenderDistance = ( int ) f.ESP_RenderDistance;
	// Thickness no driver vem como int*10 (1..30 -> 0.1..3.0)
	g_Globals.Visuals.ESP.Thickness    = ( float ) f.ESP_Thickness / 10.0f;
	g_Globals.Visuals.ESP.TextSize     = ( float ) f.ESP_TextSize;

	// Chams
	g_Globals.Visuals.Chams.Enabled        = f.Chams_Enabled != 0;
	g_Globals.Visuals.Chams.AggressiveMode = f.Chams_AggressiveMode != 0;

	// Exploits
	g_Globals.Misc.Exploits.LocalPlayer.AimLock2x     = f.Exp_AimLock2x != 0;
	g_Globals.Misc.Exploits.LocalPlayer.AimbotAwm     = f.Exp_AimbotAwm != 0;
	g_Globals.Misc.Exploits.LocalPlayer.NoRecoil      = f.Exp_NoRecoil != 0;
	g_Globals.Misc.Exploits.LocalPlayer.RecoilControl = ( int ) f.Exp_RecoilControl;
	g_Globals.Misc.Exploits.LocalPlayer.FastMedkit    = f.Exp_FastMedkit != 0;
	g_Globals.Misc.Exploits.LocalPlayer.telaparada    = f.Exp_TelaParada != 0;
	g_Globals.Misc.Exploits.LocalPlayer.AtributarArma = f.Exp_AtributarArma != 0;
	g_Globals.Misc.Exploits.LocalPlayer.AtributarArmaLevel = ( int ) f.Exp_AtributarArmaLevel;
	g_Globals.Misc.Exploits.LocalPlayer.Aimlock       = f.Exp_Aimlock != 0;
	g_Globals.Misc.Exploits.LocalPlayer.MoreDamage    = f.Exp_MoreDamage != 0;
	g_Globals.Misc.Exploits.LocalPlayer.FireDelay     = f.Exp_FireDelay != 0;
	g_Globals.Misc.Exploits.LocalPlayer.BugarPixel    = f.Exp_BugarPixel != 0;
	g_Globals.Misc.Exploits.LocalPlayer.Precision     = f.Exp_Precision != 0;
	g_Globals.Misc.Exploits.LocalPlayer.BackJump      = f.Exp_BackJump != 0;
	g_Globals.Misc.Exploits.LocalPlayer.SocoLonge     = f.Exp_SocoLonge != 0;
	g_Globals.Misc.Exploits.LocalPlayer.SpinBot       = f.Exp_SpinBot != 0;
	// SpinSpeed no driver vem como int*10 (10..50 -> 1.0..5.0)
	g_Globals.Misc.Exploits.LocalPlayer.SpinSpeed     = ( float ) f.Exp_SpinSpeed / 10.0f;

	// General
	if ( f.Gen_ThreadDelay >= 30 && f.Gen_ThreadDelay <= 240 )
		g_Globals.General.ThreadDelay = ( int ) f.Gen_ThreadDelay;
}

// Auth migrada pro driver (validacao 100% kernel).
// As funcoes abaixo ficam comentadas como fallback historico.
//
// #if 0
// void PublishHwidOnce( ) { ... }
// void ProcessWebLoginRequest( ) { ... }
// #endif

// Inicializa a memoria do FreeFire assim que o driver sinaliza Authenticated=1.
// Antes era feito pela UI de login; agora eh puxado automaticamente aqui, 1x
// so, quando percebermos auth ok.
static void EnsureFreeFireMemoryInitialized( )
{
	static volatile LONG initDone = 0;
	PVOID ptr = ( PVOID ) g_WebStatePtr;
	if ( !ptr ) return;
	PSHARED_WEB_STATE p = ( PSHARED_WEB_STATE ) ptr;
	if ( p->Magic != WEB_MAGIC ) return;
	if ( !p->Authenticated ) return;

	if ( InterlockedCompareExchange( &initDone, 1, 0 ) == 0 )
	{
		g_FreeFireMemory.Initialize( );
		g_Globals.General.EnableFuncs = true;
		// NotifyManager::Send( XorStr( "Autenticado" ), 2500 ); // notifies removidos
	}
}

// Processa acoes one-shot (save/load/restart) enfileiradas pelo web.
void ProcessWebPendingAction( )
{
	PVOID ptr = ( PVOID ) g_WebStatePtr;
	if ( !ptr ) return;

	PSHARED_WEB_STATE p = ( PSHARED_WEB_STATE ) ptr;
	if ( p->Magic != WEB_MAGIC ) return;
	if ( !p->Authenticated ) return;

	ULONG seq = p->Features.PendingActionSeq;
	if ( seq == p->Features.PendingActionAcked ) return;

	ULONG action = p->Features.PendingAction;
	switch ( action )
	{
	case WEB_ACTION_SAVE_CFG:
		Cheat::Manager::Save( );
		// NotifyManager::Send( XorStr( "Config salva via web" ), 3000 );
		break;
	case WEB_ACTION_LOAD_CFG:
		Cheat::Manager::Load( );
		// NotifyManager::Send( XorStr( "Config carregada via web" ), 3000 );
		break;
	case WEB_ACTION_RESTART:
		std::thread( g_FreeFireMemory.Restart ).detach( );
		// NotifyManager::Send( XorStr( "Restart via web" ), 3000 );
		break;
	}

	p->Features.PendingActionAcked = seq;
	MemoryBarrier( );
}


static void PrepareForUnload( )
{
	// Avisa o loader para limpar os rastros (arquivo da DLL, config)
	HANDLE hUnloadEvent = OpenEventW( EVENT_MODIFY_STATE, FALSE, L"HwMonEvt" );
	if ( hUnloadEvent )
	{
		SetEvent( hUnloadEvent );
		CloseHandle( hUnloadEvent );
	}

	// Silent pode ainda estar rodando se foi ativado fora do loop principal
	Silent::Stop( );

	if ( g_MainFinishedEvent )
	{
		CloseHandle( g_MainFinishedEvent );
		g_MainFinishedEvent = nullptr;
	}

	if ( g_MainThreadHandle )
	{
		CloseHandle( g_MainThreadHandle );
		g_MainThreadHandle = nullptr;
	}

	// Desliga Chams (remove 2 hooks GL via HWBP, libera 2 slots DR)
	Chams::Shutdown( );
	Sleep( 250 );

	// Remove hooks do FullScreenFix (PAGE_GUARD, NAO consome DR)
	RemoveFullScreenFixHooks( );
	Sleep( 250 );

	// Desliga VEH+HWBP completamente
	VehCpuHook::Shutdown( );
	Sleep( 250 );

	Data::StopReadThread( );

	// Remove o log diagnostico do TEMP (rastro da sessao)
	{
		wchar_t tmp [ MAX_PATH ] = { };
		if ( GetTempPathW( MAX_PATH, tmp ) )
			DeleteFileW( ( std::wstring( tmp ) + L"HwMon.log" ).c_str( ) );
	}

	if ( g_Interface )
	{
		delete g_Interface;
		g_Interface = nullptr;
	}

	Console::ShutdownConsole( );

	DynamicStub::Shutdown( );
	CloseUnknownExitThreadHandles( );
	StopHandleMonitor( );
	Utils::DisableDebugPrivilege( );

	if ( g_Globals.General.Local )
	{
		free( g_Globals.General.Local );
		g_Globals.General.Local = nullptr;
	}

	// Sinaliza para o driver que esta pronto para o unload
	PVOID ptr = ( PVOID ) g_CommandBufferPtr;
	if ( ptr )
	{
		PSHARED_COMMAND_BUFFER pCmd = ( PSHARED_COMMAND_BUFFER ) ptr;
		pCmd->DllReady = 1;
		MemoryBarrier( );
	}

	if ( g_hModule )
	{
		FreeLibraryAndExitThread( g_hModule, 0 );
	}
}

namespace Cheat
{
	void Initialize( )
	{
		Utils::EnableDebugPrivilege( );
		CloseUnknownExitThreadHandles( );
		VehCpuHook::Initialize( );
		FullScreenFixHooks( );
		Overlay::Setup( Render::LookupWindowByClassName( ) );
		Overlay::Initialize( );

		DEVMODE devMode = { };
		devMode.dmSize = sizeof( DEVMODE );
		if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &devMode ) )
		{
			int refreshRate = devMode.dmDisplayFrequency;
			g_Globals.General.ThreadDelay = ( refreshRate > 0 && refreshRate < 1000 ) ? refreshRate : 240;
		}
		else
		{
			g_Globals.General.ThreadDelay = 240;
		}

		if ( Overlay::IsInitialized( ) )
		{
			g_Interface = new Interface( Overlay::GetOverlayWindow( ), Overlay::GetTargetWindow( ), Overlay::glGetDeviceContext( ), Overlay::glGetContext( ) );
			g_Interface->UpdateStyle( );
			Overlay::SetupWindowProcHook( std::bind( &Interface::WindowProc, g_Interface, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4 ) );

			MSG Message{ };
			ZeroMemory( &Message, sizeof( Message ) );

			bool CaptureBypassOn = true;
			bool StreamModeApplied = false;

			auto lastFrame = std::chrono::high_resolution_clock::now( );

			while ( Message.message != WM_QUIT )
			{
				if ( InterlockedCompareExchange( &g_UnloadRequested, 0, 0 ) == 1 )
				{
					PrepareForUnload( );
					return;
				}

				if ( g_Globals.General.ShutDown )
				{
					PrepareForUnload( );
					return;
				}

				if ( PeekMessage( &Message, Overlay::GetOverlayWindow( ), 0, 0, PM_REMOVE ) )
				{
					TranslateMessage( &Message );
					DispatchMessage( &Message );
				}

				// Cursor desenhado pelo ImGui aparece quando o menu estiver aberto.
				ImGui::GetIO( ).MouseDrawCursor = g_Interface->GetMenuOpen( );

				if ( g_Interface->ResizeWidht != 0 || g_Interface->ResizeHeight != 0 )
				{
					g_Interface->ResizeWidht = g_Interface->ResizeHeight = 0;
				}

				g_Interface->HandleMenuKey( );
				Overlay::UpdateWindowPos( );

				// Stream mode: aplica WDA_EXCLUDEFROMCAPTURE direto em user-mode no
				// overlay (igual ao cheat de referencia). Nao depende do driver.
				static bool CaptureBypassApplied = false;
				if ( g_Globals.General.CaptureBypass != CaptureBypassApplied )
				{
					CaptureBypassApplied = g_Globals.General.CaptureBypass;
					SetWindowDisplayAffinity( Overlay::GetOverlayWindow( ), CaptureBypassApplied ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE );
				}

				if ( g_CommandBufferPtr )
				{
					ProcessDriverCommands( Overlay::GetOverlayWindow( ), CaptureBypassOn );

					// Web panel bypass: login e configuracao sao feitos no menu local ImGui
					// EnsureFreeFireMemoryInitialized( );
					// ApplyWebFeatures( );
					// ProcessWebPendingAction( );

					// Stream mode via interface local (checkbox)
					if ( !StreamModeApplied && g_Globals.General.CaptureBypass )
					{
						CaptureBypassOn = true;
						StreamModeApplied = true;
						SetStreamMode( Overlay::GetOverlayWindow( ), true );
					}
					else if ( g_Globals.General.CaptureBypass != CaptureBypassOn )
					{
						CaptureBypassOn = g_Globals.General.CaptureBypass;
						StreamModeApplied = true;
						SetStreamMode( Overlay::GetOverlayWindow( ), CaptureBypassOn );
					}
				}

				if ( g_Globals.Visuals.Chams.Enabled && !Chams::IsEnabled( ) )
				{
					Chams::Enable( );
				}
				else if ( !g_Globals.Visuals.Chams.Enabled && Chams::IsEnabled( ) )
				{
					Chams::Disable( );
				}

				if ( g_Globals.Silent.Enabled && !InterlockedCompareExchange( &Silent::g_Running, 0, 0 ) )
				{
					Silent::Start( );
				}
				else if ( !g_Globals.Silent.Enabled && InterlockedCompareExchange( &Silent::g_Running, 0, 0 ) )
				{
					Silent::Stop( );
				}

				ImGui_ImplOpenGL3_NewFrame( );
				ImGui_ImplWin32_NewFrame( );
				ImGui::NewFrame( );
				{
					RECT rc{ };
					GetClientRect( Overlay::GetTargetWindow( ), &rc );

					Data::Draw( rc.right - rc.left, rc.bottom - rc.top, g_Globals.General.N32, g_Globals.General.V31 );

					// Render do menu clássico ImGui
					g_Interface->RenderGui( );
				   NotifyManager::Render( );   // toasts removidos conforme pedido

					// FOV Circle
					if ( g_Globals.Misc.Screen.ShowAimbotFov )
					{
						ImColor Outline( g_Globals.Misc.Screen.AimbotFovColor [ 0 ], g_Globals.Misc.Screen.AimbotFovColor [ 1 ], g_Globals.Misc.Screen.AimbotFovColor [ 2 ], g_Globals.Misc.Screen.AimbotFovColor [ 3 ] );
						ImColor Fill( g_Globals.Misc.Screen.FilledFovColor [ 0 ], g_Globals.Misc.Screen.FilledFovColor [ 1 ], g_Globals.Misc.Screen.FilledFovColor [ 2 ], g_Globals.Misc.Screen.FilledFovColor [ 3 ] );
						const ImVec2 Center( ImGui::GetIO( ).DisplaySize.x * 0.5f, ImGui::GetIO( ).DisplaySize.y * 0.5f );
						ImGui::GetBackgroundDrawList( )->AddCircleFilled( Center, g_Globals.AimBot.Fov, Fill, 360 );
						ImGui::GetBackgroundDrawList( )->AddCircle( Center, g_Globals.AimBot.Fov, Outline, 360 );
					}

					if ( g_Globals.Misc.Screen.ShowSilentFov )
					{
						ImColor Outline( g_Globals.Misc.Screen.SilentFovColor [ 0 ], g_Globals.Misc.Screen.SilentFovColor [ 1 ], g_Globals.Misc.Screen.SilentFovColor [ 2 ], g_Globals.Misc.Screen.SilentFovColor [ 3 ] );
						ImColor Fill( g_Globals.Misc.Screen.SilentFilledFovColor [ 0 ], g_Globals.Misc.Screen.SilentFilledFovColor [ 1 ], g_Globals.Misc.Screen.SilentFilledFovColor [ 2 ], g_Globals.Misc.Screen.SilentFilledFovColor [ 3 ] );
						const ImVec2 Center( ImGui::GetIO( ).DisplaySize.x * 0.5f, ImGui::GetIO( ).DisplaySize.y * 0.5f );
						ImGui::GetBackgroundDrawList( )->AddCircleFilled( Center, g_Globals.Silent.Fov, Fill, 360 );
						ImGui::GetBackgroundDrawList( )->AddCircle( Center, g_Globals.Silent.Fov, Outline, 360 );
					}

				}
				ImGui::EndFrame( );
				ImGui::Render( );

				glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
				glClear( GL_COLOR_BUFFER_BIT );
				ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData( ) );
				Overlay::glRefresh( );

				const double targetFPS = ( g_Globals.General.ThreadDelay > 0 ) ? static_cast< double >( g_Globals.General.ThreadDelay ) : 60.0;
				const double frameDuration = 1000.0 / targetFPS;

				auto now = std::chrono::high_resolution_clock::now( );
				double elapsed = std::chrono::duration<double, std::milli>( now - lastFrame ).count( );

				if ( elapsed < frameDuration )
				{
					std::this_thread::sleep_for( std::chrono::milliseconds( static_cast< long long >( frameDuration - elapsed ) ) );
				}
				lastFrame = std::chrono::high_resolution_clock::now( );
			}

			Data::StopReadThread( );

			if ( g_Interface )
			{
				delete g_Interface;
				g_Interface = nullptr;
			}

			Console::ShutdownConsole( );
		}
	}

}
