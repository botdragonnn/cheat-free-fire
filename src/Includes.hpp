#pragma once

#include <Windows.h>
#include <functional>
#include <iostream>
#include <string>
#include <cstdio>
#include <dwmapi.h>
#include <algorithm>
#include <chrono>
#include <thread>

// GL
#include <GL/gl.h>
#include <GL/glext.h>

// ImGui
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <ImGui/imspinner.h>
#include <XorStr.hpp>
#undef XorStr
#define XorStr(str) xorstr_(str)