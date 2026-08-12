#pragma once

#include <imgui.h>
#include <unordered_set>
#include <Main/Unity/Unity.hpp>

namespace Skeleton
{
	bool DrawPlayer(ImDrawList* drawList, uintptr_t entity, uintptr_t umaData, bool isKnocked, const Matrix4x4& viewMatrix, bool N32, bool V31);
	void CleanupCache(const std::unordered_set<uintptr_t>& activeEntities);
	void ClearCache();
}
