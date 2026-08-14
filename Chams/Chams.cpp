#include "Chams.hpp"
#include <VehCpuHook/VehCpuHook.hpp>
#include <Cheat/Globals.hpp>
#include <Windows.h>
#include <exception>
#include <XorStr.hpp>
#include <Lazyimporter.hpp>

// =====================================================================
// IAT-clean API resolution
// GetModuleHandleA e GetProcAddress são resolvidos via LazyImporter
// (remove ambos da IAT), depois usados normalmente com XorStr
// =====================================================================

static decltype(&GetModuleHandleA) pLI_GetModuleHandleA = nullptr;
static decltype(&GetProcAddress)   pLI_GetProcAddress = nullptr;

static bool EnsureLazyAPIs()
{
    if (!pLI_GetModuleHandleA)
        pLI_GetModuleHandleA = lzimpLI_FN(GetModuleHandleA).get<decltype(&GetModuleHandleA)>();
    if (!pLI_GetProcAddress)
        pLI_GetProcAddress = lzimpLI_FN(GetProcAddress).get<decltype(&GetProcAddress)>();
    return pLI_GetModuleHandleA && pLI_GetProcAddress;
}

// =====================================================================
// GL types + constants
// =====================================================================

typedef unsigned int  GLenum;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned int  GLuint;
typedef unsigned char GLboolean;
typedef float         GLfloat;
typedef char          GLchar;
typedef float         GLclampf;
typedef double        GLdouble;
typedef double        GLclampd;

#define GL_FALSE                    0
#define GL_TRUE                     1
#define GL_TRIANGLES                0x0004
#define GL_LINE                     0x1B01
#define GL_FILL                     0x1B02
#define GL_DEPTH_TEST               0x0B71
#define GL_DEPTH_FUNC               0x0B74
#define GL_DEPTH_RANGE              0x0B70
#define GL_LESS                     0x0201
#define GL_ONE                      1
#define GL_ZERO                     0
#define GL_POLYGON_OFFSET_LINE      0x2A02
#define GL_FRONT_AND_BACK           0x0408
#define GL_COLOR_WRITEMASK          0x0C23
#define GL_CURRENT_COLOR            0x0B00
#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE_BINDING_2D       0x8069
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_LINEAR                   0x2601
#define GL_RGBA                     0x1908
#define GL_RGB8                     0x8051
#define GL_UNSIGNED_BYTE            0x1401
#define GL_CURRENT_PROGRAM          0x8B8D
#define GL_BLEND                    0x0BE2
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_CONSTANT_ALPHA           0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#define GL_BLEND_SRC                0x0BE1
#define GL_BLEND_DST                0x0BE0
#define GL_BLEND_COLOR              0x8005

// =====================================================================
// GL function pointers
// =====================================================================

static void      (APIENTRY* pglEnable)(GLenum) = nullptr;
static void      (APIENTRY* pglDisable)(GLenum) = nullptr;
static GLboolean(APIENTRY* pglIsEnabled)(GLenum) = nullptr;
static void      (APIENTRY* pglGetIntegerv)(GLenum, GLint*) = nullptr;
static void      (APIENTRY* pglGetBooleanv)(GLenum, GLboolean*) = nullptr;
static void      (APIENTRY* pglGetFloatv)(GLenum, GLfloat*) = nullptr;
static void      (APIENTRY* pglGetDoublev)(GLenum, GLdouble*) = nullptr;
static void      (APIENTRY* pglDepthFunc)(GLenum) = nullptr;
static void      (APIENTRY* pglDepthRange)(GLclampd, GLclampd) = nullptr;
static void      (APIENTRY* pglColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
static void      (APIENTRY* pglColor4f)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
static void      (APIENTRY* pglColor4fv)(const GLfloat*) = nullptr;
static void      (APIENTRY* pglPolygonMode)(GLenum, GLenum) = nullptr;
static void      (APIENTRY* pglPolygonOffset)(GLfloat, GLfloat) = nullptr;
static void      (APIENTRY* pglLineWidth)(GLfloat) = nullptr;
static void      (APIENTRY* pglGenTextures)(GLsizei, GLuint*) = nullptr;
static void      (APIENTRY* pglDeleteTextures)(GLsizei, const GLuint*) = nullptr;
static void      (APIENTRY* pglBindTexture)(GLenum, GLuint) = nullptr;
static void      (APIENTRY* pglTexParameteri)(GLenum, GLenum, GLint) = nullptr;
static void      (APIENTRY* pglTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
static void      (APIENTRY* pglBlendFunc)(GLenum, GLenum) = nullptr;
static void      (APIENTRY* pglBlendColor)(GLclampf, GLclampf, GLclampf, GLclampf) = nullptr;

typedef GLint(APIENTRY* PFN_glGetUniformLocation)(GLuint, const GLchar*);
static PFN_glGetUniformLocation pglGetUniformLocation = nullptr;

typedef void* (WINAPI* PFN_wglGetProcAddress)(const char*);
static PFN_wglGetProcAddress pwglGetProcAddress = nullptr;

typedef BOOL(WINAPI* PFN_wglSwapBuffers)(HDC);
typedef void (APIENTRY* PFN_glDrawElements)(GLenum, GLsizei, GLenum, const void*);

// =====================================================================
// State
// =====================================================================

static volatile bool g_shutDown = false;
static volatile LONG g_chamsState = 0;
static volatile LONG g_inHook = 0;

static int g_slotSwapBuffers = -1;
static int g_slotDrawElements = -1;

static PFN_wglSwapBuffers  g_oSwapBuffers = nullptr;
static PFN_glDrawElements  g_oDrawElements = nullptr;

static GLuint g_texNear = 0;
static GLuint g_texFar = 0;
static volatile LONG g_wantRefresh = 0;

// Cache do módulo
static HMODULE g_hGL = nullptr;

// =====================================================================
// Safe call original
// =====================================================================

static inline void SafeCallOriginal(GLenum m, GLsizei c, GLenum t, const void* i)
{
    int slot = g_slotDrawElements;
    PFN_glDrawElements fn = g_oDrawElements;
    if (slot >= 0 && fn)
    {
        VehCpuHook::CallOriginal(slot);
        fn(m, c, t, i);
    }
}

#define CALL_ORIGINAL(m,c,t,i) SafeCallOriginal(m,c,t,i)

// =====================================================================
// GL resolution — LazyImporter resolve GetModuleHandleA/GetProcAddress
// Zero IAT entries, strings protegidas por XorStr
// =====================================================================

static HMODULE GetGL()
{
    if (!g_hGL)
    {
        if (!EnsureLazyAPIs()) return nullptr;
        g_hGL = pLI_GetModuleHandleA(XorStr("opengl32.dll"));
    }
    return g_hGL;
}

static bool ResolveGL1()
{
    HMODULE hGL = GetGL();
    if (!hGL || !pLI_GetProcAddress) return false;

#define R(fn) *(FARPROC*)&p##fn = pLI_GetProcAddress(hGL, XorStr(#fn)); if (!p##fn) return false
    R(glEnable); R(glDisable); R(glIsEnabled);
    R(glGetIntegerv); R(glGetBooleanv); R(glGetFloatv); R(glGetDoublev);
    R(glDepthFunc); R(glDepthRange);
    R(glColorMask);
    R(glColor4f); R(glColor4fv);
    R(glPolygonMode); R(glPolygonOffset); R(glLineWidth);
    R(glGenTextures); R(glDeleteTextures); R(glBindTexture);
    R(glTexParameteri); R(glTexImage2D);
    R(glBlendFunc);
#undef R

    * (FARPROC*)&pglBlendColor = pLI_GetProcAddress(hGL, XorStr("glBlendColor"));
    pwglGetProcAddress = (PFN_wglGetProcAddress)pLI_GetProcAddress(hGL, XorStr("wglGetProcAddress"));
    if (!pglBlendColor && pwglGetProcAddress)
        *(void**)&pglBlendColor = pwglGetProcAddress(XorStr("glBlendColor"));

    return true;
}

// =====================================================================
// Texture helpers
// =====================================================================

static void MakeColorTexture(GLuint* tex, float r, float g, float b, float a)
{
    const int SZ = 128;
    unsigned char* d = new unsigned char[SZ * SZ * 4];
    unsigned char cr = (unsigned char)(r * 255.0f);
    unsigned char cg = (unsigned char)(g * 255.0f);
    unsigned char cb = (unsigned char)(b * 255.0f);
    unsigned char ca = (unsigned char)(a * 255.0f);
    for (int i = 0; i < SZ * SZ; i++)
    {
        d[i * 4 + 0] = cr;
        d[i * 4 + 1] = cg;
        d[i * 4 + 2] = cb;
        d[i * 4 + 3] = ca;
    }

    GLint prevTex = 0;
    pglGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    pglGenTextures(1, tex);
    pglBindTexture(GL_TEXTURE_2D, *tex);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SZ, SZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    pglBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
    delete[] d;
}

static bool ChamsContextInitialize()
{
    try
    {
        auto& c = g_Globals.Visuals.Chams;
        MakeColorTexture(&g_texNear, c.NearColor[0], c.NearColor[1], c.NearColor[2], c.NearColor[3]);
        MakeColorTexture(&g_texFar, c.FarColor[0], c.FarColor[1], c.FarColor[2], c.FarColor[3]);
        return true;
    }
    catch (const std::exception&)
    {
        // Excecao dentro do hook GL (bad_alloc ao criar textura) propaga para
        // a call do jogo e derruba o processo inteiro (ESP + overlay juntos).
        // Falha controlada: nao habilita chams, jogo continua normal.
        g_texNear = 0;
        g_texFar = 0;
        return false;
    }
    catch (...)
    {
        g_texNear = 0;
        g_texFar = 0;
        return false;
    }
}

// =====================================================================
// Uniform check
// =====================================================================

static bool CurrentShaderHasUniform(const char* name)
{
    if (!pglGetUniformLocation) return false;
    GLint prog = 0;
    pglGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    if (!prog) return false;
    return pglGetUniformLocation((GLuint)prog, name) != -1;
}

// =====================================================================
// glDrawElements Hook
// =====================================================================

static void APIENTRY hkDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (g_shutDown || g_chamsState == 0)
    {
        CALL_ORIGINAL(mode, count, type, indices);
        return;
    }

    InterlockedIncrement(&g_inHook);

    // Lazy resolve GL2+ extensions via wglGetProcAddress
    static bool gl2Ok = false;
    if (!gl2Ok)
    {
        if (pwglGetProcAddress)
        {
            pglGetUniformLocation = (PFN_glGetUniformLocation)pwglGetProcAddress(XorStr("glGetUniformLocation"));
            if (!pglBlendColor)
                *(void**)&pglBlendColor = pwglGetProcAddress(XorStr("glBlendColor"));
        }
        if (pglGetUniformLocation)
            gl2Ok = true;
        else
        {
            CALL_ORIGINAL(mode, count, type, indices);
            InterlockedDecrement(&g_inHook);
            return;
        }
    }

    // Auto-detect mudanca de cor
    {
        static float lastNear[4] = { -1,-1,-1,-1 };
        static float lastFar[4] = { -1,-1,-1,-1 };
        auto& c = g_Globals.Visuals.Chams;
        bool changed = InterlockedCompareExchange(&g_wantRefresh, 0, 1) == 1;
        for (int i = 0; i < 4; i++)
        {
            if (lastNear[i] != c.NearColor[i] || lastFar[i] != c.FarColor[i])
                changed = true;
        }
        if (changed)
        {
            if (g_texNear) { pglDeleteTextures(1, &g_texNear); g_texNear = 0; }
            if (g_texFar) { pglDeleteTextures(1, &g_texFar);  g_texFar = 0; }
            for (int i = 0; i < 4; i++) { lastNear[i] = c.NearColor[i]; lastFar[i] = c.FarColor[i]; }
        }
    }

    // ─── Outline/Rim chams ───
    if ((CurrentShaderHasUniform(XorStr("_OutLineWidth")) || CurrentShaderHasUniform(XorStr("_RimColor"))) && count != 9888)
    {
        GLboolean depthWas = pglIsEnabled(GL_DEPTH_TEST);
        GLint oldFunc = GL_LESS;
        pglGetIntegerv(GL_DEPTH_FUNC, &oldFunc);
        GLboolean polyWas = pglIsEnabled(GL_POLYGON_OFFSET_LINE);
        GLboolean cmask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        pglGetBooleanv(GL_COLOR_WRITEMASK, cmask);
        GLfloat oldCol[4] = { 1.f, 1.f, 1.f, 1.f };
        pglGetFloatv(GL_CURRENT_COLOR, oldCol);

        pglEnable(GL_DEPTH_TEST);
        pglDepthFunc(GL_LESS);
        pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        CALL_ORIGINAL(mode, count, type, indices);

        pglEnable(GL_POLYGON_OFFSET_LINE);
        pglPolygonOffset(-1.0f, -1.0f);
        pglPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        pglLineWidth(0.1f);
        pglDisable(GL_DEPTH_TEST);
        pglColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        CALL_ORIGINAL(mode, count, type, indices);

        pglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        polyWas ? pglEnable(GL_POLYGON_OFFSET_LINE) : pglDisable(GL_POLYGON_OFFSET_LINE);
        depthWas ? pglEnable(GL_DEPTH_TEST) : pglDisable(GL_DEPTH_TEST);
        pglDepthFunc(oldFunc);
        pglColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
        pglColor4fv(oldCol);
        InterlockedDecrement(&g_inHook);
        return;
    }

    // ─── Character model filter ───
    if (mode != GL_TRIANGLES || count < 6000)
    {
        CALL_ORIGINAL(mode, count, type, indices);
        InterlockedDecrement(&g_inHook);
        return;
    }

    GLint prog = 0;
    pglGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    if (!prog)
    {
        CALL_ORIGINAL(mode, count, type, indices);
        InterlockedDecrement(&g_inHook);
        return;
    }
    if (pglGetUniformLocation((GLuint)prog, XorStr("_CharaLightIntensity")) == -1)
    {
        CALL_ORIGINAL(mode, count, type, indices);
        InterlockedDecrement(&g_inHook);
        return;
    }

    // ─── Lazy create texturas ───
    static bool texInit = false;
    if (!texInit || (!g_texNear && !g_texFar))
    {
        if (!(texInit = ChamsContextInitialize()))
        {
            CALL_ORIGINAL(mode, count, type, indices);
            g_shutDown = true;
            InterlockedDecrement(&g_inHook);
            return;
        }
    }

    auto& chams = g_Globals.Visuals.Chams;
    bool useAlpha = pglBlendColor != nullptr;

    // =================================================================
    // AggressiveMode
    // =================================================================
    if (chams.AggressiveMode)
    {
        // ── Pass 1: Far (depth OFF + FarColor) ──
        {
            GLint prevTex = 0;
            pglGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
            pglBindTexture(GL_TEXTURE_2D, g_texFar);
            GLboolean dw = pglIsEnabled(GL_DEPTH_TEST);

            pglDisable(GL_DEPTH_TEST);

            if (useAlpha && chams.FarColor[3] < 1.0f)
            {
                GLboolean blendWas = pglIsEnabled(GL_BLEND);
                GLint oldSrc = 0, oldDst = 0;
                GLfloat oldBC[4] = {};
                pglGetIntegerv(GL_BLEND_SRC, &oldSrc);
                pglGetIntegerv(GL_BLEND_DST, &oldDst);
                pglGetFloatv(GL_BLEND_COLOR, oldBC);
                pglEnable(GL_BLEND);
                pglBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
                pglBlendColor(0.0f, 0.0f, 0.0f, chams.FarColor[3]);
                CALL_ORIGINAL(mode, count, type, indices);
                blendWas ? pglEnable(GL_BLEND) : pglDisable(GL_BLEND);
                pglBlendFunc((GLenum)oldSrc, (GLenum)oldDst);
                pglBlendColor(oldBC[0], oldBC[1], oldBC[2], oldBC[3]);
            }
            else
            {
                CALL_ORIGINAL(mode, count, type, indices);
            }

            dw ? pglEnable(GL_DEPTH_TEST) : pglDisable(GL_DEPTH_TEST);
            pglBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
        }

        // ── Pass 2: Near (depth ON + GL_LESS + NearColor) ──
        {
            GLint prevTex = 0;
            pglGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
            pglBindTexture(GL_TEXTURE_2D, g_texNear);
            GLboolean dw = pglIsEnabled(GL_DEPTH_TEST);
            GLint odf = GL_LESS;
            pglGetIntegerv(GL_DEPTH_FUNC, &odf);

            pglEnable(GL_DEPTH_TEST);
            pglDepthFunc(GL_LESS);

            if (useAlpha && chams.NearColor[3] < 1.0f)
            {
                GLboolean blendWas = pglIsEnabled(GL_BLEND);
                GLint oldSrc = 0, oldDst = 0;
                GLfloat oldBC[4] = {};
                pglGetIntegerv(GL_BLEND_SRC, &oldSrc);
                pglGetIntegerv(GL_BLEND_DST, &oldDst);
                pglGetFloatv(GL_BLEND_COLOR, oldBC);
                pglEnable(GL_BLEND);
                pglBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
                pglBlendColor(0.0f, 0.0f, 0.0f, chams.NearColor[3]);
                CALL_ORIGINAL(mode, count, type, indices);
                blendWas ? pglEnable(GL_BLEND) : pglDisable(GL_BLEND);
                pglBlendFunc((GLenum)oldSrc, (GLenum)oldDst);
                pglBlendColor(oldBC[0], oldBC[1], oldBC[2], oldBC[3]);
            }
            else
            {
                CALL_ORIGINAL(mode, count, type, indices);
            }

            dw ? pglEnable(GL_DEPTH_TEST) : pglDisable(GL_DEPTH_TEST);
            pglDepthFunc(odf);
            pglBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
        }

        // ── Pass 3: depthRange push ──
        {
            GLdouble dr[2] = { 0.0, 1.0 };
            pglGetDoublev(GL_DEPTH_RANGE, dr);
            GLboolean bw = pglIsEnabled(GL_BLEND);
            GLint os = GL_ONE, od = GL_ZERO;
            pglGetIntegerv(GL_BLEND_SRC, &os);
            pglGetIntegerv(GL_BLEND_DST, &od);

            pglDepthRange(1.0, 0.5);
            if (!bw) pglEnable(GL_BLEND);
            pglBlendFunc(GL_SRC_ALPHA, GL_ONE);

            CALL_ORIGINAL(mode, count, type, indices);

            pglDepthRange(dr[0], dr[1]);
            pglBlendFunc(os, od);
            if (!bw) pglDisable(GL_BLEND);
        }

        InterlockedDecrement(&g_inHook);
        return;
    }

    // =================================================================
    // Normal mode
    // =================================================================

    // Pass 1: Far
    {
        GLint prevTex = 0;
        pglGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
        pglBindTexture(GL_TEXTURE_2D, g_texFar);
        GLboolean dw = pglIsEnabled(GL_DEPTH_TEST);

        pglDisable(GL_DEPTH_TEST);

        GLboolean blendWas = GL_FALSE;
        GLint oldSrc = 0, oldDst = 0;
        GLfloat oldBC[4] = {};
        if (useAlpha && chams.FarColor[3] < 1.0f)
        {
            blendWas = pglIsEnabled(GL_BLEND);
            pglGetIntegerv(GL_BLEND_SRC, &oldSrc);
            pglGetIntegerv(GL_BLEND_DST, &oldDst);
            pglGetFloatv(GL_BLEND_COLOR, oldBC);
            pglEnable(GL_BLEND);
            pglBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
            pglBlendColor(0.0f, 0.0f, 0.0f, chams.FarColor[3]);
        }

        CALL_ORIGINAL(mode, count, type, indices);

        if (useAlpha && chams.FarColor[3] < 1.0f)
        {
            blendWas ? pglEnable(GL_BLEND) : pglDisable(GL_BLEND);
            pglBlendFunc((GLenum)oldSrc, (GLenum)oldDst);
            pglBlendColor(oldBC[0], oldBC[1], oldBC[2], oldBC[3]);
        }

        dw ? pglEnable(GL_DEPTH_TEST) : pglDisable(GL_DEPTH_TEST);
        pglBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
    }

    // Pass 2: Near
    {
        GLint prevTex = 0;
        pglGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
        pglBindTexture(GL_TEXTURE_2D, g_texNear);
        GLboolean dw = pglIsEnabled(GL_DEPTH_TEST);
        GLint odf = GL_LESS;
        pglGetIntegerv(GL_DEPTH_FUNC, &odf);

        pglEnable(GL_DEPTH_TEST);
        pglDepthFunc(GL_LESS);

        GLboolean blendWas = GL_FALSE;
        GLint oldSrc = 0, oldDst = 0;
        GLfloat oldBC[4] = {};
        if (useAlpha && chams.NearColor[3] < 1.0f)
        {
            blendWas = pglIsEnabled(GL_BLEND);
            pglGetIntegerv(GL_BLEND_SRC, &oldSrc);
            pglGetIntegerv(GL_BLEND_DST, &oldDst);
            pglGetFloatv(GL_BLEND_COLOR, oldBC);
            pglEnable(GL_BLEND);
            pglBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
            pglBlendColor(0.0f, 0.0f, 0.0f, chams.NearColor[3]);
        }

        CALL_ORIGINAL(mode, count, type, indices);

        if (useAlpha && chams.NearColor[3] < 1.0f)
        {
            blendWas ? pglEnable(GL_BLEND) : pglDisable(GL_BLEND);
            pglBlendFunc((GLenum)oldSrc, (GLenum)oldDst);
            pglBlendColor(oldBC[0], oldBC[1], oldBC[2], oldBC[3]);
        }

        dw ? pglEnable(GL_DEPTH_TEST) : pglDisable(GL_DEPTH_TEST);
        pglDepthFunc(odf);
        pglBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
    }

    InterlockedDecrement(&g_inHook);
}

#undef CALL_ORIGINAL

// =====================================================================
// wglSwapBuffers Hook
// =====================================================================

static BOOL WINAPI hkSwapBuffers(HDC hdc)
{
    int slot = g_slotSwapBuffers;
    PFN_wglSwapBuffers fn = g_oSwapBuffers;
    if (slot >= 0 && fn)
    {
        VehCpuHook::CallOriginal(slot);
        return fn(hdc);
    }
    return FALSE;
}

// =====================================================================
// PUBLIC API
// =====================================================================

bool Chams::Enable()
{
    if (g_chamsState != 0)
        return true;

    if (!pglEnable && !ResolveGL1())
        return false;

    HMODULE hGL = GetGL();
    if (!hGL) return false;

    g_shutDown = false;

    void* pSwap = (void*)pLI_GetProcAddress(hGL, XorStr("wglSwapBuffers"));
    if (!pSwap) return false;

    g_slotSwapBuffers = VehCpuHook::AddHook(
        pSwap, (void*)hkSwapBuffers, (void**)&g_oSwapBuffers);
    if (g_slotSwapBuffers < 0) return false;

    FARPROC drawAddr = nullptr;
    if (pwglGetProcAddress)
        drawAddr = (FARPROC)pwglGetProcAddress(XorStr("glDrawElements"));

    if (!drawAddr)
    {
        VehCpuHook::RemoveHook(g_slotSwapBuffers);
        g_slotSwapBuffers = -1;
        return false;
    }

    g_slotDrawElements = VehCpuHook::AddHook(
        (void*)drawAddr, (void*)hkDrawElements, (void**)&g_oDrawElements);

    if (g_slotDrawElements < 0)
    {
        VehCpuHook::RemoveHook(g_slotSwapBuffers);
        g_slotSwapBuffers = -1;
        return false;
    }

    InterlockedExchange(&g_chamsState, 1);
    return true;
}

void Chams::Disable()
{
    if (g_chamsState == 0)
        return;

    InterlockedExchange(&g_chamsState, 0);
    g_shutDown = true;

    Sleep(200);
    for (int i = 0; i < 50 && g_inHook > 0; i++)
        Sleep(10);

    if (g_slotDrawElements >= 0)
    {
        VehCpuHook::RemoveHook(g_slotDrawElements);
        g_slotDrawElements = -1;
    }
    if (g_slotSwapBuffers >= 0)
    {
        VehCpuHook::RemoveHook(g_slotSwapBuffers);
        g_slotSwapBuffers = -1;
    }

    g_texNear = 0;
    g_texFar = 0;
}

void Chams::Shutdown()
{
    InterlockedExchange(&g_chamsState, 0);
    g_shutDown = true;

    Sleep(200);
    for (int i = 0; i < 50 && g_inHook > 0; i++)
        Sleep(10);

    if (g_slotDrawElements >= 0)
    {
        VehCpuHook::RemoveHook(g_slotDrawElements);
        g_slotDrawElements = -1;
    }
    if (g_slotSwapBuffers >= 0)
    {
        VehCpuHook::RemoveHook(g_slotSwapBuffers);
        g_slotSwapBuffers = -1;
    }

    if ( pglDeleteTextures )
    {
        if ( g_texNear ) { pglDeleteTextures( 1, &g_texNear ); g_texNear = 0; }
        if ( g_texFar )  { pglDeleteTextures( 1, &g_texFar );  g_texFar = 0; }
    }
    else
    {
        g_texNear = 0;
        g_texFar = 0;
    }
}

void Chams::RefreshColors()
{
    InterlockedExchange(&g_wantRefresh, 1);
}

bool Chams::IsEnabled()
{
    return g_chamsState != 0;
}