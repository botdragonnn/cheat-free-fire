#include "Skeleton.hpp"
#include <Main/Offsets/Offsets.hpp>

namespace
{
	static constexpr int BONE_COUNT = 49;
	static constexpr int MAX_CACHE = 64;
	static constexpr int MAX_HIERARCHY = 256;

	static constexpr int kBoneHashes[BONE_COUNT] = {
		858159017, -2111735698, -1919801108, -1809306289, -1541408846,
		-1391784435, -1367065569, -1305646021, -1258743979, -1197037021,
		-1129867206, -1051086991, -1032680759, -1026310613, -617253997,
		-613827704, -607613078, -379594486, -369168920, -353109963,
		-344692431, -285661123, -263919541, -253371223, -248343593,
		-115488425, 92030410, 96688289, 220298485, 345733462,
		640639971, 952826536, 1082519766, 1113647666, 1120001744,
		1121682369, 1179749304, 1507255706, 1529948125, 1534843763,
		1541074833, 1574426499, 1604555488, 1766701553, 1777110291,
		1884719280, 1892485702, 1895283794, 2018908708
	};

	int HashToId(int hash)
	{
		for (int i = 0; i < BONE_COUNT; ++i)
			if (kBoneHashes[i] == hash) return i;
		return -1;
	}

	struct Conn
	{
		int a, b;
	};

	static constexpr Conn kBodyStyle0[] = {
		{ 1, 27 }, { 27, 4 }, { 4, 11 }, { 11, 38 },
		{ 27, 14 }, { 14, 42 }, { 42, 10 }, { 10, 46 },
		{ 27, 26 }, { 26, 5 }, { 5, 37 }, { 37, 6 },
		{ 38, 21 }, { 21, 7 }, { 7, 20 }, { 20, 8 },
		{ 38, 31 }, { 31, 32 }, { 32, 25 }, { 25, 36 },
	};
	static constexpr int kBodyStyle0Count = sizeof(kBodyStyle0) / sizeof(kBodyStyle0[0]);

	static constexpr Conn kBodyStyle1[] = {
		{ 1, 27 }, { 27, 4 }, { 4, 11 },
		{ 27, 42 }, { 42, 10 }, { 10, 46 },
		{ 27, 5 }, { 5, 37 }, { 37, 6 },
		{ 11, 21 }, { 21, 7 }, { 7, 20 }, { 20, 8 },
		{ 11, 31 }, { 31, 32 }, { 32, 25 }, { 25, 36 },
	};
	static constexpr int kBodyStyle1Count = sizeof(kBodyStyle1) / sizeof(kBodyStyle1[0]);

	static constexpr Conn kFingers[] = {
		{ 46, 43 }, { 43, 22 }, { 46, 45 }, { 45, 17 },
		{ 46, 39 }, { 39, 12 }, { 46, 33 }, { 33, 15 },
		{ 46, 28 }, { 28, 3 }, { 6, 18 }, { 18, 47 },
		{ 6, 23 }, { 23, 44 }, { 6, 16 }, { 16, 34 },
		{ 6, 13 }, { 13, 40 }, { 6, 2 }, { 2, 29 },
	};
	static constexpr int kFingersCount = sizeof(kFingers) / sizeof(kFingers[0]);

	struct BoneCache
	{
		uintptr_t entity;
		uintptr_t UMAData;
		uintptr_t tAccess[BONE_COUNT];  // TransformAccess* cacheado (estavel)
	};

	static BoneCache g_Cache[MAX_CACHE];
	static int g_CacheCount = 0;

	BoneCache* FindCache(uintptr_t entity, uintptr_t umaData)
	{
		for (int i = 0; i < g_CacheCount; ++i)
		{
			if (g_Cache[i].entity == entity && g_Cache[i].UMAData == umaData)
			{
				return &g_Cache[i];
			}
		}
		return nullptr;
	}

	BoneCache* AllocCache()
	{
		if (g_CacheCount < MAX_CACHE)
		{
			return &g_Cache[g_CacheCount++];
		}
		return &g_Cache[0];
	}

	// Mesma math do Transform::get_position_Injected, mas usando arrays locais
	Vector3 CalcPosition(int idx, const TMatrix* matrices, const int* parents)
	{
		if (idx < 0 || idx >= MAX_HIERARCHY) return Vector3::Zero();

		Vector3 result;
		result.X = matrices[idx].Position.x;
		result.Y = matrices[idx].Position.y;
		result.Z = matrices[idx].Position.z;

		int cur = parents[idx];
		int safety = 0;
		while (cur >= 0 && cur < MAX_HIERARCHY && safety++ < 60)
		{
			const TMatrix& m = matrices[cur];

			float rotX = m.Rotation.x, rotY = m.Rotation.y;
			float rotZ = m.Rotation.z, rotW = m.Rotation.w;
			float scaleX = result.X * m.Scale.x;
			float scaleY = result.Y * m.Scale.y;
			float scaleZ = result.Z * m.Scale.z;

			result.X = m.Position.x + scaleX + (scaleX * ((rotY * rotY * -2.f) - (rotZ * rotZ * 2.f))) + (scaleY * ((rotW * rotZ * -2.f) - (rotY * rotX * -2.f))) + (scaleZ * ((rotZ * rotX * 2.f) - (rotW * rotY * -2.f)));
			result.Y = m.Position.y + scaleY + (scaleX * ((rotX * rotY * 2.f) - (rotW * rotZ * -2.f))) + (scaleY * ((rotZ * rotZ * -2.f) - (rotX * rotX * 2.f))) + (scaleZ * ((rotW * rotX * -2.f) - (rotZ * rotY * -2.f)));
			result.Z = m.Position.z + scaleZ + (scaleX * ((rotW * rotY * -2.f) - (rotX * rotZ * -2.f))) + (scaleY * ((rotY * rotZ * 2.f) - (rotW * rotX * -2.f))) + (scaleZ * ((rotX * rotX * -2.f) - (rotY * rotY * 2.f)));

			cur = parents[cur];
		}

		return result;
	}
}

bool Skeleton::DrawPlayer(ImDrawList* drawList, uintptr_t entity, uintptr_t umaData, bool isKnocked, const Matrix4x4& viewMatrix, bool N32, bool V31)
{
	if (!g_Globals.Visuals.ESP.Skeleton)
	{
		return false;
	}

	auto ReadPtr = [N32](uintptr_t addr) -> uintptr_t
		{
			return N32 ? g_FreeFireMemory.Read<uint32_t>(addr) : g_FreeFireMemory.Read<uint64_t>(addr);
		};

	// ===== 1. Pegar ou construir cache dos tAccess =====
	BoneCache* bc = FindCache(entity, umaData);

	if (!bc)
	{
		uintptr_t skel = ReadPtr(umaData + Offsets::UMAData::skeleton);
		if (!skel) return false;

		uintptr_t dictAddr = ReadPtr(skel + Offsets::UMASkeleton::boneHashDataLookup);
		if (!dictAddr) return false;

		bc = AllocCache();
		memset(bc, 0, sizeof(BoneCache));
		bc->entity = entity;
		bc->UMAData = umaData;

		auto IterateDict = [&](auto* dict)
			{
				int count = dict->GetNumValues();
				if (count <= 0) return;

				for (int i = 0; i < count; ++i)
				{
					uintptr_t boneData = dict->GetValue(i);
					if (!boneData) continue;

					int hash = g_FreeFireMemory.Read<int>(boneData + Offsets::UMASkeleton::boneNameHash);
					int id = HashToId(hash);
					if (id < 0) continue;

					uintptr_t transform = ReadPtr(boneData + Offsets::UMASkeleton::boneTransform);
					if (!transform) continue;

					uintptr_t tAccess = ReadPtr(transform + Offsets::GetPosWorld::transObj);
					if (!tAccess) continue;

					bc->tAccess[id] = tAccess;
				}
			};

		if (N32 && V31)
			IterateDict(reinterpret_cast<Offsets::UnityDictionary<true, true>*>(dictAddr));
		else if (N32 && !V31)
			IterateDict(reinterpret_cast<Offsets::UnityDictionary<true, false>*>(dictAddr));
		else if (!N32 && V31)
			IterateDict(reinterpret_cast<Offsets::UnityDictionary<false, true>*>(dictAddr));
		else
			IterateDict(reinterpret_cast<Offsets::UnityDictionary<false, false>*>(dictAddr));

		if (!bc->tAccess[1] || !bc->tAccess[27] || !bc->tAccess[4])
		{
			return false;
		}
	}

	// ===== 2. Ler hierarquia a partir de qualquer tAccess valido =====
	uintptr_t anyAccess = bc->tAccess[1];
	uintptr_t hierBase = ReadPtr(anyAccess + Offsets::GetPosWorld::matrix);
	if (!hierBase) return false;

	uintptr_t pValuesAddr = ReadPtr(hierBase + Offsets::GetPosWorld::matrix_list);
	uintptr_t pParentsAddr = ReadPtr(hierBase + Offsets::GetPosWorld::matrix_indices);
	if (!pValuesAddr || !pParentsAddr) return false;

	// ===== 3. Bulk read: 2 leituras pra todos os dados =====
	static TMatrix matrices[MAX_HIERARCHY];
	static int parents[MAX_HIERARCHY];

	if (!g_FreeFireMemory.Read(pValuesAddr, matrices, sizeof(TMatrix) * MAX_HIERARCHY)) return false;
	if (!g_FreeFireMemory.Read(pParentsAddr, parents, sizeof(int) * MAX_HIERARCHY)) return false;

	// ===== 4. Re-ler indices e calcular posicoes (tudo local, 0 reads) =====
	Vector3 pos[BONE_COUNT];

	for (int i = 0; i < BONE_COUNT; ++i)
	{
		if (!bc->tAccess[i])
		{
			pos[i] = Vector3::Zero();
			continue;
		}

		int idx = g_FreeFireMemory.Read<int>(bc->tAccess[i] + Offsets::GetPosWorld::index);
		pos[i] = CalcPosition(idx, matrices, parents);
	}

	// ===== 5. Desenhar =====
	ImU32 col = isKnocked ? ImColor(255, 0, 0, 255) : ImColor(g_Globals.Visuals.ESP.SkeletonColor[0], g_Globals.Visuals.ESP.SkeletonColor[1], g_Globals.Visuals.ESP.SkeletonColor[2], g_Globals.Visuals.ESP.SkeletonColor[3]);
	float thick = 1.0f;

	auto DrawConns = [&](const Conn* conns, int count)
		{
			for (int i = 0; i < count; ++i)
			{
				const Vector3& a = pos[conns[i].a];
				const Vector3& b = pos[conns[i].b];
				if (a == Vector3::Zero() || b == Vector3::Zero()) continue;

				Vector3 sa = W2S::World2Screen(viewMatrix, a);
				Vector3 sb = W2S::World2Screen(viewMatrix, b);
				if (sa.Z > 0 && sb.Z > 0)
				{
					drawList->AddLine(ImVec2(sa.X, sa.Y), ImVec2(sb.X, sb.Y), col, thick);
				}
			}
		};

	if (g_Globals.Visuals.ESP.SkeletonStyle == 0)
		DrawConns(kBodyStyle0, kBodyStyle0Count);
	else
		DrawConns(kBodyStyle1, kBodyStyle1Count);

	if (g_Globals.Visuals.ESP.SkeletonFingers)
		DrawConns(kFingers, kFingersCount);

	return true;
}

void Skeleton::CleanupCache(const std::unordered_set<uintptr_t>& activeEntities)
{
	for (int i = 0; i < g_CacheCount; )
	{
		if (activeEntities.find(g_Cache[i].entity) == activeEntities.end())
		{
			g_Cache[i] = g_Cache[--g_CacheCount];
		}
		else
		{
			++i;
		}
	}
}

void Skeleton::ClearCache()
{
	g_CacheCount = 0;
}
