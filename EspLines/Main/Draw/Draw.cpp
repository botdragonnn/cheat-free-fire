#include "Draw.hpp"
#include "Silent.hpp"
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <Cheat/Globals.hpp>
#include <Main/Unity/Unity.hpp>
#include <Main/Unity/UTF/UTF8.hpp>
#include <DirectXMath.h>
#include <Render/Fonts/Fonts.hpp>
#include "Weapon/NameGun.h"
#include <Utils/Utils.hpp>
#include <Math/Quaternion/Quaternion.hpp>
#include <Math/MathUtils.hpp>
#include "Skeleton.hpp"
#include <DynamicStub/DynamicStub.hpp>

// Static members
std::vector<PlayerData> Data::m_Players;
GameContext Data::m_Context{ };
std::mutex Data::m_Mutex;
std::atomic<bool> Data::m_Running{ false };
HANDLE Data::m_ThreadHandle = nullptr;
bool Data::m_SnapshotFresh = false;

// Uma view-projection matrix valida tem elementos finitos e magnitude normal.
// Uma leitura "rasgada" (o jogo escreve os 64 bytes enquanto lemos) ou de um
// camera invalida costuma vir com NaN/Inf, tudo ~0 ou valores absurdos.
// Rejeitar essa matrix mantem a ultima camera boa em uso — o ESP nunca
// reprojeta com lixo e, portanto, nunca "some" por causa de um frame ruim.
static bool IsValidViewMatrix( const Matrix4x4& m )
{
	for ( int i = 0; i < 16; i++ )
	{
		if ( !std::isfinite( m.v [ i ] ) )
			return false;
	}
	float s = 0.f;
	for ( int r = 0; r < 4; r++ )
	{
		for ( int c = 0; c < 4; c++ )
			s += m.m [ r ][ c ] * m.m [ r ][ c ];
	}
	return ( s > 1e-4f && s < 1e12f );
}

// Constants
constexpr float kNameOffset = 10.0f;
constexpr float kHealthOffsetBase = 8.0f;
constexpr float kHealthBarHeight = 2.5f;
constexpr float kStackGap = 2.0f;

// Globals
static bool IsCursorVisibleNow( )
{
	CURSORINFO ci = { };
	ci.cbSize = sizeof( ci );
	if ( GetCursorInfo( &ci ) )
	{
		return ( ci.flags & 0x00000001 ) != 0;
	}
	return true;
}

bool ghostActive = false;
bool ghostSkeletonExists = false;
Vector3 ghostSkeletonPos = { 0, 0, 0 };
uintptr_t RageTarget = 0;
bool enemiesvisible = true;

static std::vector<float> g_SmoothedHealth;

// ==================== ESP Overlay Helper ====================
// Desenha o overlay de um jogador a partir do snapshot. Quando uma view matrix
// valida e passada, reprojeta as posicoes de mundo (mantendo o ESP grudado nos
// jogadores mesmo quando a leitura de entidades engasga — como no cheat de
// referencia). Com matrix invalida (restart/transicao) usa as posicoes de tela
// congeladas do snapshot. Nenhuma leitura viva e feita aqui; o unico trecho que
// depende de memoria ao vivo e o skeleton (desativado via drawSkeleton=false
// quando a base esta invalida).
static void DrawEspEntityOverlay( const PlayerData& p, ImDrawList* DL, const struct Cheat::Globals::Visuals::ESP& ESP, const Matrix4x4& ViewMatrix, bool N32, bool V31, bool drawSkeleton )
{
	if ( ESP.RenderDistance > 0 && p.Distance > ESP.RenderDistance )
		return;

	Vector3 headScreen = p.HeadScreen;
	Vector3 feetScreen = p.FeetScreen;

	if ( ViewMatrix.m [ 0 ][ 0 ] != 0.f && p.HeadWorld != Vector3::Zero( ) )
	{
		headScreen = W2S::World2Screen( ViewMatrix, p.HeadWorld );
		feetScreen = W2S::World2Screen( ViewMatrix, p.FeetWorld );
		if ( headScreen.Z <= 0 || feetScreen.Z <= 0 )
			return;
	}
	else if ( headScreen == Vector3::Zero( ) || feetScreen == Vector3::Zero( ) )
	{
		// Sem view matrix valida e a entidade nunca projetou (snapshot gravado
		// com matrix ruim): nao ha o que desenhar neste frame.
		return;
	}

	const float Height = fabsf( feetScreen.Y - headScreen.Y );
	const float Width = Height * 0.5f;

	// --- Name ---
	if ( ESP.ShowName )
	{
		ImColor nameColor = p.IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f ) : ImColor( ESP.NameColor [ 0 ], ESP.NameColor [ 1 ], ESP.NameColor [ 2 ], ESP.NameColor [ 3 ] );
		ImVec2 TextSize = Utils::CalcTextSize( Fonts::Gff, ESP.TextSize, p.Name.c_str( ) );
		ImVec2 NamePos( ( headScreen.X - Width * 0.5f ) + ( Width * 0.5f ) - ( TextSize.x * 0.5f ), headScreen.Y - kNameOffset );
		DL->AddText( Fonts::Gff, ESP.TextSize, NamePos, nameColor, p.Name.c_str( ) );
	}

	// --- Box ---
	if ( ESP.Box )
	{
		ImColor Color = p.IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f ) : ImColor( ESP.BoxColor [ 0 ], ESP.BoxColor [ 1 ], ESP.BoxColor [ 2 ], ESP.BoxColor [ 3 ] );
		ImColor ColorFilled = p.IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f ) : ImColor( ESP.FilledBoxColor [ 0 ], ESP.FilledBoxColor [ 1 ], ESP.FilledBoxColor [ 2 ], ESP.FilledBoxColor [ 3 ] );
		Data::DrawBox( headScreen.X - Width * 0.5f, headScreen.Y, Width, feetScreen.Y - headScreen.Y, Color, ColorFilled, ESP.Thickness, ESP.BoxStyle );
	}

	// --- SnapLine ---
	if ( ESP.SnapLines )
	{
		ImColor Color = p.IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f ) : ImColor( ESP.SnapLinesColor [ 0 ], ESP.SnapLinesColor [ 1 ], ESP.SnapLinesColor [ 2 ], ESP.SnapLinesColor [ 3 ] );
		const bool healthTop = ( ESP.HealthBarStyle == 3 );
		Data::DrawSnapLine( headScreen, feetScreen, ESP.ShowName, healthTop, Color, ESP.Thickness, ESP.SnapLinesPos );
	}

	// --- HealthBar ---
	if ( ESP.HealthBar )
	{
		Data::DrawHealthBar( p.CurrentHealth, p.MaxHealth, ImVec2( headScreen.X, headScreen.Y ), ImVec2( feetScreen.X, feetScreen.Y ), Width, Height, ( int )( uintptr_t )p.Entity );
	}

	// --- Weapon ---
	if ( ESP.Weapon )
	{
		Data::DrawWeapon( p.WeaponID, p.IsKnocked, headScreen, Height );
	}

	// --- Distance ---
	if ( ESP.Distance )
	{
		int mRounded = ( int )( p.Distance + 0.5f );
		char distanceText [ 16 ];
		snprintf( distanceText, sizeof( distanceText ), XorStr( "%dm" ), mRounded );

		ImGui::PushFont( Fonts::Verdana );
		ImColor Color = p.IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f ) : ImColor( ESP.DistanceColor [ 0 ], ESP.DistanceColor [ 1 ], ESP.DistanceColor [ 2 ], ESP.DistanceColor [ 3 ] );
		ImVec2 sz = ImGui::CalcTextSize( distanceText );
		DL->AddText( Fonts::Verdana, ESP.TextSize, ImVec2( headScreen.X - sz.x * 0.5f, feetScreen.Y + 5 ), Color, distanceText );
		ImGui::PopFont( );
	}

	// --- Skeleton ---
	if ( drawSkeleton )
		Skeleton::DrawPlayer( DL, p.Entity, p.UMAData, p.IsKnocked, ViewMatrix, N32, V31 );
}

// ==================== Read Thread ====================

template <bool N32, bool V31>
DWORD WINAPI Data::ReadLoopWrapper( LPVOID )
{
	ReadLoop<N32, V31>( );
	return 0;
}

void Data::StartReadThread( )
{
	if ( m_Running.load( ) ) return;
	m_Running.store( true );

	bool N32 = g_Globals.General.N32;
	bool V31 = g_Globals.General.V31;

	HANDLE h = nullptr;
	if ( N32 && V31 ) h = DynamicStub::CreateThreadWithDynamicStub( ReadLoopWrapper<true, true>, nullptr );
	else if ( N32 && !V31 ) h = DynamicStub::CreateThreadWithDynamicStub( ReadLoopWrapper<true, false>, nullptr );
	else if ( !N32 && !V31 ) h = DynamicStub::CreateThreadWithDynamicStub( ReadLoopWrapper<false, false>, nullptr );
	else if ( !N32 && V31 ) h = DynamicStub::CreateThreadWithDynamicStub( ReadLoopWrapper<false, true>, nullptr );

	if ( !h )
	{
		// Falha ao criar a thread: nao deixa m_Running travado em true,
		// senao nenhum restart futuro conseguiria recriar a ReadLoop.
		m_ThreadHandle = nullptr;
		m_Running.store( false );
		return;
	}

	m_ThreadHandle = h;
}

void Data::StopReadThread( )
{
	m_Running.store( false );
	HANDLE h = m_ThreadHandle;
	m_ThreadHandle = nullptr;
	if ( h )
	{
		WaitForSingleObject( h, INFINITE );
		CloseHandle( h );
	}
}

template <bool N32, bool V31>
void Data::ReadLoop( )
{
	int failCount = 0;
	int lobbyFrames = 0;
	int emptyFrames = 0;
	while ( m_Running.load( ) && !g_Globals.General.ShutDown )
	{
		std::vector<PlayerData> tempPlayers;
		GameContext tempCtx{ };

		// fresh          = o frame inteiro (facade->partida->entidades) leu ok
		// readMatchState = conseguiu ler o estado da partida
		// matchActive    = a partida está ativa
		// matchRead      = conseguiu ler o ponteiro do Match (sinal confiavel
		//                  de "estamos numa partida"; false = cadeia quebrou
		//                  no nivel de partida/lobby/facade)
		bool fresh = false;
		bool readMatchState = false;
		bool matchActive = false;
		bool matchRead = false;

		do
		{
			Memory::FlushTLB( );

			// Revalida o CR3 do processo a cada ~64 iteracoes. Se o processo
			// do jogo reiniciou no meio da sessao, o CR3 antigo envenena TODAS
			// as leituras de uma vez (ESP + aimbot + silent param juntos). O
			// check de ELF magic nao detecta isso quando a pagina antiga ainda
			// esta mapeada — so a revalidacao direta do pgd pega.
			static int cR3Tick = 0;
			if ( ( ++cR3Tick & 0x3F ) == 0 )
				Memory::RefreshCR3( );

			// Valida a base do modulo ANTES de qualquer leitura. Quando o
			// processo do jogo reinicia (ex: alt-tab no emulador) o ASLR muda a
			// base e o magic ELF some — mesmo que leituras de ponteiro voltem
			// lixo nao-zero (que antes nao era detectado). Com base invalida:
			// mantem o snapshot congelado (ESP nao some) e agenda restart apos
			// falha sustentada.
			uint32_t ElfMagic = 0;
			if ( !g_FreeFireMemory.Read<uint32_t>( Offsets::LibIl2Cpp, ElfMagic ) || ElfMagic != 0x464C457F )
			{
				if ( ++failCount > 150 )
				{
					failCount = 0;
					DiagLog( "[diag] ELF fail: restart async apos %d frames com base invalida", 150 );
					g_FreeFireMemory.RestartAsync( );
				}
				break;
			}
			failCount = 0;

			auto ReadPtr = [ ] ( uintptr_t addr ) -> uintptr_t
			{
				return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
			};

			uintptr_t GameFacade = ReadPtr( Offsets::LibIl2Cpp + Offsets::GameFacade::GameFacade_TypeInfo );
			if ( GameFacade == 0 ) break;

			uintptr_t AccessClass = ReadPtr( GameFacade + Offsets::AccessClass );
			if ( AccessClass == 0 ) break;

			uintptr_t MatchGame = ReadPtr( AccessClass + Offsets::GameFacade::CurrentMatchGame );
			if ( MatchGame == 0 ) break;

			uintptr_t Match = ReadPtr( MatchGame + Offsets::MatchGame::m_Match );
			if ( Match == 0 ) break;
			matchRead = true;

			int MatchRaw = g_FreeFireMemory.Read<int>( Match + Offsets::Match::m_State );
			auto MatchState = static_cast< Offsets::MatchState >( MatchRaw );
			readMatchState = true;
			if ( !Offsets::IsMatchActive( MatchState ) ) break;
			matchActive = true;

			uintptr_t LocalObserver = ReadPtr( Match + Offsets::Match::m_LocalObserver );
			bool IsObserving = ( LocalObserver != 0 );
			uintptr_t LocalPlayer;
			if ( LocalObserver != 0 )
			{
				LocalPlayer = ReadPtr( LocalObserver + Offsets::Observer::m_TargetPlayer );
			}
			else
			{
				LocalPlayer = ReadPtr( Match + Offsets::Match::m_LocalPlayer );
			}
			if ( LocalPlayer == 0 ) break;

			tempCtx.LocalPlayer = LocalPlayer;
			tempCtx.MatchGame = MatchGame;
			tempCtx.Match = Match;
			tempCtx.IsObserving = IsObserving;

			uintptr_t MainCamera = ReadPtr( LocalPlayer + Offsets::Player::MainCameraTransform );
			tempCtx.MainCamera = MainCamera;

			uintptr_t m_CameraControllerManager = ReadPtr( MatchGame + Offsets::MatchGame::m_CameraControllerManager );
			if ( m_CameraControllerManager == 0 ) break;

			uintptr_t m_Camera = ReadPtr( m_CameraControllerManager + Offsets::CameraControllerManager::m_Camera );
			if ( m_Camera == 0 ) break;

			uintptr_t m_CachedPtr = ReadPtr( m_Camera + Offsets::Camera::m_CachedPtr );
			if ( m_CachedPtr == 0 ) break;

			Matrix4x4 ViewMatrix = g_FreeFireMemory.Read<Matrix4x4>( m_CachedPtr + Offsets::Camera::ViewMatrix );
			if ( !IsValidViewMatrix( ViewMatrix ) ) break;
			tempCtx.ViewMatrix = ViewMatrix;

			auto EntityList = reinterpret_cast< Offsets::UnityList<N32>* >( ReadPtr( Match + Offsets::Match::m_AttackableEntities ) );
			if ( EntityList == nullptr ) break;

			int dictCount = EntityList->GetSize( );
			if ( dictCount <= 0 || dictCount > 200 ) break;

			tempPlayers.reserve( dictCount );

			for ( int i = 0; i < dictCount; i++ )
			{
				uintptr_t Entity = EntityList->GetItem( i );
				if ( Entity == 0 ) continue;

				PlayerType type = Data::GetPlayerType( Entity, N32 );
				if ( type == PLAYER_UNKNOWN ) continue;

				uintptr_t m_AvatarManager = ReadPtr( Entity + Offsets::Player::m_AvatarManager );
				if ( m_AvatarManager == 0 ) continue;

				uintptr_t m_Avatar = ReadPtr( m_AvatarManager + Offsets::AvatarManager::m_Avatar );
				if ( m_Avatar == 0 ) continue;

				uintptr_t UMAData = ReadPtr( m_Avatar + Offsets::UMAAvatarBase::umaData );
				if ( UMAData == 0 ) continue;

				int TransformType = g_FreeFireMemory.Read<int>( Entity + Offsets::Player::m_TransformType );
				bool IsVisible = g_FreeFireMemory.Read<bool>( m_Avatar + Offsets::UmaAvatarSimple::IsVisible );
				if ( IsVisible == 0 && TransformType == 0 ) continue;

				bool IsTeam = g_FreeFireMemory.Read<bool>( UMAData + Offsets::UMAData::isTeammate );
				if ( IsTeam ) continue;

				uintptr_t m_PRIDataPoolPtr = ReadPtr( Entity + Offsets::ReplicationEntity::m_PRIDataPool );
				if ( m_PRIDataPoolPtr == 0 ) continue;

				uintptr_t dataArrayPtr = ReadPtr( m_PRIDataPoolPtr + Offsets::ReplicationEntity::m_Datas );
				if ( dataArrayPtr == 0 ) continue;

				uintptr_t CurrentHealthptr = ReadPtr( dataArrayPtr + Offsets::ReplicationEntity::HealthCurrentPtr );
				uintptr_t MaxHealthptr = ReadPtr( dataArrayPtr + Offsets::ReplicationEntity::HealthMaxPtr );
				if ( CurrentHealthptr == 0 || MaxHealthptr == 0 ) continue;

				bool IsKnocked = false;
				uintptr_t ShadowBase = ReadPtr( Entity + Offsets::PlayerNetwork::m_ShadowState );
				if ( ShadowBase != 0 )
				{
					int PlayerPose = g_FreeFireMemory.Read<int>( ShadowBase + Offsets::ShadowState::TargetPhysXPose );
					IsKnocked = ( PlayerPose == 8 );
				}

				int CurrentHealth = g_FreeFireMemory.Read<int>( CurrentHealthptr + Offsets::ReplicationEntity::Value );
				int MaxHealth = g_FreeFireMemory.Read<int>( MaxHealthptr + Offsets::ReplicationEntity::Value );
				if ( MaxHealth == 0 || CurrentHealth <= 0 ) continue;

				float HealthPercent = ( float )CurrentHealth / ( float )MaxHealth;

				uintptr_t WeaponPtr = ReadPtr( dataArrayPtr + Offsets::ReplicationEntity::WeaponPtr );
				if ( WeaponPtr == 0 ) continue;

				int WeaponID = g_FreeFireMemory.Read<int>( WeaponPtr + Offsets::ReplicationEntity::Value );

				// Name
				std::string nameStr = "BOT";
				bool IsClientBot = false;
				g_FreeFireMemory.Read<bool>( Entity + Offsets::Player::IsClientBot, IsClientBot );
				if ( !IsClientBot )
				{
					uintptr_t profilePtr = ReadPtr( Entity + Offsets::PlayerNetwork::m_Profile );
					if ( profilePtr != 0 )
					{
						uintptr_t PlayerName = ReadPtr( profilePtr + Offsets::BaseProfileInfo::NickName );
						if ( PlayerName != 0 )
						{
							if constexpr ( N32 )
							{
								nameStr = ObterStr( PlayerName + 0xC, g_FreeFireMemory.Read<int>( PlayerName + 0x8 ) );
							}
							else
							{
								nameStr = ObterStr( PlayerName + 0x14, g_FreeFireMemory.Read<int>( PlayerName + 0x10 ) );
							}
							if ( nameStr.empty( ) )
							{
								nameStr = XorStr( "BOT" );
							}
						}
					}
				}

				Vector3 PosHeadEntity = Transform::GetHeadPosition( Entity, N32 );
				if ( PosHeadEntity == Vector3::Zero( ) ) continue;

				Vector3 PosEntity = Transform::GetPosition( Entity, N32 );
				if ( PosEntity == Vector3::Zero( ) ) continue;

				Vector3 MainPos = ( MainCamera != 0 ) ? Transform::get_position_Injected( MainCamera, N32 ) : Vector3::Zero( );
				float Distancia = ( MainPos != Vector3::Zero( ) ) ? Vector3::Distance( PosEntity, MainPos ) : 0.0f;

				Vector3 HeadWorld = PosHeadEntity + ( Vector3::Up( ) * 0.20f );
				Vector3 FeetWorld = PosEntity + ( Vector3::Down( ) * 0.1f );

				// Projecao de leitura NAO pode descartar a entidade: se a view
				// matrix estiver ruim/rasgada por um frame (escrita concorrente do
				// jogo), descartar todas as entidades esvazia o snapshot e o ESP
				// some e volta. Guarda o mundo sempre; a projecao em tela fica
				// como fallback (congelado) e a reprojecao acontece no desenho.
				Vector3 HeadPos = W2S::World2Screen( ViewMatrix, HeadWorld );
				Vector3 EntityPos = W2S::World2Screen( ViewMatrix, FeetWorld );
				bool projOk = ( HeadPos.Z > 0 && EntityPos.Z > 0 );

				PlayerData pd;
				pd.HeadScreen = projOk ? HeadPos : Vector3::Zero( );
				pd.FeetScreen = projOk ? EntityPos : Vector3::Zero( );
				pd.HeadWorld = HeadWorld;
				pd.FeetWorld = FeetWorld;
				pd.HealthPercent = HealthPercent;
				pd.IsKnocked = IsKnocked;
				pd.WeaponID = WeaponID;
				pd.Entity = Entity;
				pd.UMAData = UMAData;
				pd.Name = nameStr;
				pd.Distance = Distancia;
				pd.CurrentHealth = ( short )CurrentHealth;
				pd.MaxHealth = ( short )MaxHealth;
				tempPlayers.push_back( pd );
			}

			fresh = true;
		}
		while ( false );

		if ( fresh )
		{
			lobbyFrames = 0;
			std::lock_guard<std::mutex> lock( m_Mutex );
			// Contexto/camera sempre atualiza — mesmo quando o snapshot e
			// segurado, o ESP reprojeta as posicoes de mundo com a camera atual.
			m_Context = tempCtx;
			if ( tempPlayers.empty( ) && !m_Players.empty( ) )
			{
				// Um frame "fresco" que veio vazio (todos os inimigos filtrados
				// por leitura transitoria) NAO pode apagar o ESP. So aceita o
				// vazio apos N frames seguidos — saida legitima de todos os
				// inimigos / fim de partida. Enquanto segura, marca o snapshot
				// como nao-fresco para o aimbot nao mirar em posicao antiga.
				//
				// N=900 (~3-6s) cobre a janela de CR3 obsoleto: quando o
				// processo do jogo reinicia, leituras lixo podem produzir
				// frames "frescos e vazios" por ate ~1-2s (ate o RefreshCR3
				// recapturar o pgd novo). Se o limiar fosse menor, o wipe
				// acontecia DURANTE essa janela — o bug real do "ESP some".
				// Um fim de partida legitimo ainda limpa apos esses frames.
				if ( ++emptyFrames >= 900 )
				{
					// JA estamos sob o lock de m_Mutex acima (linha do
					// std::lock_guard externo); relock do mesmo mutex nao
					// recursivo aqui = deadlock/hard-freeze do processo.
					emptyFrames = 0;
					DiagLog( "[diag] empty-clear: snapshot apagado apos %d frames vazios frescos", 900 );
					m_Players.swap( tempPlayers );
					m_SnapshotFresh = true;
				}
				else
				{
					m_SnapshotFresh = false;
				}
			}
			else
			{
				emptyFrames = 0;
				m_Players.swap( tempPlayers );
				m_SnapshotFresh = true;
			}
		}
		else if ( readMatchState && !matchActive )
		{
			// m_State fora de [1..3] NUNCA limpa nada. No meio da partida isso
			// acontece em transicoes de rodada/re-instancia do Match (estado
			// legitimo de inatividade por alguns segundos) e com leitura lixo
			// (CR3 obsoleta). Limpar aqui era o que fazia ESP + aimbot + silent
			// desligarem JUNTOS no meio da partida. So marca nao-fresco
			// (aimbot desliga; o ESP segue desenhando o snapshot).
			lobbyFrames = 0;
			std::lock_guard<std::mutex> lock( m_Mutex );
			m_SnapshotFresh = false;
		}
		else
		{
			// Falha de leitura (GameFacade==0 no meio da partida, loading,
			// reinicio do processo, CR3 obsoleta): mantém o ultimo snapshot
			// congelado para o ESP nao sumir, mas marca como nao-fresco para o
			// aimbot nao mirar em posicao antiga/morta.
			//
			// O unico sinal confiavel de "saiu da partida" e a cadeia quebrar
			// ANTES de ler o Match (GameFacade/MatchGame == 0). Se o Match foi
			// lido, e falha de entidade/localPlayer NO MEIO DA PARTIDA: nunca
			// limpa — segura. So limpa apos um periodo longo e sustentado sem
			// Match (lobby real / jogo fechado), liberando a memoria para a
			// proxima partida.
			if ( !matchRead )
			{
				// Janela longa (~1200 frames ≈ 4-8s): cobre lobby real e
				// transicoes de partida; uma leitura lixo temporaria nao
				// consegue manter 1200 frames sem Match.
				if ( ++lobbyFrames >= 1200 )
				{
					lobbyFrames = 0;
					DiagLog( "[diag] lobby-clear: snapshot+contexto zerados apos %d frames sem Match", 1200 );
					std::lock_guard<std::mutex> lock( m_Mutex );
					m_Players.clear( );
					m_Context = GameContext{ };
					m_SnapshotFresh = false;
				}
				else
				{
					std::lock_guard<std::mutex> lock( m_Mutex );
					m_SnapshotFresh = false;
				}
			}
			else
			{
				lobbyFrames = 0;
				std::lock_guard<std::mutex> lock( m_Mutex );
				m_SnapshotFresh = false;
				// Se a leitura chegou ate a camera/view matrix antes de falhar,
				// atualiza o contexto com ela — o ESP reprojeta as posicoes de
				// mundo congeladas com a camera mais recente, ficando grudado
				// nos inimigos mesmo durante o engasgo da leitura de entidades.
				if ( tempCtx.ViewMatrix.m [ 0 ][ 0 ] != 0.f )
					m_Context = tempCtx;
			}
		}

		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
	}
}

template void Data::ReadLoop<true, false>( );    // v24.1 32-bit
template void Data::ReadLoop<false, false>( );   // v24.1 64-bit
template void Data::ReadLoop<true, true>( );     // v31 32-bit
template void Data::ReadLoop<false, true>( );    // v31 64-bit

template DWORD WINAPI Data::ReadLoopWrapper<true, false>( LPVOID );
template DWORD WINAPI Data::ReadLoopWrapper<false, false>( LPVOID );
template DWORD WINAPI Data::ReadLoopWrapper<true, true>( LPVOID );
template DWORD WINAPI Data::ReadLoopWrapper<false, true>( LPVOID );

GameContext Data::GetContext( )
{
	std::lock_guard<std::mutex> lock( m_Mutex );
	return m_Context;
}

void Data::Draw( int width, int height, bool N32, bool V31 )
{
	Memory::FlushTLB( );

	if ( g_Globals.General.EnableFuncs == 0 ) return;

	// Sempre que a base/contexto nao permitir leituras vivas, desenha o overlay
	// do ultimo snapshot congelado (posicoes de tela) para o ESP nunca sumir.
	// O skeleton e desativado nesse caminho pois precisa de memoria ao vivo.
	auto DrawFrozenEsp = [ ] ( )
	{
		const auto& ESPc = g_Globals.Visuals.ESP;
		ImDrawList* DLc = ImGui::GetForegroundDrawList( );

		std::vector<PlayerData> frozen;
		{
			std::lock_guard<std::mutex> lock( m_Mutex );
			frozen = m_Players;
		}

		for ( const auto& p : frozen )
			DrawEspEntityOverlay( p, DLc, ESPc, Matrix4x4{ }, false, false, false );
	};

	if ( Offsets::LibIl2Cpp == 0 )
	{
		// Relocaliza em background (single-flight + cooldown) sem travar
		// a thread de render nem empilhar restarts.
		g_FreeFireMemory.RestartAsync( );

		static LONGLONG lastBaseLog = 0;
		LONGLONG now = GetTickCount64( );
		if ( now - lastBaseLog > 1000 )
		{
			lastBaseLog = now;
			DiagLog( "[diag] base==0: DrawFrozenEsp, snapshot=%d", ( int )m_Players.size( ) );
		}
		DrawFrozenEsp( );
		return;
	}

	auto ReadPtr = [ N32 ] ( uintptr_t addr ) -> uintptr_t
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
	};

	auto WritePtr = [ N32 ] ( uintptr_t addr, uintptr_t val )
	{
		N32 ? g_FreeFireMemory.Write<uint32_t>( addr, ( uint32_t )val ) : g_FreeFireMemory.Write<uint64_t>( addr, ( uint64_t )val );
	};

	ScreenWidth = width;
	ScreenHeight = height;

	const auto& ESP = g_Globals.Visuals.ESP;
	const auto& AimCfg = g_Globals.AimBot;
	const float fovSq = AimCfg.Fov * AimCfg.Fov;

	ImDrawList* DL = ImGui::GetForegroundDrawList( );
	const ImVec2 screenCenter( ( float )ScreenWidth * 0.5f, ( float )ScreenHeight * 0.5f );

	float ClosestDistSq = FLT_MAX;
	uintptr_t ClosestEntity = 0;
	short ClosestHP = 0;

	int enemyCountFrame = 0;
	ImVec2 closestHead2D( 0.f, 0.f );
	float minDistance2Dsq = FLT_MAX;

	int _lastEnemyCount = -1;
	std::string _cachedEnemyText = XorStr( "Enemies Detected: 0" );

	static Matrix4x4 CurrentMatrix;

	static uintptr_t BS_LastTarget = 0;
	static uintptr_t BS_SavedNeck = 0;
	static uintptr_t BS_SavedHip = 0;
	static uintptr_t BS_NeckAddr = 0;
	static uintptr_t BS_HipAddr = 0;
	static bool BS_Applied = false;
	static bool BS_ActiveByCursor = false;

	auto BS_Restore = [ & ] ( )
	{
		if ( BS_Applied && BS_LastTarget && BS_NeckAddr && BS_HipAddr )
		{
			WritePtr( BS_NeckAddr, BS_SavedNeck );
			WritePtr( BS_HipAddr, BS_SavedHip );
		}
		BS_Applied = false;
		BS_LastTarget = 0;
		BS_SavedNeck = 0;
		BS_SavedHip = 0;
		BS_NeckAddr = 0;
		BS_HipAddr = 0;
	};

	auto BS_Apply = [ & ] ( uintptr_t ent ) -> bool
	{
		if ( ent == 0 ) return false;

		uintptr_t neckAddr = ent + Offsets::Player::m_HipNode;
		uintptr_t hipAddr = ent + Offsets::Player::m_BloodEffectNode;
		if ( g_Globals.AimBot.Target == 1 )
			hipAddr = ent + Offsets::Player::m_RightArmNode;

		uintptr_t neckVal = ReadPtr( neckAddr );
		uintptr_t hipVal = ReadPtr( hipAddr );
		if ( neckVal == 0 || hipVal == 0 ) return false;

		BS_NeckAddr = neckAddr;
		BS_HipAddr = hipAddr;
		BS_SavedNeck = neckVal;
		BS_SavedHip = hipVal;

		WritePtr( neckAddr, hipVal );
		WritePtr( hipAddr, neckVal );

		BS_LastTarget = ent;
		BS_Applied = true;
		return true;
	};

	if ( !AimCfg.Enabled && BS_Applied )
	{
		BS_Restore( );
		BS_ActiveByCursor = false;
	}

	uintptr_t GameVar_TI = ReadPtr( Offsets::LibIl2Cpp + Offsets::GameVarDef::GameVarDef_TypeInfo );
	uintptr_t GameVar = ReadPtr( GameVar_TI + Offsets::AccessClass );

	// --- BugarPixel ---
	if ( GameVar != 0 )
	{
		static bool lastBugarPixelState = false;
		float currentValue = g_FreeFireMemory.Read<float>( GameVar + Offsets::GameVarDef::ShootTraceAdjustmentDistanceThreshold );
		if ( g_Globals.Misc.Exploits.LocalPlayer.BugarPixel )
		{
			if ( currentValue != 0.0f )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::ShootTraceAdjustmentDistanceThreshold, 0.0f );
		}
		else
		{
			if ( lastBugarPixelState )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::ShootTraceAdjustmentDistanceThreshold, 1.5f );
		}
		lastBugarPixelState = g_Globals.Misc.Exploits.LocalPlayer.BugarPixel;

		// --- Precision ---
		static bool lastPrecisionState = false;
		if ( g_Globals.Misc.Exploits.LocalPlayer.Precision )
		{
			float curRotMin = g_FreeFireMemory.Read<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMin );
			float curRotMax = g_FreeFireMemory.Read<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMax );
			float curAimMin = g_FreeFireMemory.Read<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMin );
			float curAimMax = g_FreeFireMemory.Read<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMax );

			if ( curRotMin != 15.0625f )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMin, 15.0625f );
			if ( curRotMax != 3.40939e-05f )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMax, 3.40939e-05f );
			if ( curAimMin != 15.125f )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMin, 15.125f );
			if ( curAimMax != 3184.0f )
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMax, 3184.0f );
		}
		else
		{
			if ( lastPrecisionState )
			{
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMin, 15.0f );
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::RotationSensitivityMax, 35.0f );
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMin, 10.0f );
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::AimRotationSensitivityMax, 20.0f );
			}
		}
		lastPrecisionState = g_Globals.Misc.Exploits.LocalPlayer.Precision;

		// --- BackJump ---
		static bool LastBackJumpState = false;
		bool AccelerationOnFallingValue = g_FreeFireMemory.Read<bool>( GameVar + Offsets::GameVarDef::EnableAccelerationOnFalling );
		bool FallingSwapWeaponValue = g_FreeFireMemory.Read<bool>( GameVar + Offsets::GameVarDef::EnableLowFallingSwapWeapon );
		if ( g_Globals.Misc.Exploits.LocalPlayer.BackJump )
		{
			if ( AccelerationOnFallingValue == true || FallingSwapWeaponValue == false )
			{
				g_FreeFireMemory.Write<bool>( GameVar + Offsets::GameVarDef::EnableAccelerationOnFalling, false );
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::EnableLowFallingSwapWeapon, true );
			}
		}
		else
		{
			if ( LastBackJumpState )
			{
				g_FreeFireMemory.Write<bool>( GameVar + Offsets::GameVarDef::EnableAccelerationOnFalling, true );
				g_FreeFireMemory.Write<float>( GameVar + Offsets::GameVarDef::EnableLowFallingSwapWeapon, true );
			}
		}
		LastBackJumpState = g_Globals.Misc.Exploits.LocalPlayer.BackJump;
	}

	// ==================== Snapshot ====================

	std::vector<PlayerData> snapshot;
	GameContext ctx;
	bool snapshotFresh = false;
	{
		std::lock_guard<std::mutex> lock( m_Mutex );
		snapshot = m_Players;
		ctx = m_Context;
		snapshotFresh = m_SnapshotFresh;
	}

	uintptr_t localPlayer = ctx.LocalPlayer;
	uintptr_t MainCamera = ctx.MainCamera;
	Matrix4x4 ViewMatrix = ctx.ViewMatrix;
	bool IsObserving = ctx.IsObserving;

	if ( localPlayer == 0 || !IsValidViewMatrix( ViewMatrix ) )
	{
		// Contexto invalido (transicao de partida/lobby/loading): mesmo sem
		// localPlayer/view matrix, desenha o overlay do ultimo snapshot (posicoes
		// de tela congeladas) para o ESP nao desligar do nada. Exploits e aimbot
		// nao rodam nesse caminho.
		static LONGLONG lastFrozenLog = 0;
		LONGLONG now = GetTickCount64( );
		if ( now - lastFrozenLog > 1000 )
		{
			lastFrozenLog = now;
			DiagLog( "[diag] frozen: localPlayer=%llx matrixValid=%d snapshot=%d",
				( unsigned long long )localPlayer, IsValidViewMatrix( ViewMatrix ) ? 1 : 0, ( int )snapshot.size( ) );
		}
		DrawFrozenEsp( );
		return;
	}

	Silent::UpdateViewMatrix( ViewMatrix );

	// ==================== Ghost Skeleton Rendering ====================

	if ( ghostSkeletonExists )
	{
		Vector3 world = ghostSkeletonPos;
		Vector3 screen = W2S::World2Screen( ViewMatrix, world );

		if ( W2S::IsOnScreen( screen ) )
		{
			float size = 10.0f;
			ImColor color = ImColor( 0.f, 1.f, 1.f, 1.f );

			DL->AddLine( ImVec2( screen.X, screen.Y - size ), ImVec2( screen.X, screen.Y + size ), color, 2.0f );
			DL->AddLine( ImVec2( screen.X - size, screen.Y ), ImVec2( screen.X + size, screen.Y ), color, 2.0f );
		}
	}

	// ==================== FastMedkit ====================

	uintptr_t PlayerAttributes = ReadPtr( localPlayer + Offsets::Player::m_Attributes );

	// ==================== Atributar Armas ====================
	if (g_Globals.Misc.Exploits.LocalPlayer.AtributarArma)
	{
		if (PlayerAttributes != 0)
		{
			// Níveis interpolados do original(1.0) até o level max(0.75)
			float fireIntervalLevels[4] = { 0.90f, 0.85f, 0.75f, 0.65f };
			int level = g_Globals.Misc.Exploits.LocalPlayer.AtributarArmaLevel;
			if (level < 0 || level > 3) level = 0;
			
			g_FreeFireMemory.Write<float>(PlayerAttributes + Offsets::PlayerAttributes::m_FireIntervalScale, fireIntervalLevels[level]);
		}
	}
	else
	{
		if (PlayerAttributes != 0)
		{
			g_FreeFireMemory.Write<float>(PlayerAttributes + Offsets::PlayerAttributes::m_FireIntervalScale, 1.0f);
		}
	}
  	
  	// ==================== Ghost Toggle ====================

	bool ghostKeyHeld = ( GetAsyncKeyState( g_Globals.AimBot.ghostkey ) & 0x8000 );
	if ( g_Globals.AimBot.ghost )
	{
		if ( ghostKeyHeld && !ghostActive )
		{
			ghostActive = true;
			g_FreeFireMemory.Write<bool>( localPlayer + Offsets::Player::m_WaitForForceSync, true );
			ghostSkeletonPos = Transform::GetPosition( localPlayer, N32 );
			ghostSkeletonExists = true;
		}
		else if ( !ghostKeyHeld && ghostActive )
		{
			ghostActive = false;
			g_FreeFireMemory.Write<bool>( localPlayer + Offsets::Player::m_WaitForForceSync, false );
			ghostSkeletonExists = false;
		}
	}
	else
	{
		ghostActive = false;
		ghostSkeletonExists = false;
	}

	// ==================== TelaParada ====================
	if ( g_Globals.Misc.Exploits.LocalPlayer.telaparada )
	{
		uintptr_t userControl = ReadPtr( localPlayer + Offsets::Player::m_UserControl );
		if ( userControl != 0 )
		{
			uintptr_t axisDataArray = ReadPtr( userControl + Offsets::UserControlHandler::m_AxisData );
			if ( axisDataArray != 0 )
			{
				uintptr_t moveAxisData = ReadPtr( axisDataArray + ( N32 ? 0x10 : 0x20 ) );
				if ( moveAxisData != 0 )
				{
					bool isTouched = g_FreeFireMemory.Read<bool>( moveAxisData + Offsets::UserControlHandler::m_IsTouched );
					if ( isTouched )
					{
						g_FreeFireMemory.Write<bool>( userControl + Offsets::UserControlHandler::m_LockFingerInDashArea, false );
						g_FreeFireMemory.Write<bool>( userControl + Offsets::UserControlHandler::m_DashByMovingJoystick, true );
						g_FreeFireMemory.Write<int>( userControl + Offsets::UserControlHandler::m_FingerInDashArea, 1 );
					}
					else
					{
						g_FreeFireMemory.Write<int>( userControl + Offsets::UserControlHandler::m_FingerInDashArea, 0 );
					}
				}
			}
		}
	}

	// ==================== ESP Rendering Loop ====================

	if ( snapshot.empty( ) )
	{
		Skeleton::ClearCache( );
	}


	const float centerX = ( float )ScreenWidth * 0.5f;
	const float centerY = ( float )ScreenHeight * 0.5f;

	for ( size_t i = 0; i < snapshot.size( ); i++ )
	{
		const auto& p = snapshot [ i ];

		if ( ESP.RenderDistance > 0 && p.Distance > ESP.RenderDistance ) continue;

		DrawEspEntityOverlay( p, DL, ESP, ViewMatrix, N32, V31, true );

		// --- Enemy counter ---
		if ( ESP.Enemy )
		{
			enemyCountFrame++;
			float dx = p.HeadScreen.X - centerX;
			float dy = p.HeadScreen.Y - centerY;
			float d2 = dx * dx + dy * dy;
			if ( d2 < minDistance2Dsq && p.Distance < ESP.RenderDistance )
			{
				minDistance2Dsq = d2;
				closestHead2D = ImVec2( p.HeadScreen.X, p.HeadScreen.Y );
			}
		}

		// ==================== Aimbot target selection ====================
		// Só mira quando o snapshot é fresco (leitura do frame atual). Em falha
		// transitória o ESP continua desenhando a posição congelada, mas o
		// aimbot não trava em alvo antigo — evita tiro que "acerta" e não conta dano.

		if ( snapshotFresh && p.Distance >= 0 && p.Distance <= AimCfg.MaxDistance )
		{
			enemiesvisible = true;

			// Visible check
			if ( g_Globals.AimBot.VisibleCheck )
			{
				bool anyVisible = false;

				uintptr_t aimAssist = ReadPtr( localPlayer + Offsets::Player::m_AimAssist );
				if ( aimAssist != 0 )
				{
					uintptr_t targetInfo = ReadPtr( aimAssist + Offsets::AimAssistAutoLock::m_TargetHeuristic );
					if ( targetInfo != 0 )
						anyVisible = true;
				}

				uintptr_t aimAssistSighting = ReadPtr( localPlayer + Offsets::Player::m_AimAssistOnSighting );
				if ( aimAssistSighting != 0 )
				{
					uintptr_t targetInfo = ReadPtr( aimAssistSighting + Offsets::AimAssistAutoLock::m_TargetHeuristic );
					if ( targetInfo != 0 )
						anyVisible = true;
				}

				if ( !anyVisible )
				{
					enemiesvisible = false;
					continue;
				}
			}

			// Ignore bots and knocked
			bool IsClientBot = false;
			g_FreeFireMemory.Read<bool>( p.Entity + Offsets::Player::IsClientBot, IsClientBot );

			if ( ( !AimCfg.IgnoreKnocked || !p.IsKnocked ) && ( !AimCfg.IgnoreBots || !IsClientBot ) )
			{
				float dx = p.HeadScreen.X - centerX;
				float dy = p.HeadScreen.Y - centerY;
				float crosshairDistSq = dx * dx + dy * dy;

				if ( crosshairDistSq < fovSq && crosshairDistSq < ClosestDistSq )
				{
					ClosestDistSq = crosshairDistSq;
					ClosestEntity = p.Entity;
					ClosestHP = p.CurrentHealth;
				}
			}
		}
	} // end for entities

	// ==================== Skeleton Cleanup ====================
	{
		std::unordered_set<uintptr_t> activeEntities;
		for ( const auto& p : snapshot )
			activeEntities.insert( p.Entity );
		Skeleton::CleanupCache( activeEntities );
	}

	// ==================== Watermark ====================

	if ( ESP.Watermark )
	{
		ImGui::PushFont( Fonts::Verdana );

		const char* text =  "STORM CHEATS" ;
		float fontScale = ESP.TextSize / 15.0f;
		float fontSize = Fonts::Verdana->FontSize * fontScale;

		ImVec2 baseSize = ImGui::CalcTextSize( text );
		ImVec2 textSize( baseSize.x * fontScale, baseSize.y * fontScale );
		ImVec2 screenSize = ImGui::GetIO( ).DisplaySize;
		ImVec2 textPos( ( screenSize.x - textSize.x ) * 0.5f, 75.0f );

		ImColor textColor( ESP.WatermarkColor [ 0 ], ESP.WatermarkColor [ 1 ], ESP.WatermarkColor [ 2 ], ESP.WatermarkColor [ 3 ] );
		ImU32 shadowColor = 0xFF000000;

		DL->AddText( Fonts::Verdana, fontSize, ImVec2( textPos.x + 1, textPos.y + 1 ), shadowColor, text );
		DL->AddText( Fonts::Verdana, fontSize, textPos, textColor, text );

		ImGui::PopFont( );
	}

	// ==================== Enemy text ====================

	if ( ESP.Enemy )
	{
		if ( _lastEnemyCount != enemyCountFrame )
		{
			_cachedEnemyText = XorStr( "Enemies Detected: " ) + std::to_string( enemyCountFrame );
			_lastEnemyCount = enemyCountFrame;
		}

		ImGui::PushFont( Fonts::Verdana );

		float fontScale = ESP.TextSize / 15.0f;
		float fontSize = Fonts::Verdana->FontSize * fontScale;

		ImVec2 baseSize = ImGui::CalcTextSize( _cachedEnemyText.c_str( ) );
		ImVec2 textSize( baseSize.x * fontScale, baseSize.y * fontScale );
		ImVec2 screenSize = ImGui::GetIO( ).DisplaySize;
		ImVec2 textPos( ( screenSize.x - textSize.x ) * 0.5f, 75.0f + textSize.y + 3.0f );

		ImColor textColor( ESP.EnemyColor [ 0 ], ESP.EnemyColor [ 1 ], ESP.EnemyColor [ 2 ], ESP.EnemyColor [ 3 ] );
		ImU32 shadowColor = 0xFF000000;

		DL->AddText( Fonts::Verdana, fontSize, ImVec2( textPos.x + 1, textPos.y + 1 ), shadowColor, _cachedEnemyText.c_str( ) );
		DL->AddText( Fonts::Verdana, fontSize, textPos, textColor, _cachedEnemyText.c_str( ) );

		ImGui::PopFont( );
	}

	// ==================== Ghost Distance Text ====================

	if ( g_Globals.AimBot.ghost && ghostSkeletonExists )
	{
		Vector3 currentPos = Transform::GetPosition( localPlayer, N32 );
		float ghostDistance = Vector3::Distance( currentPos, ghostSkeletonPos );

		const char* ghostText = ( ghostDistance <= 4.60f ) ? ( "Ghost Damage: ON" ) : ( "Ghost Damage: FAKE" );
		ImColor textColor = ( ghostDistance <= 4.60f ) ? ImColor( 0.0f, 1.0f, 0.0f, 1.0f ) : ImColor( 1.0f, 0.0f, 0.0f, 1.0f );

		ImGui::PushFont( Fonts::Verdana );

		float fontScale = ESP.TextSize / 10.0f;
		float fontSize = Fonts::Verdana->FontSize * fontScale;

		ImVec2 baseSize = ImGui::CalcTextSize( ghostText );
		ImVec2 textSize( baseSize.x * fontScale, baseSize.y * fontScale );
		ImVec2 screenSize = ImGui::GetIO( ).DisplaySize;

		float yBase = 75.0f;
		float yOffset = textSize.y * 1.5f;
		ImVec2 textPos( ( screenSize.x - textSize.x ) * 0.5f, yBase + yOffset );
		textPos.y -= 2.5f;

		ImU32 shadowColor = 0xFF000000;

		DL->AddText( Fonts::Verdana, fontSize, ImVec2( textPos.x + 1, textPos.y + 1 ), shadowColor, ghostText );
		DL->AddText( Fonts::Verdana, fontSize, textPos, textColor, ghostText );

		ImGui::PopFont( );
	}

	// ==================== Magnet Aimbot ====================

	if ( AimCfg.aimmagnect )
	{
		static bool isHolding = false;
		static Vector3 lockedRootPos = { 0.f, 0.f, 0.f };
		static uintptr_t lockedMatrixAddr = 0;
		static std::thread magnetThread;

		bool keyPressed = ( GetAsyncKeyState( AimCfg.MagKey ) & 0x8000 );

		if ( keyPressed && !isHolding && ClosestEntity != 0 )
		{
			isHolding = true;

			Vector3 cameraPos = ( MainCamera != 0 ) ? Transform::get_position_Injected( MainCamera, N32 ) : Vector3::Zero( );
			Matrix4x4 invVP{ };
			if ( cameraPos == Vector3::Zero( ) || !MatrixUtils::Invert( ViewMatrix, invVP ) )
				goto magnet_end;

			Vector4 nearClip( 0.f, 0.f, 0.f, 1.f );
			Vector4 farClip( 0.f, 0.f, 1.f, 1.f );

			Vector4 worldNear4 = MatrixUtils::Multiply( nearClip, invVP );
			Vector4 worldFar4 = MatrixUtils::Multiply( farClip, invVP );

			if ( worldNear4.w == 0 || worldFar4.w == 0 )
				goto magnet_end;

			Vector3 worldNear( worldNear4.x / worldNear4.w, worldNear4.y / worldNear4.w, worldNear4.z / worldNear4.w );
			Vector3 worldFar( worldFar4.x / worldFar4.w, worldFar4.y / worldFar4.w, worldFar4.z / worldFar4.w );

			Vector3 cameraForward;
			cameraForward.X = worldFar.X - worldNear.X;
			cameraForward.Y = worldFar.Y - worldNear.Y;
			cameraForward.Z = worldFar.Z - worldNear.Z;

			float len = sqrtf( cameraForward.X * cameraForward.X + cameraForward.Y * cameraForward.Y + cameraForward.Z * cameraForward.Z );
			if ( len > 0.0001f )
			{
				cameraForward.X /= len;
				cameraForward.Y /= len;
				cameraForward.Z /= len;
			}
			else goto magnet_end;

			Vector3 targetHead = Transform::GetHeadPosition( ClosestEntity, N32 );
			Vector3 targetRoot = Transform::GetPosition( ClosestEntity, N32 );
			if ( targetHead == Vector3::Zero( ) || targetRoot == Vector3::Zero( ) )
				goto magnet_end;

			float distance = Vector3::Distance( cameraPos, targetHead );
			if ( distance < 0.3f || distance > 500.f )
				goto magnet_end;

			Vector3 desiredHeadPos = cameraPos + ( cameraForward * distance );
			Vector3 headOffset = targetHead - targetRoot;
			if ( headOffset == Vector3::Zero( ) )
				headOffset = Vector3( 0.f, 0.25f, 0.f );

			lockedRootPos = desiredHeadPos - headOffset;

			// Resolve transform chain for bone root (ternary 32/64)
			uintptr_t boneRoot = ReadPtr( ClosestEntity + Offsets::Player::m_HipNode );
			if ( boneRoot )
			{
				uintptr_t transformValue = ReadPtr( boneRoot + Offsets::GetPosWorld::transObj );
				if ( transformValue )
				{
					uintptr_t transformObj = ReadPtr( transformValue + Offsets::GetPosWorld::transObj );
					if ( transformObj )
					{
						lockedMatrixAddr = ReadPtr( transformObj + Offsets::GetPosWorld::matrix );
					}
				}
			}

			// Write position offset: 0x80 for 32bit, 0xB0 for 64bit
			uintptr_t posWriteOffset = N32 ? 0x80 : 0xB0;

			if ( lockedMatrixAddr )
			{
				magnetThread = std::thread( [ posWriteOffset ] ( )
				{
					HANDLE hThread = GetCurrentThread( );
					SetThreadAffinityMask( hThread, 1 << 0 );
					SetThreadPriority( hThread, THREAD_PRIORITY_TIME_CRITICAL );

					while ( isHolding && lockedMatrixAddr )
					{
						g_FreeFireMemory.Write<Vector3>( lockedMatrixAddr + posWriteOffset, lockedRootPos );
						std::this_thread::sleep_for( std::chrono::microseconds( 1 ) );
					}
				} );
				magnetThread.detach( );
			}
		}

		if ( !keyPressed && isHolding )
		{
			isHolding = false;
			lockedMatrixAddr = 0;
			lockedRootPos = { 0.f, 0.f, 0.f };
		}

	magnet_end:
		;
	}

	// ==================== BoneSwap Aimbot ====================

	if ( AimCfg.Enabled && AimCfg.aimtype == 0 )
	{
		const bool keyDown = ( GetAsyncKeyState( AimCfg.KeyBind ) & 0x8000 );
		const bool cursorVisible = IsCursorVisibleNow( );

		if ( !keyDown )
		{
			if ( BS_Applied ) BS_Restore( );
			BS_ActiveByCursor = false;
		}
		else
		{
			if ( !BS_ActiveByCursor )
			{
				if ( cursorVisible == 0 )
				{
					BS_ActiveByCursor = true;
				}
			}

			if ( BS_ActiveByCursor )
			{
				if ( ClosestEntity == 0 )
				{
					if ( BS_Applied ) BS_Restore( );
				}
				else
				{
					if ( !BS_Applied || BS_LastTarget != ClosestEntity )
					{
						if ( BS_Applied ) BS_Restore( );
						if ( ClosestHP > 0 )
						{
							BS_Apply( ClosestEntity );
						}
						else if ( BS_Applied )
						{
							BS_Restore( );
						}
					}
					else
					{
						if ( ClosestHP <= 0 && BS_Applied )
						{
							BS_Restore( );
						}
					}
				}
			}
		}
	}
	else if ( BS_Applied )
	{
		BS_Restore( );
		BS_ActiveByCursor = false;
	}

	// ==================== Silent Aim Target ====================

	if ( ClosestEntity != 0 )
		Silent::SetTarget( localPlayer, ClosestEntity );
	else
		Silent::ClearTarget( );

	// ==================== Rage Aimbot ====================

	if ( ClosestEntity != 0 )
	{
		if ( CurrentMatrix.m [ 0 ][ 0 ] == 0.f )
		{
			CurrentMatrix = ViewMatrix;
		}

		if ( AimCfg.Enabled && AimCfg.aimtype == 1 && ( !g_Globals.AimBot.VisibleCheck || ( g_Globals.AimBot.VisibleCheck && enemiesvisible ) ) )
		{
			static bool s_AimFloodRunning = false;

			bool isShooting = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 );
			bool keyCurrentlyPressed = ( GetAsyncKeyState( AimCfg.KeyBind ) & 0x8000 );

			if ( isShooting && keyCurrentlyPressed && !IsCursorVisibleNow( ) )
			{
				RageTarget = ClosestEntity;

				if ( !s_AimFloodRunning )
				{
					s_AimFloodRunning = true;
					std::thread( [ localPlayer, MainCamera, N32 ] ( )
					{
						auto ReadPtrT = [ N32 ] ( uintptr_t addr ) -> uintptr_t
						{
							return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
						};

						int delayMs = 0;
						switch ( g_Globals.AimBot.PeitosIndex )
						{
							case 1: delayMs = 300; break;
							case 2: delayMs = 400; break;
							case 3: delayMs = 450; break;
							case 4: delayMs = 550; break;
							case 5: delayMs = ( rand( ) % 450 );
								break;
							default: delayMs = 0; break;
						}

						int originalAimAssist = 0;
						bool aimAssistModified = false;

						if ( delayMs > 0 )
							std::this_thread::sleep_for( std::chrono::milliseconds( delayMs ) );

						if ( delayMs > 0 )
						{
							originalAimAssist = g_FreeFireMemory.Read<int>( localPlayer + Offsets::Player::m_EAimAssit );
							g_FreeFireMemory.Write<int>( localPlayer + Offsets::Player::m_EAimAssit, 2 );
							aimAssistModified = true;
						}

						if ( !( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) || !( GetAsyncKeyState( g_Globals.AimBot.KeyBind ) & 0x8000 ) || IsCursorVisibleNow( ) )
						{
							if ( aimAssistModified )
								g_FreeFireMemory.Write<int>( localPlayer + Offsets::Player::m_EAimAssit, originalAimAssist );
							s_AimFloodRunning = false;
							return;
						}

						// Main aim loop
						while ( !g_Globals.General.ShutDown )
						{
							if ( IsCursorVisibleNow( ) )
								break;

							bool isStillShooting = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 );
							bool isKeyStillPressed = ( GetAsyncKeyState( g_Globals.AimBot.KeyBind ) & 0x8000 );
							if ( !( isStillShooting && isKeyStillPressed ) )
								break;

							if ( g_Globals.AimBot.IgnoreKnocked )
							{
								int hp = 1;
								uintptr_t fixedTarget = RageTarget;

								uintptr_t priPool = ReadPtrT( fixedTarget + Offsets::ReplicationEntity::m_PRIDataPool );
								if ( priPool != 0 )
								{
									uintptr_t datas = ReadPtrT( priPool + Offsets::ReplicationEntity::m_Datas );
									uintptr_t health = ReadPtrT( datas + Offsets::ReplicationEntity::HealthCurrentPtr );
									if ( health )
										hp = g_FreeFireMemory.Read<int>( health + Offsets::ReplicationEntity::Value );
								}

								bool isKnocked = false;
								uintptr_t shadowBase = ReadPtrT( fixedTarget + Offsets::PlayerNetwork::m_ShadowState );
								if ( shadowBase != 0 )
								{
									int playerPose = g_FreeFireMemory.Read<int>( shadowBase + Offsets::ShadowState::TargetPhysXPose );
									isKnocked = ( playerPose == 8 );
								}

								if ( hp <= 0 || isKnocked )
								{
									// PraCima
									if ( g_Globals.AimBot.PraCima && g_Globals.AimBot.PraCimaValor > 0.f && g_Globals.AimBot.PraCimaTempo > 0 )
									{
										DirectX::XMFLOAT4 qOriginal = g_FreeFireMemory.Read<DirectX::XMFLOAT4>( localPlayer + Offsets::Player::m_AimRotation );
										DirectX::XMVECTOR qCurrent = DirectX::XMLoadFloat4( &qOriginal );

										float totalPitchUp = -g_Globals.AimBot.PraCimaValor;
										int totalTimeMs = g_Globals.AimBot.PraCimaTempo;
										float pitchPerMs = totalPitchUp / static_cast< float >( totalTimeMs );

										for ( int elapsed = 0; elapsed < totalTimeMs; ++elapsed )
										{
											DirectX::XMVECTOR qDelta = DirectX::XMQuaternionRotationRollPitchYaw( pitchPerMs, 0.f, 0.f );
											qCurrent = DirectX::XMQuaternionMultiply( qDelta, qCurrent );
											qCurrent = DirectX::XMQuaternionNormalize( qCurrent );

											DirectX::XMFLOAT4 qOut;
											DirectX::XMStoreFloat4( &qOut, qCurrent );
											g_FreeFireMemory.Write( localPlayer + Offsets::Player::m_AimRotation, qOut );
											g_FreeFireMemory.Write( localPlayer + Offsets::Player::m_AuxAimRotation, qOut );

											std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
										}

										qCurrent = DirectX::XMQuaternionNormalize( qCurrent );
										DirectX::XMFLOAT4 qOutFinal;
										DirectX::XMStoreFloat4( &qOutFinal, qCurrent );
										g_FreeFireMemory.Write( localPlayer + Offsets::Player::m_AimRotation, qOutFinal );
										g_FreeFireMemory.Write( localPlayer + Offsets::Player::m_AuxAimRotation, qOutFinal );
									}

									Sleep( 300 );
									break;
								}
							}

							Vector3 Head = Transform::GetHeadPosition( RageTarget, N32 );
							Vector3 LocalCamera = ( MainCamera != 0 ) ? Transform::get_position_Injected( MainCamera, N32 ) : Vector3::Zero( );
							if ( Head == Vector3::Zero( ) || LocalCamera == Vector3::Zero( ) )
								break;

							auto playerLook = AimBot::GetRotationToLocation( Head, 0.0f, LocalCamera );
							g_FreeFireMemory.Write( localPlayer + Offsets::Player::m_AimRotation, playerLook );
							std::this_thread::sleep_for( std::chrono::microseconds( 1 ) );
						}

						if ( aimAssistModified )
							g_FreeFireMemory.Write<int>( localPlayer + Offsets::Player::m_EAimAssit, originalAimAssist );

						s_AimFloodRunning = false;

					} ).detach( );
				}
			}
		}
	}

	// ==================== Weapon Exploits ====================

	uintptr_t m_InventoryManager = ReadPtr( localPlayer + Offsets::Player::m_InventoryManager );
	uintptr_t m_itemOnHand = ReadPtr( m_InventoryManager + Offsets::InventoryManager::m_itemOnHand );
	uintptr_t m_WeaponData = ReadPtr( m_itemOnHand + Offsets::Weapon::m_WeaponData );
	uintptr_t WeaponParams = m_itemOnHand + Offsets::Weapon::m_WeaponParams;

	// --- NoRecoil ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.NoRecoil )
	{
		uintptr_t FireComponent = ReadPtr( m_itemOnHand + Offsets::Weapon::FireComponent );
		if ( FireComponent != 0 )
		{
			float currentRecoil = 0.0f;
			if ( g_FreeFireMemory.Read<float>( FireComponent + Offsets::Weapon::tangentTheta, currentRecoil ) )
			{
				constexpr float baseRecoil = 0.0174825f;
				float control = static_cast< float >( g_Globals.Misc.Exploits.LocalPlayer.RecoilControl );
				float newRecoil = baseRecoil * ( 1.0f - ( control / 100.0f ) );
				if ( fabs( currentRecoil - newRecoil ) > 0.00001f )
					g_FreeFireMemory.Write<float>( FireComponent + Offsets::Weapon::tangentTheta, newRecoil );
			}
		}
	}

	// --- SpinBot ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.SpinBot && !IsObserving )
	{
		SpinBot( localPlayer, N32 );
	}

	// --- SocoLonge ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.SocoLonge && !IsObserving )
	{
		float socolonge = g_FreeFireMemory.Read<float>( WeaponParams + Offsets::WeaponParams::Range );
		if ( socolonge == 1.35f )
		{
			g_FreeFireMemory.Write<float>( WeaponParams + Offsets::WeaponParams::Range, 3.1f );
		}
	}

	static float s_FastmeditOriginal = 0.0f;
	static bool s_FastmeditHasOriginal = false;
	static bool s_FastmeditRestored = true;

	if (g_Globals.Misc.Exploits.LocalPlayer.FastMedkit)
	{
		if (PlayerAttributes != 0)
		{
			uintptr_t eatSpeedAddr = PlayerAttributes + Offsets::PlayerAttributes::m_EatSpeedScale;
			if (eatSpeedAddr != 0)
			{
				if (!s_FastmeditHasOriginal)
				{
					s_FastmeditOriginal = g_FreeFireMemory.Read<float>(eatSpeedAddr);
					s_FastmeditHasOriginal = true;
					s_FastmeditRestored = false;
				}

				float valueToWrite = s_FastmeditOriginal;
				if (s_FastmeditOriginal == 1.0f)
				{
					valueToWrite = 0.75f;
				}
				else if (s_FastmeditOriginal == 0.75f)
				{
					valueToWrite = 0.50f;
				}
				g_FreeFireMemory.Write<float>(eatSpeedAddr, valueToWrite);
			}
		}
	}
	else
	{
		if (s_FastmeditHasOriginal && !s_FastmeditRestored && PlayerAttributes != 0)
		{
			uintptr_t eatSpeedAddr = PlayerAttributes + Offsets::PlayerAttributes::m_EatSpeedScale;
			if (eatSpeedAddr != 0)
			{
				g_FreeFireMemory.Write<float>(eatSpeedAddr, s_FastmeditOriginal);
				s_FastmeditRestored = true;
			}
		}
	}

	// --- MoreDamage ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.MoreDamage )
	{
		g_FreeFireMemory.Write<float>( WeaponParams + Offsets::WeaponParams::FullDamageDistance, 400.0f );
	}

	// --- FireDelay ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.FireDelay )
	{
		g_FreeFireMemory.Write<float>( WeaponParams + Offsets::WeaponParams::PrefireDelay, 0.0f );
	}

	// --- Aimlock ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.Aimlock )
	{
		g_FreeFireMemory.Write<float>( m_itemOnHand + Offsets::Weapon::m_FireDuration, -3.0f );
	}

	// --- AimLock2x ---
	if ( g_Globals.Misc.Exploits.LocalPlayer.AimLock2x && AimCfg.aimtype == 0 )
	{
		bool isSighting = g_FreeFireMemory.Read<bool>( m_itemOnHand + Offsets::Weapon::m_IsSighting );
		if ( isSighting )
		{
			uintptr_t aimassist = ReadPtr( localPlayer + Offsets::Player::m_AimAssistOnSighting );
			g_FreeFireMemory.Write<float>( aimassist + Offsets::AimAssistOnSighting::m_fAimAssistCurrentLerpTime, 0.0f );
		}
	}

	// --- AimbotAwm ---
	static std::vector<std::pair<uintptr_t, int>> s_awmOriginals;
	int WeaponType = g_FreeFireMemory.Read<int>( m_WeaponData + Offsets::Weapon::IntWeaponType );
	if ( g_Globals.Misc.Exploits.LocalPlayer.AimbotAwm )
	{
		if ( WeaponType == 1 )
		{
			bool exists = false;
			for ( const auto& pair : s_awmOriginals )
			{
				if ( pair.first == m_WeaponData )
				{
					exists = true;
					break;
				}
			}
			if ( !exists )
			{
				s_awmOriginals.emplace_back( m_WeaponData, WeaponType );
			}

			int currentCheck = g_FreeFireMemory.Read<int>( m_WeaponData + Offsets::Weapon::IntWeaponType );
			if ( currentCheck != 0 )
			{
				g_FreeFireMemory.Write<int>( m_WeaponData + Offsets::Weapon::IntWeaponType, 0 );
			}
		}
	}
	else
	{
		if ( !s_awmOriginals.empty( ) )
		{
			for ( const auto& [weaponAddr, original] : s_awmOriginals )
			{
				if ( weaponAddr != 0 )
				{
					g_FreeFireMemory.Write<int>( weaponAddr + Offsets::Weapon::IntWeaponType, original );
				}
			}
			s_awmOriginals.clear( );
		}
	}
}

// ==================== Helper Functions ====================

void Data::DrawBox( float x, float y, float w, float h, ImColor color, ImColor fillColor, float thickness, int Type )
{
	ImDrawList* DrawList = ImGui::GetForegroundDrawList( );

	if ( Type == 1 )
	{
		if ( g_Globals.Visuals.ESP.BoxFilled )
		{
			DrawList->AddRectFilled( ImVec2( x, y ), ImVec2( x + w, y + h ), ImGui::GetColorU32( ImVec4( fillColor.Value.x, fillColor.Value.y, fillColor.Value.z, fillColor.Value.w * 0.3f ) ) );
		}
		DrawList->AddRect( ImVec2( x, y ), ImVec2( x + w, y + h ), color, 0.0f, thickness );
		return;
	}
	else if ( Type == 2 )
	{
		float lineW = w / 3.0f;
		float lineH = h / 3.0f;

		if ( g_Globals.Visuals.ESP.BoxFilled )
		{
			DrawList->AddRectFilled( ImVec2( x, y ), ImVec2( x + w, y + h ), ImGui::GetColorU32( ImVec4( fillColor.Value.x, fillColor.Value.y, fillColor.Value.z, fillColor.Value.w * 0.3f ) ) );
		}

		DrawList->AddLine( ImVec2( x, y - thickness / 2 ), ImVec2( x, y + lineH ), color, thickness );
		DrawList->AddLine( ImVec2( x - thickness / 2, y ), ImVec2( x + lineW, y ), color, thickness );
		DrawList->AddLine( ImVec2( x + w - lineW, y ), ImVec2( x + w + thickness / 2, y ), color, thickness );
		DrawList->AddLine( ImVec2( x + w, y - thickness / 2 ), ImVec2( x + w, y + lineH ), color, thickness );
		DrawList->AddLine( ImVec2( x, y + h - lineH ), ImVec2( x, y + h + thickness / 2 ), color, thickness );
		DrawList->AddLine( ImVec2( x - thickness / 2, y + h ), ImVec2( x + lineW, y + h ), color, thickness );
		DrawList->AddLine( ImVec2( x + w - lineW, y + h ), ImVec2( x + w + thickness / 2, y + h ), color, thickness );
		DrawList->AddLine( ImVec2( x + w, y + h - lineH ), ImVec2( x + w, y + h + thickness / 2 ), color, thickness );
		return;
	}
	else if ( Type == 3 )
	{
		if ( g_Globals.Visuals.ESP.BoxFilled )
		{
			DrawList->AddRectFilled( ImVec2( x, y ), ImVec2( x + w, y + h ), ImGui::GetColorU32( ImVec4( fillColor.Value.x, fillColor.Value.y, fillColor.Value.z, fillColor.Value.w ) ) );
		}
		DrawList->AddRect( ImVec2( x, y ), ImVec2( x + w, y + h ), color, 0.0f, thickness );
		return;
	}
	else
	{
		if ( g_Globals.Visuals.ESP.BoxFilled )
		{
			DrawList->AddRectFilled( ImVec2( x, y ), ImVec2( x + w, y + h ), ImGui::GetColorU32( ImVec4( fillColor.Value.x, fillColor.Value.y, fillColor.Value.z, fillColor.Value.w * 0.3f ) ) );
		}
		DrawList->AddRect( ImVec2( x, y ), ImVec2( x + w, y + h ), color, 0.0f, thickness );
		return;
	}
}

void Data::DrawWeapon( int WeaponID, bool IsKnocked, Vector3 HeadPos, float Height )
{
	const int Style = g_Globals.Visuals.ESP.WeaponStyle;
	if ( Style == 0 ) return;

	static bool namegun_inited = false;
	if ( !namegun_inited )
	{
		Namegun::Init( ); namegun_inited = true;
	}

	ImDrawList* DrawList = ImGui::GetForegroundDrawList( );

	const float feetY = HeadPos.Y + Height;
	const float centerX = HeadPos.X;

	ImVec2 iconSz( 0, 0 );
	ImVec2 textSz( 0, 0 );
	std::string icon, name;

	ImColor Color = IsKnocked ? ImColor( 1.f, 0.f, 0.f, 1.f )
		: ImColor( g_Globals.Visuals.ESP.WeaponColor [ 0 ], g_Globals.Visuals.ESP.WeaponColor [ 1 ],
			g_Globals.Visuals.ESP.WeaponColor [ 2 ], g_Globals.Visuals.ESP.WeaponColor [ 3 ] );

	if ( Namegun::HasIcon( WeaponID ) ) icon = Namegun::GetGunIcon( WeaponID );
	name = Namegun::GetGunName( WeaponID );

	if ( Style == 2 || Style == 3 )
	{
		if ( !icon.empty( ) )
		{
			ImGui::PushFont( Fonts::IconWeapon );
			iconSz = ImGui::CalcTextSize( icon.c_str( ) );
			ImGui::PopFont( );
		}
	}

	if ( Style == 1 || Style == 3 )
	{
		if ( !name.empty( ) )
		{
			ImGui::PushFont( Fonts::InterRegular );
			textSz = ImGui::CalcTextSize( name.c_str( ) );
			ImGui::PopFont( );
		}
	}

	if ( Style == 2 || Style == 3 )
	{
		if ( !icon.empty( ) )
		{
			ImGui::PushFont( Fonts::IconWeapon );
			DrawList->AddText( Fonts::IconWeapon, g_Globals.Visuals.ESP.TextSize,
				ImVec2( centerX - iconSz.x * 0.5f, feetY + 33.0f ), Color, icon.c_str( ) );
			ImGui::PopFont( );
		}
	}

	if ( Style == 1 || Style == 3 )
	{
		if ( !name.empty( ) )
		{
			ImGui::PushFont( Fonts::Verdana );
			float yName = ( Style == 1 ) ? ( feetY + 20.0f ) : ( feetY + 33.0f );
			DrawList->AddText( Fonts::Verdana, g_Globals.Visuals.ESP.TextSize,
				ImVec2( centerX - textSz.x * 0.5f, yName ), Color, name.c_str( ) );
			ImGui::PopFont( );
		}
	}
}

void Data::DrawHealthBar( short CurrentHealth, short MaxHealth, ImVec2 HeadPos, ImVec2 EntityPos, float Width, float Height, int Entity )
{
	struct HealthCacheEntry
	{
		int Entity;
		float SmoothedHealth;
	};

	if ( MaxHealth <= 0 ) return;

	int Style = g_Globals.Visuals.ESP.HealthBarStyle;
	if ( Style == 0 ) return;

	static std::vector<HealthCacheEntry> HealthCache;
	static const int MAX_HEALTH_ENTITIES = 512;

	ImDrawList* DrawList = ImGui::GetForegroundDrawList( );
	ImGuiIO& io = ImGui::GetIO( );

	float HealthPercentage = static_cast< float >( CurrentHealth ) / MaxHealth;
	float* SmoothedHealthPtr = nullptr;

	for ( auto& entry : HealthCache )
	{
		if ( entry.Entity == Entity )
		{
			SmoothedHealthPtr = &entry.SmoothedHealth;
			break;
		}
	}

	if ( !SmoothedHealthPtr )
	{
		if ( HealthCache.size( ) < MAX_HEALTH_ENTITIES )
		{
			HealthCache.push_back( { Entity, HealthPercentage } );
			SmoothedHealthPtr = &HealthCache.back( ).SmoothedHealth;
		}
		else
		{
			int index = Entity % MAX_HEALTH_ENTITIES;
			HealthCache [ index ] = { Entity, HealthPercentage };
			SmoothedHealthPtr = &HealthCache [ index ].SmoothedHealth;
		}
	}

	float& SmoothedHealth = *SmoothedHealthPtr;
	SmoothedHealth = ImLerp( SmoothedHealth, HealthPercentage, io.DeltaTime * 10.0f );

	ImVec4 GreenColor = ImVec4( 0.0f, 1.0f, 0.0f, 1.0f );
	ImVec4 YellowColor = ImVec4( 1.0f, 1.0f, 0.0f, 1.0f );
	ImVec4 RedColor = ImVec4( 1.0f, 0.0f, 0.0f, 1.0f );

	ImVec4 HealthBarColor;
	if ( SmoothedHealth > 0.5f )
		HealthBarColor = ImLerp( GreenColor, YellowColor, ( 1.0f - SmoothedHealth ) * 2.0f );
	else
		HealthBarColor = ImLerp( YellowColor, RedColor, ( 0.5f - SmoothedHealth ) * 2.0f );

	if ( Style == 1 )
	{
		ImVec2 Position( HeadPos.x - ( Width * 0.5f ) - 5.0f, HeadPos.y );
		float BarWidth = 2.5f;
		float FilledBarHeight = Height * SmoothedHealth;

		DrawList->AddRectFilled( ImVec2( Position.x, Position.y ), ImVec2( Position.x + BarWidth, Position.y + Height ), IM_COL32( 0, 0, 0, 128 ) );
		DrawList->AddRectFilled( ImVec2( Position.x, Position.y + ( Height - FilledBarHeight ) ), ImVec2( Position.x + BarWidth, Position.y + Height ), ImGui::ColorConvertFloat4ToU32( HealthBarColor ) );
	}
	else if ( Style == 2 )
	{
		ImVec2 Position( HeadPos.x + ( Width * 0.5f ) + 3.0f, HeadPos.y );
		float BarWidth = 2.5f;
		float FilledBarHeight = Height * SmoothedHealth;

		DrawList->AddRectFilled( ImVec2( Position.x, Position.y ), ImVec2( Position.x + BarWidth, Position.y + Height ), IM_COL32( 0, 0, 0, 128 ) );
		DrawList->AddRectFilled( ImVec2( Position.x, Position.y + ( Height - FilledBarHeight ) ), ImVec2( Position.x + BarWidth, Position.y + Height ), ImGui::ColorConvertFloat4ToU32( HealthBarColor ) );
	}
	else if ( Style == 3 )
	{
		const bool showName = g_Globals.Visuals.ESP.ShowName;
		float dynamicTopOffset = showName ? ( kNameOffset + kStackGap + kHealthBarHeight )
			: kHealthOffsetBase;

		ImVec2 Position( HeadPos.x - Width * 0.5f, HeadPos.y - dynamicTopOffset );
		float BarHeight = kHealthBarHeight;
		float FilledWidth = Width * SmoothedHealth;

		DrawList->AddRectFilled( Position, ImVec2( Position.x + Width, Position.y + BarHeight ), IM_COL32( 0, 0, 0, 128 ) );
		DrawList->AddRectFilled( Position, ImVec2( Position.x + FilledWidth, Position.y + BarHeight ), ImGui::ColorConvertFloat4ToU32( HealthBarColor ) );
	}
	else if ( Style == 4 )
	{
		ImVec2 Position( HeadPos.x - ( Width * 0.5f ), EntityPos.y + 5.0f );
		float BarHeight = 2.5f;
		float FilledWidth = Width * SmoothedHealth;

		DrawList->AddRectFilled( Position, ImVec2( Position.x + Width, Position.y + BarHeight ), IM_COL32( 0, 0, 0, 128 ) );
		DrawList->AddRectFilled( Position, ImVec2( Position.x + FilledWidth, Position.y + BarHeight ), ImGui::ColorConvertFloat4ToU32( HealthBarColor ) );
	}
	else if ( Style == 5 )
	{
		ImGui::PushFont( Fonts::Gff );
		char healthText [ 16 ];
		snprintf( healthText, sizeof( healthText ), "HP: %d", ( int )CurrentHealth );
		ImVec2 TextSize = Utils::CalcTextSize( Fonts::Gff, 13.0f, healthText );
		ImVec2 barPos( HeadPos.x - Width * 0.5f, HeadPos.y - 3.0f );
		ImVec2 textPos( HeadPos.x - ( TextSize.x * 0.5f ), barPos.y - TextSize.y - 2.0f );
		DrawList->AddText( Fonts::Gff, 13.0f, textPos, ImColor( 255, 255, 255, 255 ), healthText );
		ImGui::PopFont( );
	}
}

void Data::DrawSnapLine( const Vector3& HeadPos, const Vector3& EntityPos, bool showName, bool healthTop, ImColor color, float thickness, int type )
{
	if ( type == 0 ) return;

	float topOffset = healthTop ? ( showName ? ( kNameOffset + kStackGap + kHealthBarHeight )
		: kHealthOffsetBase )
		: ( showName ? kNameOffset : 1.0f );

	float targetX = HeadPos.X;
	float targetY = HeadPos.Y;

	switch ( type )
	{
		case 1: targetY = HeadPos.Y - topOffset; break;
		case 2: targetY = EntityPos.Y + 1.0f; break;
		default: targetY = HeadPos.Y; break;
	}

	ImDrawList* DrawList = ImGui::GetForegroundDrawList( );
	ImVec2 startPoint = ( type == 2 )
		? ImVec2( ScreenWidth * 0.5f, ( float )ScreenHeight )
		: ImVec2( ScreenWidth * 0.5f, ScreenHeight * 0.03f );

	ImVec2 endPoint( targetX, ImClamp( targetY, 0.0f, ( float )ScreenHeight - 1.0f ) );
	DrawList->AddLine( startPoint, endPoint, color, thickness );
}

void Data::SpinBot( uintptr_t LocalPlayer, bool N32 )
{
	if ( LocalPlayer == 0 ) return;

	auto ReadPtr = [ N32 ] ( uintptr_t addr ) -> uintptr_t
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
	};

	uintptr_t transform = ReadPtr( LocalPlayer + Offsets::PlayerTransformNode::m_CachedTransform );
	if ( !transform ) return;

	uintptr_t transformObj = ReadPtr( transform + Offsets::GetPosWorld::transObj );
	if ( !transformObj ) return;

	uintptr_t matrix = ReadPtr( transformObj + Offsets::GetPosWorld::matrix );
	if ( !matrix ) return;

	int index = g_FreeFireMemory.Read<int>( transformObj + Offsets::GetPosWorld::index );

	uintptr_t matrixList = ReadPtr( matrix + Offsets::GetPosWorld::matrix_list );
	uintptr_t matrixIndices = ReadPtr( matrix + Offsets::GetPosWorld::matrix_indices );

	if ( !matrixList || !matrixIndices ) return;

	bool isShooting = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 );
	uintptr_t userControl = ReadPtr( LocalPlayer + Offsets::Player::m_UserControl );
	if ( userControl != 0 )
	{
		uintptr_t axisDataArray = ReadPtr( userControl + Offsets::UserControlHandler::m_AxisData );
		if ( axisDataArray != 0 )
		{
			uintptr_t moveAxisData = ReadPtr( axisDataArray + ( N32 ? 0x10 : 0x20 ) );
			if ( moveAxisData != 0 )
			{
				bool isTouched = g_FreeFireMemory.Read<bool>( moveAxisData + Offsets::UserControlHandler::m_IsTouched );
				if ( isTouched && !isShooting )
				{
					return;
				}
			}
		}
	}

	static float rotationAngle = 0.0f;
	rotationAngle += g_Globals.Misc.Exploits.LocalPlayer.SpinSpeed * 0.1f;

	if ( rotationAngle >= 3.14159265358979323846f * 2.0f )
		rotationAngle -= 3.14159265358979323846f * 2.0f;

	uintptr_t rotationWriteOffset = N32 ? 0x10 : 0x20;

	TMatrix rootMatrix = g_FreeFireMemory.Read<TMatrix>( matrixList + sizeof( TMatrix ) * index );
	Quaternion rootQuat( rootMatrix.Rotation.x, rootMatrix.Rotation.y, rootMatrix.Rotation.z, rootMatrix.Rotation.w );
	Vector3 rootEuler = Quaternion::ToEuler( rootQuat );
	Quaternion newRootQuat = Quaternion::FromEuler( rootEuler.X, rotationAngle, rootEuler.Z );

	newRootQuat = Quaternion::Normalized( newRootQuat );
	g_FreeFireMemory.Write<Vector4>( matrixList + sizeof( TMatrix ) * index + rotationWriteOffset, { newRootQuat.X, newRootQuat.Y, newRootQuat.Z, newRootQuat.W } );
	int parentIndex = g_FreeFireMemory.Read<int>( matrixIndices + sizeof( int ) * index );

	int curIndex = 0;
	while ( parentIndex >= 0 && curIndex++ < 60 )
	{
		TMatrix parentMatrix = g_FreeFireMemory.Read<TMatrix>( matrixList + sizeof( TMatrix ) * parentIndex );
		Quaternion parentQuat( parentMatrix.Rotation.x, parentMatrix.Rotation.y, parentMatrix.Rotation.z, parentMatrix.Rotation.w );
		Vector3 parentEuler = Quaternion::ToEuler( parentQuat );
		Quaternion newParentQuat = Quaternion::FromEuler( parentEuler.X, rotationAngle, parentEuler.Z );
		newParentQuat = Quaternion::Normalized( newParentQuat );
		g_FreeFireMemory.Write<Vector4>( matrixList + sizeof( TMatrix ) * parentIndex + rotationWriteOffset, { newParentQuat.X, newParentQuat.Y, newParentQuat.Z, newParentQuat.W } );
		parentIndex = g_FreeFireMemory.Read<int>( matrixIndices + sizeof( int ) * parentIndex );
	}
}