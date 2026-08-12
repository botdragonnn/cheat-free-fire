#pragma once

#include <Windows.h>

namespace Render {
    HWND LookupWindowByClassName(const char* toFindClassName = "BlueStacksApp");
}