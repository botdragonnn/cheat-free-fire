#pragma once
#include <Windows.h>
#include <Math/Vectors/Vector3.hpp>
#include <Main/Unity/Unity.hpp>

namespace Silent
{
	constexpr int WRITE_LOOP_COUNT = 28000;
	constexpr float SMOOTH_MIN = 0.0015f;
	constexpr float SMOOTH_MAX = 0.0035f;

	extern volatile LONG g_Running;

	void Start();
	void Stop();
	void UpdateViewMatrix(const Matrix4x4& matrix);

	void SetTarget(uintptr_t localPlayer, uintptr_t targetEntity);
	void ClearTarget();
}
