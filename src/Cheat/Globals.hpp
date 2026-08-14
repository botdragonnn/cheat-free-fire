#pragma once
#include <Windows.h> 
namespace Cheat {
	class Globals {
	public:
		struct AimBot
		{
			bool Enabled = false;
			int aimtype = 0;
			bool IgnoreKnocked = false;
			bool IgnoreBots = false;
			bool VisibleCheck = false;
			bool PraCima = false;
			int KeyBind = VK_LBUTTON;
			bool aimmagnect = false;
			int MagKey = VK_XBUTTON2;
			bool ghost = false;
			int ghostkey = VK_XBUTTON2;
			int TPKey = 0;
			bool TP = false;
			int CityIndex = 0;
			int Fov = 360;
			int MaxDistance = 200;
			int Sleep = 0;
			float PraCimaValor = 0.50F;
			int PraCimaTempo = 50;
			int PeitosIndex = 0;
			int Target = 0;
		} AimBot;
		struct Silent
		{
			bool Enabled = false;
			int KeyBind = VK_LBUTTON;
			int Fov = 30;
			int MaxDistance = 100;
		} Silent;
		struct Visuals
		{
			struct ESP
			{
				bool Enabled = false;
				bool ShowTeam = false;
				int RenderDistance = 240;
				float Thickness = 1.0f;
				float TextSize = 15.0f;
				bool Watermark = false;
				float WatermarkColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				bool Enemy = false;
				float EnemyColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
				float TeamColor[4] = { 0.2f, 0.6f, 1.0f, 1.0f };
				bool Weapon = false;
				bool ShowIcons = false;
				int WeaponStyle = 1;
				float WeaponColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool SnapLines = false;
				int SnapLinesPos = 1;
				float SnapLinesColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool HealthBar = false;
				int HealthBarStyle = 1;
				bool Box = false;
				bool BoxFilled = false;
				int BoxStyle = 1;
				float BoxColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float FilledBoxColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool ShowName = false;
				float NameColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool Distance = false;
				float DistanceColor[4] = { 1.f, 1.f, 1.f, 1.f };
				bool Skeleton = false;
				bool SkeletonFingers = true;
				int SkeletonStyle = 0;
				float SkeletonColor[4] = { 1.f, 1.f, 1.f, 1.f };
			} ESP;
			struct Chams
			{
				bool Enabled = false;
				bool AggressiveMode = false;
				float NearColor[4] = { 0.18f, 1.0f, 0.0f, 1.0f };
				float FarColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			} Chams;
		} Visuals;
		struct Misc
		{
			struct Screen
			{
				bool ShowAimbotFov = false;
				float AimbotFovColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float FilledFovColor[4] = { 0.f, 0.f, 0.f, 0.0f };
				bool ShowSilentFov = false;
				float SilentFovColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float SilentFilledFovColor[4] = { 0.f, 0.f, 0.f, 0.0f };
			} Screen;
			struct Exploits
			{
				struct LocalPlayer
				{
					bool AimLock2x = false;
					bool NoRecoil = false;
					bool Aimlock = false;
					bool FastMedkit = false;
					bool telaparada = false;
					bool AtributarArma = false;
					int AtributarArmaLevel = 0;
					bool fly = false;
					bool AimbotAwm = false;
					bool MoreDamage = false;
					bool SocoLonge = false;
					bool FireDelay = false;
					bool BugarPixel = false;
					bool Precision = false;
					bool BackJump = false;
					bool SpinBot = false;
					float SpinSpeed = 1.0f;
					int RecoilControl = 100;
				} LocalPlayer;
			} Exploits;
		} Misc;
		struct General
		{
			int MenuKey = VK_INSERT;
			int ThreadDelay = 240;
			bool CaptureBypass = true;
			bool WebRemote = false;
			bool ShutDown = false;
			bool NoAnogs = false;
			bool N32 = false;
			bool V31 = false;
			bool EnableFuncs = false;
			char Username[20];
			char Role[20];
			char PassWord[20];
			char License[255];
			char* Local;
		} General;
	};
}
inline Cheat::Globals g_Globals;