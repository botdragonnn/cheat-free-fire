#include "Offsets.hpp"
#include <Main/Memory/Memory.hpp>
#include <Cheat/Globals.hpp>
#include <Main/Unity/UTF/UTF8.hpp>

// ==================== Offsets ====================

uintptr_t Offsets::LibIl2Cpp = 0;
std::vector<uintptr_t> Offsets::LibIl2CppCandidates;
uintptr_t Offsets::AccessClass = 0;

bool Offsets::IsMatchActive(Offsets::MatchState s)
{
	int v = static_cast<int>(s);
	return v >= 1 && v <= 3;
}

// ==================== Game Flow ====================

// GameConfig
//uintptr_t Offsets::GameConfig::ReleaseVersion = 0;

// GameVarDef
uintptr_t Offsets::GameVarDef::GameVarDef_TypeInfo = 0;
uintptr_t Offsets::GameVarDef::ShootTraceAdjustmentDistanceThreshold = 0;
uintptr_t Offsets::GameVarDef::EnableAccelerationOnFalling = 0;
uintptr_t Offsets::GameVarDef::EnableLowFallingSwapWeapon = 0;
uintptr_t Offsets::GameVarDef::RotationSensitivityMin = 0;
uintptr_t Offsets::GameVarDef::RotationSensitivityMax = 0;
uintptr_t Offsets::GameVarDef::AimRotationSensitivityMin = 0;
uintptr_t Offsets::GameVarDef::AimRotationSensitivityMax = 0;

// GameFacade
uintptr_t Offsets::GameFacade::GameFacade_TypeInfo = 0;
uintptr_t Offsets::GameFacade::CurrentMatchGame = 0;

// MatchGame
uintptr_t Offsets::MatchGame::m_Match = 0;
uintptr_t Offsets::MatchGame::m_CameraControllerManager = 0;

// Match
uintptr_t Offsets::Match::m_State = 0;
uintptr_t Offsets::Match::m_LocalPlayer = 0;
uintptr_t Offsets::Match::m_LocalObserver = 0;
uintptr_t Offsets::Match::m_AttackableEntities = 0;

// FollowCamera

// ==================== Camera ====================

uintptr_t Offsets::CameraControllerManager::m_Camera = 0;
uintptr_t Offsets::Camera::m_CachedPtr = 0;
uintptr_t Offsets::Camera::ViewMatrix = 0;

// ==================== Observer  ====================

uintptr_t Offsets::Observer::m_TargetPlayer = 0;

// ==================== Player ====================

// General
uintptr_t Offsets::Player::IsClientBot = 0;
uintptr_t Offsets::Player::IsFemale = 0;
uintptr_t Offsets::Player::IsPrepareAttack = 0;
uintptr_t Offsets::Player::m_IsCurFrameFowardLockToAimRot = 0;
uintptr_t Offsets::Player::m_WaitForForceSync = 0;
uintptr_t Offsets::Player::m_TransformType = 0;

// Aim
uintptr_t Offsets::Player::m_AimRotation = 0;
uintptr_t Offsets::Player::m_AuxAimRotation = 0;
uintptr_t Offsets::Player::m_AimAssist = 0;
uintptr_t Offsets::Player::m_EAimAssit = 0;
uintptr_t Offsets::Player::m_AimAssistOnSighting = 0;
uintptr_t Offsets::Player::m_LastAimingInfoFromWeapon = 0;

// Transform / Camera
uintptr_t Offsets::Player::MainCameraTransform = 0;
uintptr_t Offsets::Player::m_SwapWeaponTime = 0;
uintptr_t Offsets::Player::m_FollowCamera = 0;

// Managers
uintptr_t Offsets::Player::m_Attributes = 0;
uintptr_t Offsets::Player::m_AvatarManager = 0;
uintptr_t Offsets::Player::m_InventoryManager = 0;

// UserControlHandler
uintptr_t Offsets::Player::m_UserControl = 0;

// Colliders
uintptr_t Offsets::Player::m_HeadCollider = 0;
uintptr_t Offsets::Player::m_fireColliders = 0;
//uintptr_t Offsets::Player::LockedAimingCollider = 0;

// Bone Nodes (ITransformNode)
uintptr_t Offsets::Player::HeadNode = 0;
uintptr_t Offsets::Player::m_HipNode = 0;
uintptr_t Offsets::Player::m_BloodEffectNode = 0;
uintptr_t Offsets::Player::m_RootNode = 0;
uintptr_t Offsets::Player::m_BoneRootNode = 0;
uintptr_t Offsets::Player::m_WeaponMountNode = 0;
uintptr_t Offsets::Player::m_LeftWeaponNode = 0;
uintptr_t Offsets::Player::m_FlightNode = 0;
uintptr_t Offsets::Player::m_RightArmNode = 0;
uintptr_t Offsets::Player::m_LeftArmNode = 0;
uintptr_t Offsets::Player::m_RightForeArmNode = 0;
uintptr_t Offsets::Player::m_LeftForeArmNode = 0;
uintptr_t Offsets::Player::m_RightHandNode = 0;
uintptr_t Offsets::Player::m_LeftHandNode = 0;
uintptr_t Offsets::Player::m_RightAnkleNode = 0;
uintptr_t Offsets::Player::m_LeftAnkleNode = 0;
uintptr_t Offsets::Player::m_RightToeNode = 0;
uintptr_t Offsets::Player::m_LeftToeNode = 0;

// ==================== PlayerNetwork ====================

uintptr_t Offsets::PlayerNetwork::m_ShadowState = 0;
uintptr_t Offsets::PlayerNetwork::m_Profile = 0;

// ==================== PlayerAttributes ====================

uintptr_t Offsets::PlayerAttributes::m_EatSpeedScale = 0;
uintptr_t Offsets::PlayerAttributes::m_FireIntervalScale = 0;
//uintptr_t Offsets::PlayerAttributes::ShootNoReload = 0;
//uintptr_t Offsets::PlayerAttributes::DamageAdditionScale = 0;
//uintptr_t Offsets::PlayerAttributes::ExecuteDamageScale = 0;
//uintptr_t Offsets::PlayerAttributes::BuffWeaponDamageScale = 0;
//uintptr_t Offsets::PlayerAttributes::ShowEnermyTargetOnMap = 0;
//uintptr_t Offsets::PlayerAttributes::ShowEnermyTargetOnHud = 0;
//uintptr_t Offsets::PlayerAttributes::ShowEnemyFootStep = 0;
//uintptr_t Offsets::PlayerAttributes::EnemyFootStepMaxDistanceDelta = 0;
//uintptr_t Offsets::PlayerAttributes::EnemyFootStepMinDistanceDelta = 0;
//uintptr_t Offsets::PlayerAttributes::EnemyFootStepLimitNumDelta = 0;

// ==================== AimAssistAutoLock ====================

uintptr_t Offsets::AimAssistAutoLock::m_TargetHeuristic = 0;
uintptr_t Offsets::AimAssistAutoLock::m_Entity = 0;

// ==================== UserControlHandler ====================

uintptr_t Offsets::UserControlHandler::m_AxisData = 0;
uintptr_t Offsets::UserControlHandler::m_FingerInDashArea = 0;
uintptr_t Offsets::UserControlHandler::m_IsTouched = 0;
uintptr_t Offsets::UserControlHandler::m_LockFingerInDashArea = 0;
uintptr_t Offsets::UserControlHandler::m_DashByMovingJoystick = 0;

// ==================== AimAssistOnSighting ====================

uintptr_t Offsets::AimAssistOnSighting::m_fAimAssistCurrentLerpTime = 0;

// ==================== HitObjectInfo ====================

uintptr_t Offsets::HitObjectInfo::RayDir = 0;
uintptr_t Offsets::HitObjectInfo::StartPosition = 0;

// ==================== InventoryManager ====================

uintptr_t Offsets::InventoryManager::m_itemOnHand = 0;

// ==================== Avatar ====================

uintptr_t Offsets::AvatarManager::m_Avatar = 0;
uintptr_t Offsets::UMAAvatarBase::umaData = 0;
uintptr_t Offsets::UmaAvatarSimple::IsVisible = 0;

// UMAData
uintptr_t Offsets::UMAData::skeleton = 0;
uintptr_t Offsets::UMAData::isLocalPlayer = 0;
uintptr_t Offsets::UMAData::isTeammate = 0;

// UMASkeleton
uintptr_t Offsets::UMASkeleton::boneHashDataLookup = 0;
uintptr_t Offsets::UMASkeleton::boneNameHash = 0;
uintptr_t Offsets::UMASkeleton::boneTransform = 0;

// ==================== Replication ====================

uintptr_t Offsets::ReplicationEntity::m_PRIDataPool = 0;
uintptr_t Offsets::ReplicationEntity::m_Datas = 0;
uintptr_t Offsets::ReplicationEntity::HealthCurrentPtr = 0;
uintptr_t Offsets::ReplicationEntity::HealthMaxPtr = 0;
uintptr_t Offsets::ReplicationEntity::WeaponPtr = 0;
uintptr_t Offsets::ReplicationEntity::EpPtr = 0;
uintptr_t Offsets::ReplicationEntity::Value = 0;

// ==================== Shadow ====================

uintptr_t Offsets::ShadowState::TargetPhysXPose = 0;

// ==================== Profile ====================

uintptr_t Offsets::BaseProfileInfo::AccountID = 0;
uintptr_t Offsets::BaseProfileInfo::Level = 0;
uintptr_t Offsets::BaseProfileInfo::NickName = 0;

// ==================== Weapon ====================

// Weapon
uintptr_t Offsets::Weapon::FireComponent = 0;
uintptr_t Offsets::Weapon::m_WeaponData = 0;
uintptr_t Offsets::Weapon::m_WeaponParams = 0;
uintptr_t Offsets::Weapon::m_FireDuration = 0;
uintptr_t Offsets::Weapon::m_IsSighting = 0;
uintptr_t Offsets::Weapon::tangentTheta = 0;
uintptr_t Offsets::Weapon::IntWeaponType = 0;

// WeaponParams
uintptr_t Offsets::WeaponParams::FullDamageDistance = 0;
uintptr_t Offsets::WeaponParams::PrefireDelay = 0;
uintptr_t Offsets::WeaponParams::Range = 0;

// ==================== Transform / Position ====================

uintptr_t Offsets::PlayerTransformNode::Transform = 0;
uintptr_t Offsets::PlayerTransformNode::m_CachedTransform = 0;

uintptr_t Offsets::GetPosWorld::transObj = 0;
uintptr_t Offsets::GetPosWorld::matrix = 0;
uintptr_t Offsets::GetPosWorld::index = 0;
uintptr_t Offsets::GetPosWorld::matrix_list = 0;
uintptr_t Offsets::GetPosWorld::matrix_indices = 0;

uintptr_t Offsets::GetPosWorld::HeadColliderMale = 0;
uintptr_t Offsets::GetPosWorld::HeadColliderFemale = 0;
uintptr_t Offsets::GetPosWorld::ColliderTransform = 0;
uintptr_t Offsets::GetPosWorld::BoundsCenter_1 = 0;
uintptr_t Offsets::GetPosWorld::BoundsCenter_2 = 0;
uintptr_t Offsets::GetPosWorld::BoundsCenter_3 = 0;

// ==================== IL2CPP Containers ====================

// UnityList
template <bool N32, typename TValue>
uintptr_t Offsets::UnityList<N32, TValue>::GetItems()
{
	uintptr_t ArrayBase = N32 ? g_FreeFireMemory.Read<uint32_t>((uint32_t)this + 0x8) + 0x10 : g_FreeFireMemory.Read<uint64_t>((uint64_t)this + 0x10) + 0x20;
	return ArrayBase;
}

template <bool N32, typename TValue>
int Offsets::UnityList<N32, TValue>::GetSize()
{
	int Size = N32 ? g_FreeFireMemory.Read<int>((uint32_t)this + 0xC) : g_FreeFireMemory.Read<int>((uint64_t)this + 0x18);
	return (Size >= 0 && Size <= 500) ? Size : 0;
}

template <bool N32, typename TValue>
TValue Offsets::UnityList<N32, TValue>::GetItem(int Index)
{
	return g_FreeFireMemory.Read<TValue>(GetItems() + (N32 ? 0x4 : 0x8) * Index);
}

// Explicit instantiations
template class Offsets::UnityList<true, uint32_t>;
template class Offsets::UnityList<true, uint64_t>;
template class Offsets::UnityList<false, uint32_t>;
template class Offsets::UnityList<false, uint64_t>;

template <bool N32, bool V31, typename TValue>
uintptr_t Offsets::UnityDictionary<N32, V31, TValue>::GetValues()
{
	if constexpr (V31)
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>((uint32_t)this + 0xC) + 0x10 : g_FreeFireMemory.Read<uint64_t>((uint64_t)this + 0x18) + 0x20;
	}
	else
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>((uint32_t)this + 0x14) + 0x10 : g_FreeFireMemory.Read<uint64_t>((uint64_t)this + 0x28) + 0x20;
	}
}

template <bool N32, bool V31, typename TValue>
int Offsets::UnityDictionary<N32, V31, TValue>::GetNumValues()
{
	int Count;
	if constexpr (V31)
	{
		Count = N32 ? g_FreeFireMemory.Read<int>((uint32_t)this + 0x10) : g_FreeFireMemory.Read<int>((uint64_t)this + 0x20);
	}
	else
	{
		Count = N32 ? g_FreeFireMemory.Read<int>((uint32_t)this + 0x18) : g_FreeFireMemory.Read<int>((uint64_t)this + 0x30);
	}
	return (Count >= 1 && Count <= 500) ? Count : 0;
}

template <bool N32, bool V31, typename TValue>
TValue Offsets::UnityDictionary<N32, V31, TValue>::GetValue(int Index)
{
	if constexpr (V31)
	{
		return g_FreeFireMemory.Read<TValue>(GetValues() + (N32 ? 0x10 * Index + 0xC : 0x18 * Index + 0x10));
	}
	else
	{
		return g_FreeFireMemory.Read<TValue>(GetValues() + (N32 ? 0x4 : 0x8) * Index);
	}
}

// Explicit instantiations
template class Offsets::UnityDictionary<true, false, uint32_t>;
template class Offsets::UnityDictionary<true, false, uint64_t>;
template class Offsets::UnityDictionary<false, false, uint32_t>;
template class Offsets::UnityDictionary<false, false, uint64_t>;
template class Offsets::UnityDictionary<true, true, uint32_t>;
template class Offsets::UnityDictionary<true, true, uint64_t>;
template class Offsets::UnityDictionary<false, true, uint32_t>;
template class Offsets::UnityDictionary<false, true, uint64_t>;

void Offsets::GameConfig()
{
	for (uintptr_t candidate : LibIl2CppCandidates)
	{
		LibIl2Cpp = candidate;
		printf("[GameConfig] Testando libil2cpp.so candidata: 0x%llX\n", (uint64_t)candidate);
		// ==================== FF TH v7a 75 32-bit, OB54) ====================
		{
			uint32_t pTypeInfo = g_FreeFireMemory.Read<uint32_t>(LibIl2Cpp + 0xABFF3B8);
			uint32_t StaticFields = g_FreeFireMemory.Read<uint32_t>(pTypeInfo + 0x5C);
			uint32_t ReleaseVersion = g_FreeFireMemory.Read<uint32_t>(StaticFields + 0x0);

			printf("[V7A] pTypeInfo    : 0x%X (from LibIl2Cpp+0xABFF3B8)\n", pTypeInfo);
			printf("[V7A] StaticFields : 0x%X (+0x5C)\n", StaticFields);
			printf("[V7A] ReleaseVersion: 0x%X\n", ReleaseVersion);

			if (ReleaseVersion)
			{
				std::string version = ObterStr(ReleaseVersion + 0xC, g_FreeFireMemory.Read<uint32_t>(ReleaseVersion + 0x8));
				printf("[V7A] Release Version: %s\n", version.c_str());

				if (version == "OB54")
				{
					g_Globals.General.N32 = true;
					g_Globals.General.V31 = true;
					g_Globals.General.NoAnogs = true;
					FFTHV7A75();
					return;
				}
			}
		}

		LibIl2Cpp = candidate;
		printf("[GameConfig] Testando libil2cpp.so candidata: 0x%llX\n", (uint64_t)candidate);
		// ==================== FF TH v7a 76 32-bit, OB54) ====================
		{
			uint32_t pTypeInfo = g_FreeFireMemory.Read<uint32_t>(LibIl2Cpp + 0xABFF6D8);
			uint32_t StaticFields = g_FreeFireMemory.Read<uint32_t>(pTypeInfo + 0x5C);
			uint32_t ReleaseVersion = g_FreeFireMemory.Read<uint32_t>(StaticFields + 0x0);

			printf("[V7A] pTypeInfo    : 0x%X (from LibIl2Cpp+0xABFF6D8)\n", pTypeInfo);
			printf("[V7A] StaticFields : 0x%X (+0x5C)\n", StaticFields);
			printf("[V7A] ReleaseVersion: 0x%X\n", ReleaseVersion);

			if (ReleaseVersion)
			{
				std::string version = ObterStr(ReleaseVersion + 0xC, g_FreeFireMemory.Read<uint32_t>(ReleaseVersion + 0x8));
				printf("[V7A] Release Version: %s\n", version.c_str());

				if (version == "OB54")
				{
					g_Globals.General.N32 = true;
					g_Globals.General.V31 = true;
					g_Globals.General.NoAnogs = true;
					FFTHV7A76();
					return;
				}
			}
		}

	}

	LibIl2Cpp = 0;
	printf("[GameConfig] No matching version found!\n");
}

void Offsets::FFTHV7A75() // v31 32-bit
{
	AccessClass = 0x5C; // TODO: update manually

	// GameVarDef
	GameVarDef::GameVarDef_TypeInfo = 0xABFF414; // TODO: update manually
	GameVarDef::ShootTraceAdjustmentDistanceThreshold = 0x674;
	GameVarDef::EnableAccelerationOnFalling = 0x27CA;
	GameVarDef::EnableLowFallingSwapWeapon = 0x2AE5;
	GameVarDef::RotationSensitivityMin = 0xF0C;
	GameVarDef::RotationSensitivityMax = 0xF10;
	GameVarDef::AimRotationSensitivityMin = 0xF14;
	GameVarDef::AimRotationSensitivityMax = 0xF18;

	// GameFacade
	GameFacade::GameFacade_TypeInfo = 0xABFF3C0; // TODO: update manually
	GameFacade::CurrentMatchGame = 0x4;

	// MatchGame
	MatchGame::m_Match = 0x50;
	MatchGame::m_CameraControllerManager = 0x74;

	// Match
	Match::m_State = 0x8C;
	Match::m_LocalPlayer = 0x94;
	Match::m_LocalObserver = 0xB4;
	Match::m_AttackableEntities = 0x140;

	// Camera
	CameraControllerManager::m_Camera = 0x10;
	Camera::m_CachedPtr = 0x8;
	Camera::ViewMatrix = 0xE8; // V7A: 0xE8 | FF MAX: 0xE4

	// Observer
	Observer::m_TargetPlayer = 0x28;

	// Player / PlayerNetwork
	// General
	Player::IsClientBot = 0x2E4;
	Player::IsFemale = 0x7D8;
	Player::IsPrepareAttack = 0x540;
	Player::m_IsCurFrameFowardLockToAimRot = 0x1D4;
	Player::m_WaitForForceSync = 0x520;
	Player::m_TransformType = 0xC4C;

	// Aim
	Player::m_AimRotation = 0x400;
	Player::m_AuxAimRotation = 0x410;
	Player::m_AimAssist = 0x420;
	Player::m_EAimAssit = 0x438;
	Player::m_AimAssistOnSighting = 0x43C;
	Player::m_LastAimingInfoFromWeapon = 0x978;

	// Transform / Camera
	Player::MainCameraTransform = 0x24C;
	Player::m_SwapWeaponTime = 0x51C;

	// Managers
	Player::m_Attributes = 0x4BC;
	Player::m_AvatarManager = 0x4C0;
	Player::m_InventoryManager = 0x4A8;

	// UserControlHandler
	Player::m_UserControl = 0x304;

	// Colliders
	Player::m_HeadCollider = 0x4A4;
	Player::m_fireColliders = 0x760;
	//Player::LockedAimingCollider = 0x54;

	// Bone Nodes
	Player::HeadNode = 0x458;
	Player::m_HipNode = 0x45C;
	Player::m_BloodEffectNode = 0x460;
	Player::m_RootNode = 0x46C;
	Player::m_BoneRootNode = 0x470;
	Player::m_WeaponMountNode = 0x454;
	Player::m_LeftWeaponNode = 0x484;
	Player::m_FlightNode = 0x468;
	Player::m_RightArmNode = 0x490;
	Player::m_LeftArmNode = 0x48C;
	Player::m_RightForeArmNode = 0x498;
	Player::m_LeftForeArmNode = 0x4A0;
	Player::m_RightHandNode = 0x494;
	Player::m_LeftHandNode = 0x49C;
	Player::m_RightAnkleNode = 0x478;
	Player::m_LeftAnkleNode = 0x474;
	Player::m_RightToeNode = 0x480;
	Player::m_LeftToeNode = 0x47C;

	// PlayerNetwork
	PlayerNetwork::m_ShadowState = 0x18B8;
	PlayerNetwork::m_Profile = 0x18CC;

	// Shadow
	ShadowState::TargetPhysXPose = 0x78;

	// PlayerAttributes
	PlayerAttributes::m_EatSpeedScale = 0x60;
	PlayerAttributes::m_FireIntervalScale = 0x18C;

	// AimAssistAutoLock
	AimAssistAutoLock::m_TargetHeuristic = 0xC;
	AimAssistAutoLock::m_Entity = 0xC;

	// UserControlHandler
	UserControlHandler::m_AxisData = 0x34;
	UserControlHandler::m_FingerInDashArea = 0x4C;
	UserControlHandler::m_IsTouched = 0x37;
	UserControlHandler::m_LockFingerInDashArea = 0x50;
	UserControlHandler::m_DashByMovingJoystick = 0x58;

	// AimAssistOnSighting
	AimAssistOnSighting::m_fAimAssistCurrentLerpTime = 0x44;

	// HitObjectInfo
	HitObjectInfo::RayDir = 0x2C;
	HitObjectInfo::StartPosition = 0x38;

	// InventoryManager
	InventoryManager::m_itemOnHand = 0x54;

	// Avatar
	AvatarManager::m_Avatar = 0xA8;
	UMAAvatarBase::umaData = 0x14;
	UmaAvatarSimple::IsVisible = 0x95;

	// UmaData
	UMAData::skeleton = 0xCC;
	UMAData::isLocalPlayer = 0x58;
	UMAData::isTeammate = 0x59;

	// Skeleton
	UMASkeleton::boneHashDataLookup = 0x18;
	UMASkeleton::boneNameHash = 0x8;
	UMASkeleton::boneTransform = 0x10;

	// Replication
	ReplicationEntity::m_PRIDataPool = 0x48;
	ReplicationEntity::m_Datas = 0x8;
	ReplicationEntity::HealthCurrentPtr = 0x10;
	ReplicationEntity::HealthMaxPtr = 0x14;
	ReplicationEntity::WeaponPtr = 0x20;
	ReplicationEntity::EpPtr = 0x28;
	ReplicationEntity::Value = 0x10;

	// Profile
	BaseProfileInfo::AccountID = 0x8;
	BaseProfileInfo::Level = 0x14;
	BaseProfileInfo::NickName = 0x18;

	// Weapon
	Weapon::FireComponent = 0x58;
	Weapon::m_WeaponData = 0x64;
	Weapon::m_WeaponParams = 0x6C;
	Weapon::m_FireDuration = 0x4BC;
	Weapon::m_IsSighting = 0x5E4;
	Weapon::tangentTheta = 0xC;
	Weapon::IntWeaponType = 0xB8;

	// WeaponParams
	WeaponParams::FullDamageDistance = 0x48;
	WeaponParams::PrefireDelay = 0x144;
	WeaponParams::Range = 0x44;

	// PlayerTransformNode
	PlayerTransformNode::Transform = 0x8;
	PlayerTransformNode::m_CachedTransform = 0x38;

	// get_position_Injected
	GetPosWorld::transObj = 0x8; // fixed (may change if Unity updates)
	GetPosWorld::matrix = 0x20; // fixed (may change if Unity updates)
	GetPosWorld::index = 0x24; // fixed (may change if Unity updates)
	GetPosWorld::matrix_list = 0x18; // fixed (may change if Unity updates)
	GetPosWorld::matrix_indices = 0x1C; // fixed (may change if Unity updates)

	// GetHeadPosition
	GetPosWorld::HeadColliderMale = 0x38; // fixed (may change if Unity updates)
	GetPosWorld::HeadColliderFemale = 0x3C; // fixed (may change if Unity updates)
	GetPosWorld::ColliderTransform = 0x8; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_1 = 0x28; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_2 = 0x14; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_3 = 0x60; // fixed (may change if Unity updates)
}

void Offsets::FFTHV7A76() // v31 32-bit
{
	AccessClass = 0x5C; // TODO: update manually

	// GameVarDef
	GameVarDef::GameVarDef_TypeInfo = 0xABFF734; // TODO: update manually
	GameVarDef::ShootTraceAdjustmentDistanceThreshold = 0x674;
	GameVarDef::EnableAccelerationOnFalling = 0x27CA;
	GameVarDef::EnableLowFallingSwapWeapon = 0x2AE5;
	GameVarDef::RotationSensitivityMin = 0xF0C;
	GameVarDef::RotationSensitivityMax = 0xF10;
	GameVarDef::AimRotationSensitivityMin = 0xF14;
	GameVarDef::AimRotationSensitivityMax = 0xF18;

	// GameFacade
	GameFacade::GameFacade_TypeInfo = 0xABFF6E0; // TODO: update manually
	GameFacade::CurrentMatchGame = 0x4;

	// MatchGame
	MatchGame::m_Match = 0x50;
	MatchGame::m_CameraControllerManager = 0x74;

	// Match
	Match::m_State = 0x8C;
	Match::m_LocalPlayer = 0x94;
	Match::m_LocalObserver = 0xB4;
	Match::m_AttackableEntities = 0x140;

	// Camera
	CameraControllerManager::m_Camera = 0x10;
	Camera::m_CachedPtr = 0x8;
	Camera::ViewMatrix = 0xE8; // V7A: 0xE8 | FF MAX: 0xE4

	// Observer
	Observer::m_TargetPlayer = 0x28;

	// Player / PlayerNetwork
	// General
	Player::IsClientBot = 0x2E4;
	Player::IsFemale = 0x7D8;
	Player::IsPrepareAttack = 0x540;
	Player::m_IsCurFrameFowardLockToAimRot = 0x1D4;
	Player::m_WaitForForceSync = 0x520;
	Player::m_TransformType = 0xC4C;

	// Aim
	Player::m_AimRotation = 0x400;
	Player::m_AuxAimRotation = 0x410;
	Player::m_AimAssist = 0x420;
	Player::m_EAimAssit = 0x438;
	Player::m_AimAssistOnSighting = 0x43C;
	Player::m_LastAimingInfoFromWeapon = 0x978;

	// Transform / Camera
	Player::MainCameraTransform = 0x24C;
	Player::m_SwapWeaponTime = 0x51C;

	// Managers
	Player::m_Attributes = 0x4BC;
	Player::m_AvatarManager = 0x4C0;
	Player::m_InventoryManager = 0x4A8;

	// UserControlHandler
	Player::m_UserControl = 0x304;

	// Colliders
	Player::m_HeadCollider = 0x4A4;
	Player::m_fireColliders = 0x760;
	//Player::LockedAimingCollider = 0x54;

	// Bone Nodes
	Player::HeadNode = 0x458;
	Player::m_HipNode = 0x45C;
	Player::m_BloodEffectNode = 0x460;
	Player::m_RootNode = 0x46C;
	Player::m_BoneRootNode = 0x470;
	Player::m_WeaponMountNode = 0x454;
	Player::m_LeftWeaponNode = 0x484;
	Player::m_FlightNode = 0x468;
	Player::m_RightArmNode = 0x490;
	Player::m_LeftArmNode = 0x48C;
	Player::m_RightForeArmNode = 0x498;
	Player::m_LeftForeArmNode = 0x4A0;
	Player::m_RightHandNode = 0x494;
	Player::m_LeftHandNode = 0x49C;
	Player::m_RightAnkleNode = 0x478;
	Player::m_LeftAnkleNode = 0x474;
	Player::m_RightToeNode = 0x480;
	Player::m_LeftToeNode = 0x47C;

	// PlayerNetwork
	PlayerNetwork::m_ShadowState = 0x18B8;
	PlayerNetwork::m_Profile = 0x18CC;

	// Shadow
	ShadowState::TargetPhysXPose = 0x78;

	// PlayerAttributes
	PlayerAttributes::m_EatSpeedScale = 0x60;
	PlayerAttributes::m_FireIntervalScale = 0x18C;

	// AimAssistAutoLock
	AimAssistAutoLock::m_TargetHeuristic = 0xC;
	AimAssistAutoLock::m_Entity = 0xC;

	// UserControlHandler
	UserControlHandler::m_AxisData = 0x34;
	UserControlHandler::m_FingerInDashArea = 0x4C;
	UserControlHandler::m_IsTouched = 0x37;
	UserControlHandler::m_LockFingerInDashArea = 0x50;
	UserControlHandler::m_DashByMovingJoystick = 0x58;

	// AimAssistOnSighting
	AimAssistOnSighting::m_fAimAssistCurrentLerpTime = 0x44;

	// HitObjectInfo
	HitObjectInfo::RayDir = 0x2C;
	HitObjectInfo::StartPosition = 0x38;

	// InventoryManager
	InventoryManager::m_itemOnHand = 0x54;

	// Avatar
	AvatarManager::m_Avatar = 0xA8;
	UMAAvatarBase::umaData = 0x14;
	UmaAvatarSimple::IsVisible = 0x95;

	// UmaData
	UMAData::skeleton = 0xCC;
	UMAData::isLocalPlayer = 0x58;
	UMAData::isTeammate = 0x59;

	// Skeleton
	UMASkeleton::boneHashDataLookup = 0x18;
	UMASkeleton::boneNameHash = 0x8;
	UMASkeleton::boneTransform = 0x10;

	// Replication
	ReplicationEntity::m_PRIDataPool = 0x48;
	ReplicationEntity::m_Datas = 0x8;
	ReplicationEntity::HealthCurrentPtr = 0x10;
	ReplicationEntity::HealthMaxPtr = 0x14;
	ReplicationEntity::WeaponPtr = 0x20;
	ReplicationEntity::EpPtr = 0x28;
	ReplicationEntity::Value = 0x10;

	// Profile
	BaseProfileInfo::AccountID = 0x8;
	BaseProfileInfo::Level = 0x14;
	BaseProfileInfo::NickName = 0x18;

	// Weapon
	Weapon::FireComponent = 0x58;
	Weapon::m_WeaponData = 0x64;
	Weapon::m_WeaponParams = 0x6C;
	Weapon::m_FireDuration = 0x4BC;
	Weapon::m_IsSighting = 0x5E4;
	Weapon::tangentTheta = 0xC;
	Weapon::IntWeaponType = 0xB8;

	// WeaponParams
	WeaponParams::FullDamageDistance = 0x48;
	WeaponParams::PrefireDelay = 0x144;
	WeaponParams::Range = 0x44;

	// PlayerTransformNode
	PlayerTransformNode::Transform = 0x8;
	PlayerTransformNode::m_CachedTransform = 0x38;

	// get_position_Injected
	GetPosWorld::transObj = 0x8; // fixed (may change if Unity updates)
	GetPosWorld::matrix = 0x20; // fixed (may change if Unity updates)
	GetPosWorld::index = 0x24; // fixed (may change if Unity updates)
	GetPosWorld::matrix_list = 0x18; // fixed (may change if Unity updates)
	GetPosWorld::matrix_indices = 0x1C; // fixed (may change if Unity updates)

	// GetHeadPosition
	GetPosWorld::HeadColliderMale = 0x38; // fixed (may change if Unity updates)
	GetPosWorld::HeadColliderFemale = 0x3C; // fixed (may change if Unity updates)
	GetPosWorld::ColliderTransform = 0x8; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_1 = 0x28; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_2 = 0x14; // fixed (may change if Unity updates)
	GetPosWorld::BoundsCenter_3 = 0x60; // fixed (may change if Unity updates)
}