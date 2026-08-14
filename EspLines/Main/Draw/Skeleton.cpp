#include "Skeleton.hpp"
#include <Main/Offsets/Offsets.hpp>
#include <cmath>

namespace
{
	static constexpr int BONE_COUNT = 49;
	static constexpr int MAX_CACHE = 64;
	// Janela da hierarquia lida em bulk. 256 entradas nao cobria avatares com
	// indice de bone alto na TransformHierarchy (a cadeia de pais escapa da
	// janela e o skeleton resolvia TRS de OUTROS objetos da cena — esqueleto
	// deslocado/no chao). 1024 cobre o bloco de bones de qualquer avatar.
	static constexpr int MAX_HIERARCHY = 1024;

	// Bone a mais de 6m do personagem e' lixo (ponteiro reutilizado pelo jogo,
	// hierarquia truncada ou leitura rasgada) — nunca e' desenhado.
	static constexpr float kMaxBoneDistance = 6.0f;
	// Minimo de bones validos para considerar o skeleton bom; abaixo disso o
	// cache de tAccess e' reconstruido (avatar staleless) e, se persistir,
	// o caminho UMA devolve false (cai no fallback de bones da entidade).
	static constexpr int kMinValidBones = 10;

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

	// =====================================================================
	// Fallback p/ entidades sem UMA utilizavel (bots da ilha de treinamento
	// saem pelo caminho UMA com skeleton/dicionario inexistente): os 18
	// bones principais sao lidos direto da ENTIDADE (offsets fixos do
	// PlayerBase, mesma versao do jogo), e cada um e' um Transform/Node que
	// resolve para a mesma TransformAccessArray usada pelo caminho UMA.
	// =====================================================================

	static constexpr int FB_COUNT = 18;

	// Head, Neck, LShoulder, RShoulder, LElbow, RElbow, LWrist, RWrist,
	// LHand, RHand, Hip, Groin, Root, RootBone, LAnkle, RAnkle, LFoot, RFoot
	static const uintptr_t kEntityBoneOffsets[FB_COUNT] = {
		0x458, 0x460, 0x48C, 0x490, 0x49C, 0x4A0,
		0x454, 0x480, 0x484, 0x494, 0x468, 0x45C,
		0x46C, 0x464, 0x474, 0x470, 0x47C, 0x478
	};

	enum FB_Bone
	{
		FB_HEAD, FB_NECK, FB_LSHOULDER, FB_RSHOULDER, FB_LELBOW, FB_RELBOW,
		FB_LWRIST, FB_RWRIST, FB_LHAND, FB_RHAND, FB_HIP, FB_GROIN,
		FB_ROOT, FB_ROOTBONE, FB_LANKLE, FB_RANKLE, FB_LFOOT, FB_RFOOT
	};

	struct FallbackCache
	{
		uintptr_t entity;
		uintptr_t boneNode[FB_COUNT];
	};

	static FallbackCache g_FbCache[MAX_CACHE];
	static int g_FbCacheCount = 0;

	static FallbackCache* FindFallbackCache( uintptr_t entity )
	{
		for ( int i = 0; i < g_FbCacheCount; ++i )
			if ( g_FbCache[ i ].entity == entity )
				return &g_FbCache[ i ];
		return nullptr;
	}

	static FallbackCache* AllocFallbackCache( )
	{
		if ( g_FbCacheCount < MAX_CACHE )
			return &g_FbCache[ g_FbCacheCount++ ];
		return &g_FbCache[ 0 ];
	}

	// Resolve um bone node do Entity ate a TransformAccessArray
	static bool ResolveBoneAccess( uintptr_t boneNode, bool N32, int& outIndex, uintptr_t& outMatrixList, uintptr_t& outMatrixIndices )
	{
		auto ReadPtr = [ N32 ] ( uintptr_t a ) -> uintptr_t
			{
				return N32 ? g_FreeFireMemory.Read<uint32_t>( a ) : g_FreeFireMemory.Read<uint64_t>( a );
			};

		uintptr_t tr = ReadPtr( boneNode + Offsets::GetPosWorld::transObj );        // +0x8
		if ( !tr ) return false;
		uintptr_t ta = ReadPtr( tr + Offsets::GetPosWorld::transObj );              // +0x8
		if ( !ta ) return false;

		outIndex = g_FreeFireMemory.Read<int>( ta + Offsets::GetPosWorld::index );  // +0x24
		uintptr_t matrix = ReadPtr( ta + Offsets::GetPosWorld::matrix );            // +0x20
		if ( !matrix ) return false;

		outMatrixList = ReadPtr( matrix + Offsets::GetPosWorld::matrix_list );      // +0x18
		outMatrixIndices = ReadPtr( matrix + Offsets::GetPosWorld::matrix_indices );// +0x1C

		return outMatrixList != 0 && outMatrixIndices != 0
			&& outIndex >= 0 && outIndex < MAX_HIERARCHY;
	}

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

bool Skeleton::DrawPlayer(ImDrawList* drawList, uintptr_t entity, uintptr_t umaData, bool isKnocked, const Matrix4x4& viewMatrix, const Vector3& entityPos, bool N32, bool V31)
{
	if (!g_Globals.Visuals.ESP.Skeleton)
	{
		return false;
	}

	// Caminho principal: UMA (hash de bones). Se a entidade nao tem UMA
	// utilizavel (bots da ilha de treinamento), cai no fallback de bones
	// diretos da entidade.
	if (DrawPlayerUma( drawList, entity, umaData, isKnocked, viewMatrix, entityPos, N32, V31 ))
		return true;

	return DrawPlayerEntityBones( drawList, entity, isKnocked, viewMatrix, entityPos, N32 );
}

bool Skeleton::DrawPlayerUma(ImDrawList* drawList, uintptr_t entity, uintptr_t umaData, bool isKnocked, const Matrix4x4& viewMatrix, const Vector3& entityPos, bool N32, bool V31)
{
	auto ReadPtr = [N32](uintptr_t addr) -> uintptr_t
		{
			return N32 ? g_FreeFireMemory.Read<uint32_t>(addr) : g_FreeFireMemory.Read<uint64_t>(addr);
		};

	// Posicao de bone so vale se for finita e ficar dentro do raio do
	// personagem. Descartar lixo aqui e' o que impede o skeleton de resolver
	// TRS de outros objetos da cena (cadeia de pais fora da janela de leitura)
	// ou de um avatar reutilizado (ponteiro stale) — sintoma de esqueleto
	// deslocado, no chao ou ao lado do personagem.
	auto IsUsableBone = [&](const Vector3& p) -> bool
		{
			if (!std::isfinite(p.X) || !std::isfinite(p.Y) || !std::isfinite(p.Z)) return false;
			if (entityPos != Vector3::Zero() && Vector3::Distance(p, entityPos) > kMaxBoneDistance) return false;
			return true;
		};

	// ===== 1. Construir cache dos tAccess (hash -> bone) =====
	auto BuildCache = [&]() -> BoneCache*
		{
			uintptr_t skel = ReadPtr(umaData + Offsets::UMAData::skeleton);
			if (!skel) return nullptr;

			uintptr_t dictAddr = ReadPtr(skel + Offsets::UMASkeleton::boneHashDataLookup);
			if (!dictAddr) return nullptr;

			BoneCache* nb = AllocCache();
			memset(nb, 0, sizeof(BoneCache));
			nb->entity = entity;
			nb->UMAData = umaData;

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

						nb->tAccess[id] = tAccess;
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

			if (!nb->tAccess[1] || !nb->tAccess[27] || !nb->tAccess[4])
			{
				--g_CacheCount;
				return nullptr;
			}

			return nb;
		};

	auto RemoveCache = [&]()
		{
			for (int i = 0; i < g_CacheCount; ++i)
			{
				if (g_Cache[i].entity == entity && g_Cache[i].UMAData == umaData)
				{
					g_Cache[i] = g_Cache[--g_CacheCount];
					return;
				}
			}
		};

	BoneCache* bc = FindCache(entity, umaData);
	if (!bc) bc = BuildCache();
	if (!bc) return false;

	// ===== 2/3/4. Hierarquia + posicoes, com rebuild se o cache estiver stale =====
	static TMatrix matrices[MAX_HIERARCHY];
	static int parents[MAX_HIERARCHY];
	Vector3 pos[BONE_COUNT];
	int validCount = 0;

	for (int attempt = 0; attempt < 2; ++attempt)
	{
		if (attempt > 0)
		{
			// Todos os bones do cache sairam invalidos: tAccess stale
			// (avatar/ponteiro reutilizado pelo jogo) — descarta e re-resolve.
			RemoveCache();
			bc = BuildCache();
			if (!bc) return false;
		}

		uintptr_t anyAccess = bc->tAccess[1];
		uintptr_t hierBase = ReadPtr(anyAccess + Offsets::GetPosWorld::matrix);
		if (!hierBase) continue;

		uintptr_t pValuesAddr = ReadPtr(hierBase + Offsets::GetPosWorld::matrix_list);
		uintptr_t pParentsAddr = ReadPtr(hierBase + Offsets::GetPosWorld::matrix_indices);
		if (!pValuesAddr || !pParentsAddr) continue;

		if (!g_FreeFireMemory.Read(pValuesAddr, matrices, sizeof(TMatrix) * MAX_HIERARCHY)) continue;
		if (!g_FreeFireMemory.Read(pParentsAddr, parents, sizeof(int) * MAX_HIERARCHY)) continue;

		validCount = 0;
		for (int i = 0; i < BONE_COUNT; ++i)
		{
			pos[i] = Vector3::Zero();
			if (!bc->tAccess[i]) continue;

			int idx = g_FreeFireMemory.Read<int>(bc->tAccess[i] + Offsets::GetPosWorld::index);
			Vector3 p = CalcPosition(idx, matrices, parents);
			if (IsUsableBone(p))
			{
				pos[i] = p;
				++validCount;
			}
		}

		if (validCount >= kMinValidBones) break;
	}

	if (validCount < kMinValidBones) return false;

	// ===== 5. Desenhar =====
	ImU32 col = isKnocked ? ImColor(255, 0, 0, 255) : ImColor(g_Globals.Visuals.ESP.SkeletonColor[0], g_Globals.Visuals.ESP.SkeletonColor[1], g_Globals.Visuals.ESP.SkeletonColor[2], g_Globals.Visuals.ESP.SkeletonColor[3]);
	float thick = 1.0f;

	// Flag de degeneracao: se NENHUMA linha for desenhada (bones ausentes no
	// UMA deste player), o caminho UMA devolve false e o DrawPlayer cai no
	// fallback de bones diretos da entidade — senao o player fica sem
	// skeleton ("skeleton quebrado" em alguns players).
	bool anyDrawn = false;

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
					anyDrawn = true;
				}
			}
		};

	if (g_Globals.Visuals.ESP.SkeletonStyle == 0)
		DrawConns(kBodyStyle0, kBodyStyle0Count);
	else
		DrawConns(kBodyStyle1, kBodyStyle1Count);

	if (g_Globals.Visuals.ESP.SkeletonFingers)
		DrawConns(kFingers, kFingersCount);

	return anyDrawn;
}

// ===== Fallback: bones diretos da entidade (bots da ilha de treinamento) =====

bool Skeleton::DrawPlayerEntityBones(ImDrawList* drawList, uintptr_t entity, bool isKnocked, const Matrix4x4& viewMatrix, const Vector3& entityPos, bool N32)
{
	auto ReadPtr = [N32](uintptr_t addr) -> uintptr_t
		{
			return N32 ? g_FreeFireMemory.Read<uint32_t>(addr) : g_FreeFireMemory.Read<uint64_t>(addr);
		};

	// 1. Cache dos bone nodes (ponteiros estaveis)
	FallbackCache* fc = FindFallbackCache( entity );
	if ( !fc )
	{
		FallbackCache* nf = AllocFallbackCache( );
		memset( nf, 0, sizeof( FallbackCache ) );
		nf->entity = entity;
		fc = nf;

		for ( int i = 0; i < FB_COUNT; ++i )
		{
			uintptr_t node = ReadPtr( entity + kEntityBoneOffsets[ i ] );
			if ( node )
				fc->boneNode[ i ] = node;
		}

		// Sem nenhum bone, nao ha o que fazer
		bool any = false;
		for ( int i = 0; i < FB_COUNT; ++i )
			if ( fc->boneNode[ i ] ) { any = true; break; }
		if ( !any )
			return false;
	}

	// 2. Hierarquia (bulk read, igual ao caminho UMA)
	static TMatrix matrices[MAX_HIERARCHY];
	static int parents[MAX_HIERARCHY];

	uintptr_t matrixList = 0, matrixIndices = 0;
	int baseIndex = -1;

	for ( int i = 0; i < FB_COUNT; ++i )
	{
		if ( !fc->boneNode[ i ] ) continue;
		if ( ResolveBoneAccess( fc->boneNode[ i ], N32, baseIndex, matrixList, matrixIndices ) )
			break;
	}
	if ( baseIndex < 0 || !matrixList || !matrixIndices ) return false;

	if ( !g_FreeFireMemory.Read( matrixList, matrices, sizeof( TMatrix ) * MAX_HIERARCHY ) ) return false;
	if ( !g_FreeFireMemory.Read( matrixIndices, parents, sizeof( int ) * MAX_HIERARCHY ) ) return false;

	// 3. Posicoes dos 18 bones
	Vector3 pos[FB_COUNT];
	int validCount = 0;

	for ( int i = 0; i < FB_COUNT; ++i )
	{
		if ( !fc->boneNode[ i ] )
		{
			pos[ i ] = Vector3::Zero( );
			continue;
		}

		int idx = -1;
		uintptr_t ml = 0, mi = 0;
		if ( ResolveBoneAccess( fc->boneNode[ i ], N32, idx, ml, mi ) )
		{
			Vector3 p = CalcPosition( idx, matrices, parents );
			// Mesma validacao do caminho UMA: bone longe do personagem ou
			// com valores nao-finitos e' lixo (cache stale / leitura rasgada)
			// e nao pode ser desenhado.
			if ( std::isfinite( p.X ) && std::isfinite( p.Y ) && std::isfinite( p.Z )
				&& ( entityPos == Vector3::Zero( ) || Vector3::Distance( p, entityPos ) <= kMaxBoneDistance ) )
			{
				pos[ i ] = p;
				++validCount;
			}
			else
			{
				pos[ i ] = Vector3::Zero( );
			}
		}
		else
		{
			pos[ i ] = Vector3::Zero( );
		}
	}

	// Menos da metade dos bones validos: provavelmente offsets errados —
	// nao desenha nada em vez de linhas malucas na tela.
	if ( validCount < FB_COUNT / 2 )
		return false;

	// 4. Desenha
	ImU32 col = isKnocked ? ImColor(255, 0, 0, 255) : ImColor(g_Globals.Visuals.ESP.SkeletonColor[0], g_Globals.Visuals.ESP.SkeletonColor[1], g_Globals.Visuals.ESP.SkeletonColor[2], g_Globals.Visuals.ESP.SkeletonColor[3]);
	const float thick = 1.0f;

	auto W2S = [ &viewMatrix ] ( const Vector3& w ) -> Vector3 { return W2S::World2Screen( viewMatrix, w ); };

	auto DrawConn = [ & ] ( int a, int b )
		{
			const Vector3& pa = pos[ a ];
			const Vector3& pb = pos[ b ];
			if ( pa == Vector3::Zero( ) || pb == Vector3::Zero( ) ) return;

			Vector3 sa = W2S( pa );
			Vector3 sb = W2S( pb );
			if ( sa.Z > 0 && sb.Z > 0 )
				drawList->AddLine( ImVec2( sa.X, sa.Y ), ImVec2( sb.X, sb.Y ), col, thick );
		};

	// Coluna
	DrawConn( FB_HEAD, FB_NECK );
	DrawConn( FB_NECK, FB_HIP );

	// Braco esquerdo
	DrawConn( FB_NECK, FB_LSHOULDER );
	DrawConn( FB_LSHOULDER, FB_LELBOW );
	DrawConn( FB_LELBOW, FB_LWRIST );
	DrawConn( FB_LWRIST, FB_LHAND );

	// Braco direito
	DrawConn( FB_NECK, FB_RSHOULDER );
	DrawConn( FB_RSHOULDER, FB_RELBOW );
	DrawConn( FB_RELBOW, FB_RWRIST );
	DrawConn( FB_RWRIST, FB_RHAND );

	// Pelve
	DrawConn( FB_HIP, FB_GROIN );
	DrawConn( FB_GROIN, FB_ROOTBONE );
	DrawConn( FB_ROOTBONE, FB_ROOT );

	// Perna esquerda
	DrawConn( FB_HIP, FB_LANKLE );
	DrawConn( FB_LANKLE, FB_LFOOT );

	// Perna direita
	DrawConn( FB_HIP, FB_RANKLE );
	DrawConn( FB_RANKLE, FB_RFOOT );

	// Cabeca (circulo proporcional ao pescoco)
	if ( pos[ FB_HEAD ] != Vector3::Zero( ) && pos[ FB_NECK ] != Vector3::Zero( ) )
	{
		Vector3 sh = W2S( pos[ FB_HEAD ] );
		if ( sh.Z > 0 )
		{
			float headRadius = Vector3::Distance( pos[ FB_HEAD ], pos[ FB_NECK ] ) * 0.35f;
			drawList->AddCircle( ImVec2( sh.X, sh.Y ), headRadius, col, 0, thick );
		}
	}

	return true;
}

void Skeleton::CleanupCache(const std::unordered_set<uintptr_t>& activeEntities)
{
	for ( int i = 0; i < g_CacheCount; )
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

	for ( int i = 0; i < g_FbCacheCount; )
	{
		if ( activeEntities.find( g_FbCache[ i ].entity ) == activeEntities.end( ) )
		{
			g_FbCache[ i ] = g_FbCache[ --g_FbCacheCount ];
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
	g_FbCacheCount = 0;
}
