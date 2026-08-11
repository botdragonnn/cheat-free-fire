#pragma once
#include "Includes.hpp"
#include <Render/Overlay/Overlay.hpp>
#include <Render/Overlay/Render/Render.hpp>
#include <Render/Interface/Interface.hpp>
#include <Utils/Utils.hpp>
#include "Globals.hpp"

namespace Cheat {
	void Initialize();
	DWORD WINAPI Unload();
}