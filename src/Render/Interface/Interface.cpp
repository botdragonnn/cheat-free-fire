#include "Interface.hpp"
#include "Notify/Notify.hpp"
#include <Main/Memory/Memory.hpp>
#include <Cheat/saveconfig.cpp>
#include <Cheat/SharedMemory.h>
#include <Cheat/WebPanel.hpp>
#include <shellapi.h>
#include <cmath>
#include <XorStr.hpp>
#include <ext/KeyAuth/KeyAuth.hpp>
#include <ext/Discord/DiscordRPC.hpp>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static char g_LicenseKey[128] = "";

static KeyAuth::api g_KeyAuth(
	"zimon base",
	"97I2iXQCxX",
	"76a9a0cb94cde6b2a5f62bda0f8e791d560cc9383a9c4d8d94e54b95e40d5983",
	"1.0",
	"https://keyauth.win/api/1.2/"
);



static float g_WindowScale = 0.0f;
static float g_WindowAlpha = 0.0f;
static float g_ContentAlpha = 0.0f;
static ImVec2 g_WindowPos = ImVec2(-1, -1);
static bool g_Dragging = false;
static ImVec2 g_DragOffset = ImVec2(0, 0);

void Interface::Initialize(HWND Window, HWND TargetWindow, HDC DeviceContext, HGLRC RenderContext)
{
	hWindow = Window;
	hTargetWindow = TargetWindow;
	hDeviceContext = DeviceContext;
	hRenderContext = RenderContext;

	wglMakeCurrent(hDeviceContext, hRenderContext);

	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWindow);
	ImGui_ImplOpenGL3_Init(XorStr("#version 130"));
	InitializeMenu();

	std::thread([]()
		{
			NotifyManager::Send(XorStr("Bem Vindo(a)"), 4000);
		}).detach();
}

void Interface::InitializeMenu()
{
	bIsMenuOpen = true;
	SetWindowLong(hWindow, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED);
	SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	SetForegroundWindow(hWindow);
}

void Interface::UpdateStyle()
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle* Style = &ImGui::GetStyle();

	Style->AntiAliasedLines = true;
	Style->AntiAliasedLinesUseTex = true;
	Style->AntiAliasedFill = true;

	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	Style->WindowRounding = 14.0f;
	Style->WindowBorderSize = 0;
	Style->WindowPadding = ImVec2(0, 0);
	Style->FrameBorderSize = 0;
	Style->FrameRounding = 8.0f;
	Style->WindowShadowSize = 0.0f;
	Style->ScrollbarSize = 5.0f;
	Style->ScrollbarRounding = 3.0f;
	Style->PopupRounding = 10.0f;
	Style->GrabRounding = 6.0f;

	Style->Colors[ImGuiCol_Separator] = ImColor(40, 40, 48, 255);
	Style->Colors[ImGuiCol_SeparatorActive] = ImColor(200, 0, 0, 255);
	Style->Colors[ImGuiCol_SeparatorHovered] = ImColor(200, 0, 0, 150);
	Style->Colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
	Style->Colors[ImGuiCol_ResizeGripActive] = ImColor(0, 0, 0, 0);
	Style->Colors[ImGuiCol_ResizeGripHovered] = ImColor(0, 0, 0, 0);

	Style->Colors[ImGuiCol_ScrollbarBg] = ImColor(0, 0, 0, 0);
	Style->Colors[ImGuiCol_ScrollbarGrab] = ImColor(200, 0, 0, 90);
	Style->Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(200, 0, 0, 255);
	Style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(200, 0, 0, 180);

	Style->Colors[ImGuiCol_WindowBg] = ImColor(12, 12, 12, 255);
	Style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
	Style->Colors[ImGuiCol_Border] = ImColor(45, 45, 45, 255);
	Style->Colors[ImGuiCol_Text] = ImColor(240, 238, 245, 255);
	Style->Colors[ImGuiCol_TextSelectedBg] = ImColor(200, 0, 0, 80);
	Style->Colors[ImGuiCol_PopupBg] = ImColor(18, 18, 18, 250);
	Style->Colors[ImGuiCol_FrameBg] = ImColor(22, 22, 22, 255);
	Style->Colors[ImGuiCol_FrameBgHovered] = ImColor(30, 30, 30, 255);
	Style->Colors[ImGuiCol_FrameBgActive] = ImColor(40, 40, 40, 255);
	Style->Colors[ImGuiCol_Header] = ImColor(200, 0, 0, 60);
	Style->Colors[ImGuiCol_HeaderHovered] = ImColor(200, 0, 0, 100);
	Style->Colors[ImGuiCol_HeaderActive] = ImColor(200, 0, 0, 160);

	Fonts::Initialize();
}

// ═══════════════════════════════════════════════════════════════════════════════
// DOCK - 5 items: Aimbot | Silent | Exploits | ESP | Config
// ═══════════════════════════════════════════════════════════════════════════════

void DrawDock(ImDrawList* dl, ImVec2 windowPos, ImVec2 windowSize, int& currentTab, float alpha)
{
	const float dockHeight = 56.0f;
	const float dockPadding = 12.0f;
	const float itemSize = 44.0f;
	const float itemSpacing = 14.0f;
	const int numItems = 5;

	float dockWidth = (itemSize + itemSpacing) * numItems - itemSpacing + dockPadding * 2;
	float dockX = windowPos.x + (windowSize.x - dockWidth) * 0.5f;
	float dockY = windowPos.y + windowSize.y - dockHeight - 16.0f;

	ImVec2 dockMin = ImVec2(dockX, dockY);
	ImVec2 dockMax = ImVec2(dockX + dockWidth, dockY + dockHeight);

	// Shadow
	dl->AddRectFilled(
		dockMin + ImVec2(0, 4),
		dockMax + ImVec2(0, 4),
		IM_COL32(0, 0, 0, (int)(30 * alpha)),
		18.0f
	);

	// Background
	dl->AddRectFilled(dockMin, dockMax, IM_COL32(18, 18, 18, (int)(245 * alpha)), 18.0f);

	// Border
	dl->AddRect(dockMin, dockMax, IM_COL32(45, 45, 45, (int)(120 * alpha)), 18.0f, 0, 1.0f);

	struct DockItemData
	{
		const char* icon;
		const char* label;
	};

	// [FIX] POD static buffers — sem magic statics do CRT
	static char dock_labels[5][16] = { };
	static bool dock_init = false;
	if (!dock_init)
	{
		strcpy_s(dock_labels[0], XorStr("Aimbot"));
		strcpy_s(dock_labels[1], XorStr("Silent"));
		strcpy_s(dock_labels[2], XorStr("Exploits"));
		strcpy_s(dock_labels[3], XorStr("ESP"));
		strcpy_s(dock_labels[4], XorStr("Config"));
		dock_init = true;
	}

	DockItemData items[] = {
		{ ICON_FA_CROSSHAIRS, dock_labels[0] },
		{ ICON_FA_GHOST, dock_labels[1] },
		{ ICON_FA_BOLT, dock_labels[2] },
		{ ICON_FA_EYE, dock_labels[3] },
		{ ICON_FA_GEAR, dock_labels[4] },
	};

	static float hoverAnims[5] = { 0, 0, 0, 0, 0 };
	static float activeAnims[5] = { 0, 0, 0, 0, 0 };
	static float bounceAnims[5] = { 0, 0, 0, 0, 0 };
	static float bounceVelocity[5] = { 0, 0, 0, 0, 0 };

	ImGuiIO& io = ImGui::GetIO();
	float dt = io.DeltaTime;

	dt = ImClamp(dt, 0.0001f, 0.1f);

	auto smoothLerp = [](float current, float target, float speed, float deltaTime) -> float
		{
			float factor = 1.0f - expf(-speed * deltaTime);
			return current + (target - current) * factor;
		};

	extern ImGuiID activeCombo;
	extern ImGuiID activeColorPicker;
	extern ImRect activeComboPopupRect;

	bool mouseOverComboPopup = false;
	if (activeCombo != 0)
	{
		mouseOverComboPopup = io.MousePos.x >= activeComboPopupRect.Min.x &&
			io.MousePos.x <= activeComboPopupRect.Max.x &&
			io.MousePos.y >= activeComboPopupRect.Min.y &&
			io.MousePos.y <= activeComboPopupRect.Max.y;
	}

	bool blockDockInteraction = ((activeCombo != 0) && mouseOverComboPopup) || (activeColorPicker != 0);

	float startX = dockX + dockPadding;
	float itemY = dockY + (dockHeight - itemSize) * 0.5f;

	for (int i = 0; i < numItems; i++)
	{
		float itemX = startX + i * (itemSize + itemSpacing);
		ImVec2 itemMin = ImVec2(itemX, itemY);
		ImVec2 itemMax = ImVec2(itemX + itemSize, itemY + itemSize);

		bool isHovered = !blockDockInteraction &&
			io.MousePos.x >= itemMin.x && io.MousePos.x <= itemMax.x &&
			io.MousePos.y >= itemMin.y && io.MousePos.y <= itemMax.y;
		bool isActive = (currentTab == i + 1);

		hoverAnims[i] = smoothLerp(hoverAnims[i], isHovered ? 1.0f : 0.0f, 12.0f, dt);
		activeAnims[i] = smoothLerp(activeAnims[i], isActive ? 1.0f : 0.0f, 10.0f, dt);

		if (isHovered && ImGui::IsMouseClicked(0) && !blockDockInteraction)
		{
			currentTab = i + 1;
			bounceVelocity[i] = -140.0f;
		}

		float springK = 800.0f;
		float damping = 12.0f;

		float springForce = -springK * bounceAnims[i];
		float dampingForce = -damping * bounceVelocity[i];
		float acceleration = springForce + dampingForce;

		bounceVelocity[i] += acceleration * dt;
		bounceAnims[i] += bounceVelocity[i] * dt;

		if (fabsf(bounceAnims[i]) < 0.1f && fabsf(bounceVelocity[i]) < 1.0f)
		{
			bounceAnims[i] = 0.0f;
			bounceVelocity[i] = 0.0f;
		}

		float hoverOffset = hoverAnims[i] * 3.0f;
		float bounceOffset = bounceAnims[i];
		float totalOffset = hoverOffset + bounceOffset;

		ImVec2 drawMin = ImVec2(itemMin.x, itemMin.y - totalOffset);
		ImVec2 drawMax = ImVec2(itemMax.x, itemMax.y - totalOffset);
		ImVec2 center = ImVec2((drawMin.x + drawMax.x) * 0.5f, (drawMin.y + drawMax.y) * 0.5f);

		float t = activeAnims[i];
		ImU32 bgColor = IM_COL32(
			(int)(18.0f * (1.0f - t) + 200.0f * t),
			(int)(18.0f * (1.0f - t) + 25.0f * t),
			(int)(18.0f * (1.0f - t) + 25.0f * t),
			(int)(255 * alpha)
		);

		dl->AddRectFilled(drawMin, drawMax, bgColor, 12.0f);

		if (t < 0.5f)
		{
			dl->AddRect(drawMin, drawMax, IM_COL32(45 + (int)(20 * hoverAnims[i]), 45 + (int)(20 * hoverAnims[i]), 45 + (int)(20 * hoverAnims[i]), (int)(80 * alpha)), 12.0f, 0, 1.0f);
		}

		ImGui::PushFont(Fonts::FontAwesomeSolid);
		ImVec2 iconSize = ImGui::CalcTextSize(items[i].icon);
		ImVec2 iconPos = ImVec2(center.x - iconSize.x * 0.5f, center.y - iconSize.y * 0.5f);

		ImU32 iconColor = IM_COL32(
			(int)(140 + 100 * t + 40 * hoverAnims[i] * (1 - t)),
			(int)(140 + 100 * t + 35 * hoverAnims[i] * (1 - t)),
			(int)(140 + 100 * t + 30 * hoverAnims[i] * (1 - t)),
			(int)(255 * alpha)
		);

		dl->AddText(iconPos, iconColor, items[i].icon);
		ImGui::PopFont();

		if (isHovered)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
			ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.15f, 0.95f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.30f, 1.0f));
			ImGui::BeginTooltip();
			ImGui::PushFont(Fonts::InterMedium);
			ImGui::TextUnformatted(items[i].label);
			ImGui::PopFont();
			ImGui::EndTooltip();
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(2);
		}

		if (activeAnims[i] > 0.1f)
		{
			float dotAlpha = activeAnims[i] * alpha;
			ImVec2 dotPos = ImVec2(center.x, drawMax.y + 6.0f);
			dl->AddCircleFilled(dotPos, 2.5f, IM_COL32(200, 0, 0, (int)(255 * dotAlpha)), 12);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN RENDER
// ═══════════════════════════════════════════════════════════════════════════════

static bool g_WantShutdown = false;
static float g_ShutdownProgress = 0.0f;
static float g_ShutdownScale = 1.0f;
static float g_ShutdownAlpha = 1.0f;
static float g_ShutdownRotation = 0.0f;

void Interface::RenderGui()
{
	ImGuiIO& io = ImGui::GetIO();
	float dt = io.DeltaTime;

	dt = ImClamp(dt, 0.0001f, 0.1f);

	auto smoothLerp = [](float current, float target, float speed, float deltaTime) -> float
		{
			float factor = 1.0f - expf(-speed * deltaTime);
			return current + (target - current) * factor;
		};

	if (g_WantShutdown)
	{
		g_ShutdownProgress += dt * 3.5f;
		float t = g_ShutdownProgress;
		float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
		g_ShutdownScale = 1.0f - easeOut * 0.3f;
		g_ShutdownAlpha = 1.0f - easeOut;
		g_ShutdownRotation = easeOut * 3.0f;
		if (g_ShutdownProgress >= 1.0f)
		{
			g_Globals.General.ShutDown = true;
			return;
		}
	}

	float targetScale = bIsMenuOpen ? 1.0f : 0.95f;
	float targetAlpha = bIsMenuOpen ? 1.0f : 0.0f;

	float scaleSpeed = bIsMenuOpen ? 12.0f : 25.0f;
	float alphaSpeed = bIsMenuOpen ? 10.0f : 30.0f;

	g_WindowScale = smoothLerp(g_WindowScale, targetScale, scaleSpeed, dt);
	g_WindowAlpha = smoothLerp(g_WindowAlpha, targetAlpha, alphaSpeed, dt);
	g_ContentAlpha = smoothLerp(g_ContentAlpha, targetAlpha, alphaSpeed, dt);

	if (g_WantShutdown)
	{
		g_WindowScale *= g_ShutdownScale;
		g_WindowAlpha *= g_ShutdownAlpha;
		g_ContentAlpha *= g_ShutdownAlpha;
	}

	if (g_WindowAlpha < 0.01f && !g_WantShutdown) return;

	static float AnimaTab = 0.0f;
	static float AnimaTabVelocity = 0.0f;
	static int LastCurrentTab = 0;

	if (LastCurrentTab != CurrentTab)
	{
		AnimaTab = (LastCurrentTab > CurrentTab) ? -30.f : 30.f;
		AnimaTabVelocity = (LastCurrentTab > CurrentTab) ? -200.f : 200.f;
		LastCurrentTab = CurrentTab;
	}

	float springStiffness = 180.0f;
	float springDamping = 18.0f;
	float springForce = -springStiffness * AnimaTab;
	float dampingForce = -springDamping * AnimaTabVelocity;
	AnimaTabVelocity += (springForce + dampingForce) * dt;
	AnimaTab += AnimaTabVelocity * dt;

	if (fabsf(AnimaTab) < 0.1f && fabsf(AnimaTabVelocity) < 1.0f)
	{
		AnimaTab = 0.0f;
		AnimaTabVelocity = 0.0f;
	}

	static int CurrentSub = 0;
	static int LastCurrentSub = 0;
	static float SubAnima = 0.f;
	static float SubAnimaVelocity = 0.f;

	if (LastCurrentSub != CurrentSub)
	{
		SubAnima = (LastCurrentSub > CurrentSub) ? -30.f : 30.f;
		SubAnimaVelocity = (LastCurrentSub > CurrentSub) ? -200.f : 200.f;
		LastCurrentSub = CurrentSub;
	}

	float subSpringForce = -springStiffness * SubAnima;
	float subDampingForce = -springDamping * SubAnimaVelocity;
	SubAnimaVelocity += (subSpringForce + subDampingForce) * dt;
	SubAnima += SubAnimaVelocity * dt;

	if (fabsf(SubAnima) < 0.1f && fabsf(SubAnimaVelocity) < 1.0f)
	{
		SubAnima = 0.0f;
		SubAnimaVelocity = 0.0f;
	}

	ImVec2 windowSize = ImVec2(700, 460);
	ImVec2 displaySize = io.DisplaySize;

	static bool g_WindowPosInitialized = false;
	if (!g_WindowPosInitialized)
	{
		g_WindowPos = ImVec2(
			(displaySize.x - windowSize.x) * 0.5f,
			(displaySize.y - windowSize.y) * 0.5f
		);
		g_WindowPosInitialized = true;
	}

	g_WindowPos.x = ImClamp(g_WindowPos.x, -windowSize.x + 100.0f, displaySize.x - 100.0f);
	g_WindowPos.y = ImClamp(g_WindowPos.y, 0.0f, displaySize.y - 50.0f);

	ImVec2 scaledSize = windowSize * g_WindowScale;
	ImVec2 scaledPos = g_WindowPos + (windowSize - scaledSize) * 0.5f;

	if (g_WantShutdown)
	{
		float moveUp = g_ShutdownProgress * 30.0f;
		scaledPos.y -= moveUp;
	}

	ImGui::SetNextWindowPos(scaledPos);
	ImGui::SetNextWindowSize(scaledSize);
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_WindowAlpha);
	ImGui::Begin(XorStr("Storm Cheats"), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		ImVec2 Pos = ImGui::GetWindowPos();
		ImVec2 Size = ImGui::GetWindowSize();

		// ═══════════════════════════════════════════════════════════════════════════
		// WINDOW DRAGGING
		// ═══════════════════════════════════════════════════════════════════════════

		static ImGuiID lastFrameHoveredId = 0;
		ImGuiContext& gc = *GImGui;

		bool scrollbarActive = false;
		if (gc.ActiveId != 0)
		{
			ImGuiWindow* activeWindow = gc.ActiveIdWindow;
			if (activeWindow)
			{
				ImGuiID scrollYId = ImGui::GetWindowScrollbarID(activeWindow, ImGuiAxis_Y);
				ImGuiID scrollXId = ImGui::GetWindowScrollbarID(activeWindow, ImGuiAxis_X);
				if (gc.ActiveId == scrollYId || gc.ActiveId == scrollXId)
				{
					scrollbarActive = true;
				}
			}
		}

		bool canStartDrag = false;

		if (CurrentTab == 0)
		{
			float headerHeight = 60.0f;
			bool inHeader = io.MousePos.y >= Pos.y && io.MousePos.y <= Pos.y + headerHeight &&
				io.MousePos.x >= Pos.x && io.MousePos.x <= Pos.x + Size.x;

			float formLeft = Pos.x + (Size.x - 320) * 0.5f - 20;
			float formRight = formLeft + 360;
			float formTop = Pos.y + 130;
			float formBottom = Pos.y + 340;

			bool inForm = io.MousePos.x >= formLeft && io.MousePos.x <= formRight &&
				io.MousePos.y >= formTop && io.MousePos.y <= formBottom;

			bool inWindow = io.MousePos.x >= Pos.x && io.MousePos.x <= Pos.x + Size.x &&
				io.MousePos.y >= Pos.y && io.MousePos.y <= Pos.y + Size.y;

			canStartDrag = inWindow && (inHeader || !inForm);
		}
		else
		{
			float headerHeight = 55.0f;
			bool inHeader = io.MousePos.y >= Pos.y && io.MousePos.y <= Pos.y + headerHeight &&
				io.MousePos.x >= Pos.x && io.MousePos.x <= Pos.x + Size.x;

			const float dockHeight = 56.0f;
			const float dockPadding = 12.0f;
			const float itemSz = 44.0f;
			const float itemSpacing = 14.0f;
			const int numItems = 5;

			float dockWidth = (itemSz + itemSpacing) * numItems - itemSpacing + dockPadding * 2;
			float dockX = Pos.x + (Size.x - dockWidth) * 0.5f;
			float dockY = Pos.y + Size.y - dockHeight - 16.0f;

			bool inDockArea = io.MousePos.y >= dockY - 10 && io.MousePos.y <= Pos.y + Size.y;
			bool inDockButtons = false;

			if (inDockArea)
			{
				float startX = dockX + dockPadding;
				float itmY = dockY + (dockHeight - itemSz) * 0.5f;

				for (int i = 0; i < numItems; i++)
				{
					float itemX = startX + i * (itemSz + itemSpacing);
					if (io.MousePos.x >= itemX && io.MousePos.x <= itemX + itemSz &&
						io.MousePos.y >= itmY && io.MousePos.y <= itmY + itemSz)
					{
						inDockButtons = true;
						break;
					}
				}
			}

			bool inDockBackground = inDockArea && !inDockButtons;

			float contentTop = Pos.y + headerHeight;
			float contentBottom = dockY - 10;
			bool inContentArea = io.MousePos.y >= contentTop && io.MousePos.y <= contentBottom &&
				io.MousePos.x >= Pos.x && io.MousePos.x <= Pos.x + Size.x;

			bool inEmptyContentArea = inContentArea && lastFrameHoveredId == 0 && !scrollbarActive;

			canStartDrag = inHeader || inDockBackground || inEmptyContentArea;
		}

		extern ImGuiID activeSlider;
		extern ImGuiID activeColorPicker;

		if (scrollbarActive)
		{
			canStartDrag = false;
		}

		if (canStartDrag && ImGui::IsMouseClicked(0) && !g_Dragging && activeSlider == 0 && activeColorPicker == 0)
		{
			g_Dragging = true;
			g_DragOffset = ImVec2(io.MousePos.x - g_WindowPos.x, io.MousePos.y - g_WindowPos.y);
		}

		if (CustomDrag::WantDrag && !g_Dragging && activeSlider == 0 && activeColorPicker == 0 && !scrollbarActive)
		{
			g_Dragging = true;
			g_DragOffset = ImVec2(CustomDrag::DragClickPos.x - g_WindowPos.x, CustomDrag::DragClickPos.y - g_WindowPos.y);
		}
		CustomDrag::WantDrag = false;

		if (g_Dragging && (activeSlider != 0 || activeColorPicker != 0 || scrollbarActive))
		{
			g_Dragging = false;
		}

		if (g_Dragging)
		{
			if (ImGui::IsMouseDown(0))
			{
				g_WindowPos = ImVec2(io.MousePos.x - g_DragOffset.x, io.MousePos.y - g_DragOffset.y);
			}
			else
			{
				g_Dragging = false;
			}
		}

		lastFrameHoveredId = gc.HoveredId;

		DrawList->AddRect(Pos, Pos + Size, IM_COL32(50, 50, 58, (int)(150 * g_WindowAlpha)), 14.0f, 0, 1.0f);

		if (CurrentTab == 0)
		{
			// ═══════════════════════════════════════════════════════════════
			// LOGIN SCREEN
			// ═══════════════════════════════════════════════════════════════

			float centerX = (Size.x - 320) * 0.5f;
			float centerY = 200;

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_WindowAlpha);
			ImGui::SetCursorPos(ImVec2(0, 15));
			Custom::TabHeader(XorStr("LoginHeader"), &CurrentSub, { XorStr("Login") }, CurrentTab);
			ImGui::PopStyleVar();

			ImVec2 logoSize = ImVec2(150, 150);
			ImVec2 logoPos = ImVec2(
				Pos.x + (Size.x - logoSize.x) * 0.5f,
				Pos.y + 75
			);

			DrawList->AddImage((ImTextureID)(intptr_t)Fonts::LogoTexture,
				logoPos,
				logoPos + logoSize,
				ImVec2(0, 0), ImVec2(1, 1),
				IM_COL32(255, 255, 255, (int)(255 * g_WindowAlpha))
			);

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_WindowAlpha);

			{
				// --- License Key field ---
				ImGui::SetCursorPos({ centerX, centerY });
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 12));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.13f, 0.13f, 0.16f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
				ImGui::InputTextEx(XorStr("##LicenseKey"), XorStr("License Key"), g_LicenseKey, IM_ARRAYSIZE(g_LicenseKey), ImVec2(320, 44), 0);
				ImGui::PopStyleColor(3);
				ImGui::PopStyleVar(2);

				// --- Sign In button ---
				ImGui::SetCursorPos({ centerX, centerY + 54 });
				static bool isLoggingIn = false;
				if (Custom::Button(XorStr("Sign In"), ImVec2(320, 46)) && !isLoggingIn)
				{
					if (g_LicenseKey[0] == '\0')
					{
						NotifyManager::Send(XorStr("Digite sua license key"), 3000);
					}
					else
					{
						isLoggingIn = true;
						std::thread([&]()
							{
								isLoading = true;
								strcpy_s(loadingMessage, sizeof(loadingMessage), XorStr("Autenticando..."));

								if (!g_KeyAuth.initalized)
									g_KeyAuth.init();

								bool authed = false;
								if (g_KeyAuth.initalized)
								{
									authed = g_KeyAuth.license(g_LicenseKey);
								}
								else
								{
									g_KeyAuth.message = "Falha ao conectar com o servidor de auth";
								}

								if (!authed)
								{
									isLoading = false;
									isLoggingIn = false;
									NotifyManager::Send(g_KeyAuth.message.c_str(), 5000);
									return;
								}

								strcpy_s(loadingMessage, sizeof(loadingMessage), XorStr("Iniciando..."));
								std::this_thread::sleep_for(std::chrono::milliseconds(500));
								g_FreeFireMemory.Initialize();
								CurrentTab = 1;
								g_Globals.General.EnableFuncs = true;
								// Conecta no Discord local para obter nome + avatar do usuario
								DiscordRPC::Tick(true);
								isLoading = false;
								std::string welcome = XorStr("Bem-vindo(a), ") + g_KeyAuth.username;
								NotifyManager::Send(welcome.c_str(), 4000);
								isLoggingIn = false;
							}).detach();
					}
				}
			}

			ImGui::PopStyleVar();

			// Loading overlay
			if (isLoading)
			{
				DrawList->AddRectFilled(Pos, Pos + Size, IM_COL32(15, 15, 15, 220), 14.0f);

				ImVec2 windowCenter = Size * 0.5f;
				ImGui::SetCursorPos(ImVec2(windowCenter.x - 15, windowCenter.y - 30));
				ImSpinner::SpinnerWaveDots(XorStr("##Loading"), 10.0f, 2.0f, IM_COL32(200, 0, 0, 220));
				if (loadingMessage[0] != '\0')
				{
					ImGui::PushFont(Fonts::InterMedium);
					ImVec2 textSize = ImGui::CalcTextSize(loadingMessage);
					ImGui::SetCursorPos(ImVec2(windowCenter.x - textSize.x * 0.5f, windowCenter.y + 30));
					ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "%s", loadingMessage);
					ImGui::PopFont();
				}
			}
		}
		else
		{
			// ═══════════════════════════════════════════════════════════════
			// MAIN INTERFACE
			// ═══════════════════════════════════════════════════════════════

			float headerHeight = 55.0f;
			float dockSpace = 90.0f;

			DrawList->AddRectFilled(
				Pos,
				Pos + ImVec2(Size.x, headerHeight),
				IM_COL32(18, 18, 18, (int)(255 * g_ContentAlpha)),
				14.0f,
				ImDrawFlags_RoundCornersTop
			);

			DrawList->AddLine(
				Pos + ImVec2(0, headerHeight),
				Pos + ImVec2(Size.x, headerHeight),
				IM_COL32(40, 40, 40, (int)(180 * g_ContentAlpha))
			);

			float logoScale = 0.40f;
			ImVec2 logoSize(100.0f * logoScale, 100.0f * logoScale);
			ImVec2 logoPos(Pos.x + 16, Pos.y + (headerHeight - logoSize.y) * 0.5f);

			DrawList->AddImage((ImTextureID)(intptr_t)Fonts::LogoTexture,
				logoPos,
				logoPos + logoSize,
				ImVec2(0, 0), ImVec2(1, 1),
				IM_COL32(255, 255, 255, (int)(255 * g_ContentAlpha))
			);

			// [FIX] POD static buffers — sem magic statics do CRT
			// Tab order: 1=Aimbot, 2=Silent, 3=Exploits, 4=ESP, 5=Config
			static char tab_names[6][16] = { };
			static bool tabs_init = false;
			if (!tabs_init)
			{
				tab_names[0][0] = '\0';
				strcpy_s(tab_names[1], XorStr("Aimbot"));
				strcpy_s(tab_names[2], XorStr("Silent"));
				strcpy_s(tab_names[3], XorStr("Exploits"));
				strcpy_s(tab_names[4], XorStr("ESP"));
				strcpy_s(tab_names[5], XorStr("Config"));
				tabs_init = true;
			}

			const char* tabNames[] = {
				tab_names[0], tab_names[1], tab_names[2],
				tab_names[3], tab_names[4], tab_names[5]
			};

			if (CurrentTab >= 1 && CurrentTab <= 5)
			{
				ImGui::PushFont(Fonts::InterBold);
				ImVec2 titleSize = ImGui::CalcTextSize(tabNames[CurrentTab]);
				ImVec2 titlePos = ImVec2(Pos.x + (Size.x - titleSize.x) * 0.5f, Pos.y + (headerHeight - titleSize.y) * 0.5f);
				DrawList->AddText(titlePos, IM_COL32(220, 220, 225, (int)(255 * g_ContentAlpha)), tabNames[CurrentTab]);
				ImGui::PopFont();
			}

			// Profile section
			{
				std::string username = g_KeyAuth.username.empty() ? XorStr("User") : g_KeyAuth.username;
				std::string expiry = g_KeyAuth.username.empty() ? XorStr("Lifetime") : g_KeyAuth.getExpiry();

				// Preferir o nome do Discord (vem via RPC apos o login).
				// Fallback para a username do KeyAuth se o Discord nao estiver aberto.
				std::string discordName;
				if (DiscordRPC::GetUsername(discordName) && !discordName.empty())
					username = discordName;

				float rightPadding = 18.0f;
				float avatarSize = 36.0f;

				ImVec2 avatarPos = ImVec2(
					Pos.x + Size.x - rightPadding - avatarSize,
					Pos.y + (headerHeight - avatarSize) * 0.5f
				);

				ImVec2 avatarCenter = avatarPos + ImVec2(avatarSize * 0.5f, avatarSize * 0.5f);

				// Upload da foto do Discord (RGBA -> GL texture) feito 1x quando
				// os pixels chegam. O contexto GL do ImGui esta ativo aqui.
				static GLuint s_AvatarTex = 0;
				static bool s_AvatarUploaded = false;
				if (!s_AvatarUploaded)
				{
					DiscordRPC::AvatarData av;
					if (DiscordRPC::GetAvatarData(av))
					{
						if (Fonts::LoadTextureFromRawRGBA(av.pixels, av.width, av.height, &s_AvatarTex, true))
							s_AvatarUploaded = true;
					}
				}

				if (s_AvatarUploaded && s_AvatarTex != 0)
				{
					DrawList->AddImageRounded(
						(ImTextureID)(intptr_t)s_AvatarTex,
						avatarPos,
						avatarPos + ImVec2(avatarSize, avatarSize),
						ImVec2(0, 0), ImVec2(1, 1),
						IM_COL32(255, 255, 255, (int)(255 * g_ContentAlpha)),
						avatarSize * 0.5f,
						ImDrawFlags_RoundCornersAll
					);
					DrawList->AddCircle(avatarCenter, avatarSize * 0.5f + 1.5f, IM_COL32(200, 0, 0, (int)(220 * g_ContentAlpha)), 24, 2.0f);
				}
				else
				{
					DrawList->AddCircleFilled(avatarCenter, avatarSize * 0.5f, IM_COL32(200, 0, 0, (int)(200 * g_ContentAlpha)), 24);
					char avatarLetter[2] = { (char)toupper(username[0]), 0 };
					ImGui::PushFont(Fonts::InterBold);
					ImVec2 letterSize = ImGui::CalcTextSize(avatarLetter);
					DrawList->AddText(
						avatarPos + ImVec2((avatarSize - letterSize.x) * 0.5f, (avatarSize - letterSize.y) * 0.5f),
						IM_COL32(255, 255, 255, (int)(255 * g_ContentAlpha)),
						avatarLetter
					);
					ImGui::PopFont();
				}

				ImGui::PushFont(Fonts::InterBold);
				ImVec2 nameSize = ImGui::CalcTextSize(username.c_str());
				ImGui::PopFont();

				ImGui::PushFont(Fonts::InterMedium);
				ImVec2 expirySize = ImGui::CalcTextSize(expiry.c_str());
				ImGui::PopFont();

				float textRightEdge = avatarPos.x - 10;

				ImGui::PushFont(Fonts::InterBold);
				DrawList->AddText(
					ImVec2(textRightEdge - nameSize.x, Pos.y + headerHeight * 0.5f - nameSize.y - 1),
					IM_COL32(230, 230, 235, (int)(255 * g_ContentAlpha)),
					username.c_str()
				);
				ImGui::PopFont();

				ImGui::PushFont(Fonts::InterMedium);
				DrawList->AddText(
					ImVec2(textRightEdge - expirySize.x, Pos.y + headerHeight * 0.5f + 2),
					IM_COL32(120, 120, 130, (int)(255 * g_ContentAlpha)),
					expiry.c_str()
				);
				ImGui::PopFont();
			}

			DrawDock(DrawList, Pos, Size, CurrentTab, g_ContentAlpha);

			float contentTop = headerHeight + 10;
			float contentHeight = Size.y - headerHeight - dockSpace - 5;

			ImGui::SetCursorPos(ImVec2(14, contentTop));
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_ContentAlpha);
			ImGui::BeginChild(XorStr("ContentArea"), ImVec2(Size.x - 28, contentHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				float cardWidth = (ImGui::GetWindowSize().x - 10) * 0.5f;
				float cardHeight = contentHeight - 8;

				// ═══════════════════════════════════════════════════════════
				// TAB 1: AIMBOT
				// ═══════════════════════════════════════════════════════════
				if (CurrentTab == 1)
				{
					ImGui::SetCursorPos(ImVec2(AnimaTab, 0));
					ImGui::BeginGroup();
					{
						Custom::CustomChild(XorStr("General"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Aimbot"), &g_Globals.AimBot.Enabled);
							Custom::KeyBind(XorStr("AimKey"), &g_Globals.AimBot.KeyBind);
							if (!g_Globals.General.V31)
							{
								Custom::Checkbox(XorStr("Pull Player"), &g_Globals.AimBot.aimmagnect);
								Custom::KeyBind(XorStr("PullKey"), &g_Globals.AimBot.MagKey);
							}
							Custom::Checkbox(XorStr("Aimbot 2x"), &g_Globals.Misc.Exploits.LocalPlayer.AimLock2x);
							Custom::Checkbox(XorStr("Aimbot Sniper"), &g_Globals.Misc.Exploits.LocalPlayer.AimbotAwm);
							Custom::Checkbox(XorStr("No Recoil"), &g_Globals.Misc.Exploits.LocalPlayer.NoRecoil);
							if (g_Globals.Misc.Exploits.LocalPlayer.NoRecoil)
								Custom::SliderInt(XorStr("Recoil Control"), &g_Globals.Misc.Exploits.LocalPlayer.RecoilControl, 0, 100, "%d%%");
						}
						Custom::EndCustomChild();

						ImGui::SetCursorPos(ImVec2(cardWidth + 10 + AnimaTab, 0));
						Custom::CustomChild(XorStr("Config"), ImVec2(cardWidth, cardHeight));
						{
							if (!g_Globals.General.NoAnogs && g_Globals.AimBot.aimtype == 1)
								g_Globals.AimBot.aimtype = 0;

							Custom::Combo(XorStr("Aimbot Type"), &g_Globals.AimBot.aimtype,
								g_Globals.General.NoAnogs ? XorStr("Safe\0Rage\0") : XorStr("Safe\0"));

							if (g_Globals.AimBot.aimtype == 0)
							{
								Custom::Combo(XorStr("Target Bone"), &g_Globals.AimBot.Target, XorStr("Neck\0Legit\0"));
							}

							if (g_Globals.General.NoAnogs && g_Globals.AimBot.aimtype == 1)
							{
								Custom::Combo(XorStr("Aimbot Delay"), &g_Globals.AimBot.PeitosIndex, XorStr("Peito 0\0Peito 1\0Peito 2\0Peito 3\0Peito 4\0Random\0"));
								Custom::Checkbox(XorStr("Visible Check"), &g_Globals.AimBot.VisibleCheck);
								if (g_Globals.AimBot.IgnoreKnocked)
								{
									Custom::Checkbox(XorStr("Puxar Cima"), &g_Globals.AimBot.PraCima);
									if (g_Globals.AimBot.PraCima)
									{
										Custom::SliderFloat(XorStr("Altura"), &g_Globals.AimBot.PraCimaValor, 0.1f, 1.0f, "%.2f");
										Custom::SliderInt(XorStr("Tempo"), &g_Globals.AimBot.PraCimaTempo, 10, 200, "%d ms");
									}
								}
							}

							Custom::Checkbox(XorStr("Ignore Bots"), &g_Globals.AimBot.IgnoreBots);
							Custom::Checkbox(XorStr("Ignore Knocked"), &g_Globals.AimBot.IgnoreKnocked);
							Custom::SliderInt(XorStr("Max Distance"), &g_Globals.AimBot.MaxDistance, 0, 200, "%d m");
							Custom::Checkbox(XorStr("Show Fov"), &g_Globals.Misc.Screen.ShowAimbotFov);
							Custom::SliderInt(XorStr("Field of View"), &g_Globals.AimBot.Fov, 0, 360, "%d");

							if (g_Globals.Misc.Screen.ShowAimbotFov)
							{
								Custom::ColorEdit4(XorStr("Fov Color"), g_Globals.Misc.Screen.AimbotFovColor);
								Custom::ColorEdit4(XorStr("Fov Filled Color"), g_Globals.Misc.Screen.FilledFovColor);
							}
						}
						Custom::EndCustomChild();
					}
					ImGui::EndGroup();
				}
				// ═══════════════════════════════════════════════════════════
				// TAB 2: SILENT
				// ═══════════════════════════════════════════════════════════
				else if (CurrentTab == 2)
				{
					ImGui::SetCursorPos(ImVec2(AnimaTab, 0));
					ImGui::BeginGroup();
					{
						Custom::CustomChild(XorStr("Silent Aim"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Enable Silent"), &g_Globals.Silent.Enabled);
							Custom::KeyBind(XorStr("SilentKey"), &g_Globals.Silent.KeyBind);

						}
						Custom::EndCustomChild();
						ImGui::SetCursorPos(ImVec2(cardWidth + 10 + AnimaTab, 0));

						Custom::CustomChild(XorStr("Config"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Show Fov"), &g_Globals.Misc.Screen.ShowSilentFov);

							if (g_Globals.Misc.Screen.ShowSilentFov)
							{
								Custom::ColorEdit4(XorStr("Fov Color"), g_Globals.Misc.Screen.SilentFovColor);
								Custom::ColorEdit4(XorStr("Fov Filled Color"), g_Globals.Misc.Screen.SilentFilledFovColor);
							}
							Custom::SliderInt(XorStr("Silent FOV"), &g_Globals.Silent.Fov, 0, 360, "%d");

							Custom::SliderInt(XorStr("Silent Distance"), &g_Globals.Silent.MaxDistance, 0, 200, "%d m");
						}
					}
					ImGui::EndGroup();
				}
				// ═══════════════════════════════════════════════════════════
				// TAB 3: EXPLOITS
				// ═══════════════════════════════════════════════════════════
				else if (CurrentTab == 3)
				{
					ImGui::SetCursorPos(ImVec2(AnimaTab, 0));
					ImGui::BeginGroup();
					{
						Custom::CustomChild(XorStr("Player Exploits"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Fast MedKit"), &g_Globals.Misc.Exploits.LocalPlayer.FastMedkit);
							Custom::Checkbox(XorStr("Semi Tela Parada"), &g_Globals.Misc.Exploits.LocalPlayer.telaparada);
							Custom::Checkbox(XorStr("AimLock"), &g_Globals.Misc.Exploits.LocalPlayer.Aimlock);
							Custom::Checkbox(XorStr("Max Damage"), &g_Globals.Misc.Exploits.LocalPlayer.MoreDamage);
							Custom::Checkbox(XorStr("No Fire Delay"), &g_Globals.Misc.Exploits.LocalPlayer.FireDelay);
							Custom::Checkbox(XorStr("Bugar Pixel"), &g_Globals.Misc.Exploits.LocalPlayer.BugarPixel);
							Custom::Checkbox(XorStr("Precision"), &g_Globals.Misc.Exploits.LocalPlayer.Precision);
							Custom::Checkbox(XorStr("Back Jump"), &g_Globals.Misc.Exploits.LocalPlayer.BackJump);
							Custom::Checkbox(XorStr("Soco Longe"), &g_Globals.Misc.Exploits.LocalPlayer.SocoLonge);
							Custom::Checkbox(XorStr("Atributar Armas"), &g_Globals.Misc.Exploits.LocalPlayer.AtributarArma);
							if (g_Globals.Misc.Exploits.LocalPlayer.AtributarArma)
							{
								Custom::Combo(XorStr("Level"), &g_Globals.Misc.Exploits.LocalPlayer.AtributarArmaLevel, XorStr("Lv 1\0Lv 2\0Lv 3\0Lv 4\0"));
							}
							if (!g_Globals.General.V31)
							{
								Custom::Checkbox(XorStr("Spin Bot"), &g_Globals.Misc.Exploits.LocalPlayer.SpinBot);
								if (g_Globals.Misc.Exploits.LocalPlayer.SpinBot)
								{
									Custom::SliderFloat(XorStr("Spin Speed"), &g_Globals.Misc.Exploits.LocalPlayer.SpinSpeed, 1.0f, 5.0f, "%.1f");
								}
							}
							Custom::Checkbox(XorStr("Ghost"), &g_Globals.AimBot.ghost);
							Custom::KeyBind(XorStr("GhostKey"), &g_Globals.AimBot.ghostkey);
						}
						Custom::EndCustomChild();

						ImGui::SetCursorPos(ImVec2(cardWidth + 10 + AnimaTab, 0));
						Custom::CustomChild(XorStr("World Exploits"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Enable Chams"), &g_Globals.Visuals.Chams.Enabled);
							Custom::Checkbox(XorStr("Aggressive Mode"), &g_Globals.Visuals.Chams.AggressiveMode);
							Custom::ColorEdit4(XorStr("Visible Color"), g_Globals.Visuals.Chams.NearColor);
							Custom::ColorEdit4(XorStr("No Visible Color"), g_Globals.Visuals.Chams.FarColor);
						}
						Custom::EndCustomChild();
					}
					ImGui::EndGroup();
				}
				// ═══════════════════════════════════════════════════════════
				// TAB 4: ESP
				// ═══════════════════════════════════════════════════════════
				else if (CurrentTab == 4)
				{
					ImGui::SetCursorPos(ImVec2(AnimaTab, 0));
					ImGui::BeginGroup();
					{
						Custom::CustomChild(XorStr("General"), ImVec2(cardWidth, cardHeight));
						{
							// Master ESP: so controla o desenho dos players.
							// Aimbot/silent continuam funcionando com ele desligado.
							Custom::Checkbox(XorStr("Player"), &g_Globals.Visuals.ESP.Enabled);
							Custom::Checkbox(XorStr("Watermark"), &g_Globals.Visuals.ESP.Watermark);
							Custom::Checkbox(XorStr("Enemies"), &g_Globals.Visuals.ESP.Enemy);
							Custom::Checkbox(XorStr("Team"), &g_Globals.Visuals.ESP.ShowTeam);
							Custom::Checkbox(XorStr("Weapons"), &g_Globals.Visuals.ESP.Weapon);
							if (g_Globals.Visuals.ESP.Weapon && g_Globals.Visuals.ESP.WeaponStyle == 0)
								g_Globals.Visuals.ESP.WeaponStyle = 1;
							if (g_Globals.Visuals.ESP.Weapon)
							{
								Custom::Combo(XorStr("Style##w"), &g_Globals.Visuals.ESP.WeaponStyle, XorStr("None\0Text\0Icon\0Both\0"));
							}
							Custom::Checkbox(XorStr("SnapLines"), &g_Globals.Visuals.ESP.SnapLines);
							if (g_Globals.Visuals.ESP.SnapLines && g_Globals.Visuals.ESP.SnapLinesPos == 0)
								g_Globals.Visuals.ESP.SnapLinesPos = 1;
							if (g_Globals.Visuals.ESP.SnapLines)
							{
								Custom::Combo(XorStr("Position##s"), &g_Globals.Visuals.ESP.SnapLinesPos, XorStr("None\0Top\0Bottom\0"));
							}
							Custom::Checkbox(XorStr("HealthBar"), &g_Globals.Visuals.ESP.HealthBar);
							if (g_Globals.Visuals.ESP.HealthBar && g_Globals.Visuals.ESP.HealthBarStyle == 0)
								g_Globals.Visuals.ESP.HealthBarStyle = 1;
							if (g_Globals.Visuals.ESP.HealthBar)
							{
								Custom::Combo(XorStr("Style##h"), &g_Globals.Visuals.ESP.HealthBarStyle, XorStr("None\0Left\0Right\0Top\0Bottom\0Text\0"));
							}
							Custom::Checkbox(XorStr("Box"), &g_Globals.Visuals.ESP.Box);
							if (g_Globals.Visuals.ESP.Box && g_Globals.Visuals.ESP.BoxStyle == 0)
								g_Globals.Visuals.ESP.BoxStyle = 1;
							if (g_Globals.Visuals.ESP.Box)
							{
								Custom::Combo(XorStr("Box Style##b"), &g_Globals.Visuals.ESP.BoxStyle, XorStr("None\0Full\0Cornered\0Filled\0"));
								Custom::Checkbox(XorStr("Box Filled"), &g_Globals.Visuals.ESP.BoxFilled);
							}
							Custom::Checkbox(XorStr("Name"), &g_Globals.Visuals.ESP.ShowName);
							Custom::Checkbox(XorStr("Distance"), &g_Globals.Visuals.ESP.Distance);
							Custom::Checkbox(XorStr("Skeleton"), &g_Globals.Visuals.ESP.Skeleton);
						}
						Custom::EndCustomChild();

						ImGui::SetCursorPos(ImVec2(cardWidth + 10 + AnimaTab, 0));
						Custom::CustomChild(XorStr("Colors"), ImVec2(cardWidth, cardHeight));
						{
							Custom::SliderInt(XorStr("Render Distance"), &g_Globals.Visuals.ESP.RenderDistance, 0, 240, "%dm");
							Custom::SliderFloat(XorStr("Text Size"), &g_Globals.Visuals.ESP.TextSize, 10.0f, 20.0f, "%.1f");
							Custom::SliderFloat(XorStr("Thickness"), &g_Globals.Visuals.ESP.Thickness, 0.1f, 3.0f, "%.1f");
							Custom::ColorEdit4(XorStr("Watermark"), g_Globals.Visuals.ESP.WatermarkColor);
							Custom::ColorEdit4(XorStr("Enemy"), g_Globals.Visuals.ESP.EnemyColor);
							Custom::ColorEdit4(XorStr("Team"), g_Globals.Visuals.ESP.TeamColor);
							Custom::ColorEdit4(XorStr("Weapons"), g_Globals.Visuals.ESP.WeaponColor);
							Custom::ColorEdit4(XorStr("SnapLines"), g_Globals.Visuals.ESP.SnapLinesColor);
							Custom::ColorEdit4(XorStr("Box"), g_Globals.Visuals.ESP.BoxColor);
							Custom::ColorEdit4(XorStr("Box Filled"), g_Globals.Visuals.ESP.FilledBoxColor);
							Custom::ColorEdit4(XorStr("Name"), g_Globals.Visuals.ESP.NameColor);
							Custom::ColorEdit4(XorStr("Distance"), g_Globals.Visuals.ESP.DistanceColor);
							Custom::ColorEdit4(XorStr("Skeleton"), g_Globals.Visuals.ESP.SkeletonColor);
						}
						Custom::EndCustomChild();
					}
					ImGui::EndGroup();
				}
				// ═══════════════════════════════════════════════════════════
				// TAB 5: CONFIG
				// ═══════════════════════════════════════════════════════════
				else if (CurrentTab == 5)
				{
					ImGui::SetCursorPos(ImVec2(AnimaTab, 0));
					ImGui::BeginGroup();
					{
						Custom::CustomChild(XorStr("General"), ImVec2(cardWidth, cardHeight));
						{
							Custom::Checkbox(XorStr("Stream Mode"), &g_Globals.General.CaptureBypass);
							Custom::SliderInt(XorStr("Frame Rate"), &g_Globals.General.ThreadDelay, 30, 240, "%dFPS");

							ImGui::Dummy(ImVec2(0, 8));

							Custom::Checkbox(XorStr("Web Remote"), &g_Globals.General.WebRemote);
							if (g_Globals.General.WebRemote)
							{
								std::string url = WebPanel::GetWebUrl();
								ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.0f), "%s", url.c_str());

								if (Custom::Button(XorStr("Copy Link"), ImVec2(ImGui::GetWindowSize().x - 28, 30)))
								{
									if (OpenClipboard(nullptr))
									{
										EmptyClipboard();
										HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, url.size() + 1);
										if (mem)
										{
											memcpy(GlobalLock(mem), url.c_str(), url.size() + 1);
											GlobalUnlock(mem);
											SetClipboardData(CF_TEXT, mem);
											NotifyManager::Send(XorStr("Link Copiado!"), 2500);
										}
										CloseClipboard();
									}
								}
							}

							if (Custom::Button(XorStr("Save Config"), ImVec2(ImGui::GetWindowSize().x - 28, 36)))
							{
								if (Cheat::Manager::Save())
								{
									NotifyManager::Send(XorStr("Config Salva!"), 3000);
								}
								else
								{
									NotifyManager::Send(XorStr("Falha ao Salvar Config."), 3000);
								}
							}
							ImGui::Dummy(ImVec2(0, 3));
							if (Custom::Button(XorStr("Load Config"), ImVec2(ImGui::GetWindowSize().x - 28, 36)))
							{
								if (Cheat::Manager::Load())
								{
									NotifyManager::Send(XorStr("Config Carregada!"), 3000);
								}
								else
								{
									NotifyManager::Send(XorStr("Config Inexistente."), 3000);
								}
							}
							ImGui::Dummy(ImVec2(0, 3));
							if (Custom::Button(XorStr("Restart"), ImVec2(ImGui::GetWindowSize().x - 28, 36)))
							{
								std::thread(g_FreeFireMemory.Restart).detach();
								NotifyManager::Send(XorStr("Restarted"), 4000);
							}
						}
						Custom::EndCustomChild();

						ImGui::SetCursorPos(ImVec2(cardWidth + 10 + AnimaTab, 0));
						Custom::CustomChild(XorStr("Extra"), ImVec2(cardWidth, cardHeight));
						{
							Custom::KeyBind(XorStr("Menu Key"), &g_Globals.General.MenuKey, false);

							ImGui::Dummy(ImVec2(0, 8));

							if (Custom::Button(XorStr("Unload"), ImVec2(ImGui::GetWindowSize().x - 28, 36)))
							{
								g_WantShutdown = true;
							}
						}
						Custom::EndCustomChild();
					}
					ImGui::EndGroup();
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void Interface::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			ResizeWidht = (UINT)LOWORD(lParam);
			ResizeHeight = (UINT)HIWORD(lParam);
			if (ResizeWidht > 0 && ResizeHeight > 0)
			{
				glViewport(0, 0, ResizeWidht, ResizeHeight);
			}
		}
		break;
	}

	if (bIsMenuOpen)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
	}
}

void Interface::HandleMenuKey()
{
	static bool MenuKeyDown = false;
	if (GetAsyncKeyState(g_Globals.General.MenuKey) & 0x8000)
	{
		if (!MenuKeyDown)
		{
			MenuKeyDown = true;
			bIsMenuOpen = !bIsMenuOpen;
			if (bIsMenuOpen)
			{
				// Remove transparent & noactivate styles to allow clicking and interaction
				LONG style = GetWindowLong(hWindow, GWL_EXSTYLE);
				style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
				SetWindowLong(hWindow, GWL_EXSTYLE, style);
				
				SetForegroundWindow(hWindow);
			}
			else
			{
				// Re-add transparent & noactivate styles so clicks pass through to the game
				LONG style = GetWindowLong(hWindow, GWL_EXSTYLE);
				style |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
				SetWindowLong(hWindow, GWL_EXSTYLE, style);
				
				SetForegroundWindow(hTargetWindow);
			}
			// Update window position and flags to apply styling changes instantly
			SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
	}
	else
	{
		MenuKeyDown = false;
	}
}

void Interface::ShutDown()
{
	DiscordRPC::Shutdown();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	Fonts::CleanupTextures();
	Overlay::ShutDown();
}