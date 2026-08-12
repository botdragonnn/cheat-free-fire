#pragma once
#include <Windows.h>
#include <imgui.h>
#include <Main/Memory/Memory.hpp>
#include <Main/Offsets/Offsets.hpp>
#include <Main/Unity/Unity.hpp>
#include <Math/Vectors/Vector3.hpp>
#include <Cheat/Globals.hpp>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <XorStr.hpp>

enum PlayerType
{
	PLAYER_UNKNOWN = 0,
	PLAYER_NETWORK,
	PLAYER,
	PLAYER_TRAINING_HUMAN,
	PLAYER_TRAINING_STAND
};

struct PlayerData
{
	Vector3 HeadScreen;
	Vector3 FeetScreen;
	// Posicoes no mundo (usadas para reprojetar na tela com a view matrix mais
	// recente a cada frame, mesmo quando a leitura de entidades engasga — assim
	// o ESP continua "grudado" nos jogadores em vez de congelar/desligar).
	Vector3 HeadWorld;
	Vector3 FeetWorld;
	float HealthPercent;
	bool IsKnocked;
	int WeaponID;
	uintptr_t Entity;
	uintptr_t UMAData;
	std::string Name;
	float Distance;
	short CurrentHealth;
	short MaxHealth;
};

struct GameContext
{
	uintptr_t LocalPlayer;
	uintptr_t MatchGame;
	uintptr_t Match;
	uintptr_t MainCamera;
	Matrix4x4 ViewMatrix;
	bool IsObserving;
};

class Data
{
	public:
	static void StartReadThread( );
	static void StopReadThread( );
	static void Draw( int width, int height, bool N32, bool V31 );
	static GameContext GetContext( );

	static void DrawBox( float x, float y, float w, float h, ImColor color, ImColor fillColor, float thickness, int Type );
	static void DrawWeapon( int WeaponID, bool IsKnocked, Vector3 HeadPos, float Height );
	static void DrawHealthBar( short CurrentHealth, short MaxHealth, ImVec2 HeadPos, ImVec2 EntityPos, float Width, float Height, int Entity );
	static void DrawSnapLine( const Vector3& HeadPos, const Vector3& EntityPos, bool showName, bool healthTop, ImColor color, float thickness, int type );
	static void SpinBot( uintptr_t LocalPlayer, bool N32 );

	static PlayerType GetPlayerType( uintptr_t entity, bool N32 )
	{
		uintptr_t klass = N32 ? g_FreeFireMemory.Read<uint32_t>( entity ) : g_FreeFireMemory.Read<uint64_t>( entity );
		if ( klass == 0 ) return PLAYER_UNKNOWN;

		uintptr_t namePtr = N32 ? g_FreeFireMemory.Read<uint32_t>( klass + 0x8 ) : g_FreeFireMemory.Read<uint64_t>( klass + 0x10 );
		if ( namePtr == 0 ) return PLAYER_UNKNOWN;

		std::string name = g_FreeFireMemory.String( namePtr, 32 );

		if ( name == XorStr("PlayerNetwork") ) return PLAYER_NETWORK;
		if ( name == XorStr("PlayerUGCCommon") ) return PLAYER;
		if ( name == XorStr("Player_TrainingHumanTarget") ) return PLAYER_TRAINING_HUMAN;
		if ( name == XorStr("Player_TrainingHumanTarget_Stand") ) return PLAYER_TRAINING_STAND;

		return PLAYER_UNKNOWN;
	}

	private:
	template <bool N32, bool V31>
	static void ReadLoop( );

	static std::vector<PlayerData> m_Players;
	static GameContext m_Context;
	static std::mutex m_Mutex;
	static std::atomic<bool> m_Running;
	static HANDLE m_ThreadHandle;
	// true quando m_Players/m_Context vieram de um frame de leitura que
	// completou com sucesso. Em falha transitória o snapshot antigo é mantido
	// (ESP continua desenhando), mas fica "não fresco" para o aimbot não mirar
	// em posição congelada.
	static bool m_SnapshotFresh;
	// Tick (GetTickCount64) do último frame com snapshot fresco — usado pelo
	// Draw para nunca desenhar posições congeladas por mais que ~3s (transição
	// real de partida), como o cheat de referência que limpa as entidades.
	static std::atomic<LONGLONG> m_LastFreshTick;

	template <bool N32, bool V31>
	static DWORD WINAPI ReadLoopWrapper( LPVOID );
};
