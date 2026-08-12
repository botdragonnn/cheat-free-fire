#include "Fonts.hpp"
#include <wincodec.h>
#include <vector>
#pragma comment(lib, "Windowscodecs.lib")

#include "Bytes/BytesFonts.hpp"
#include "Bytes/IconsFontAwesome6.h"
#include "Bytes/FontAwesomeBytes.hpp"
#include "Bytes/IconsFontAwesome6Brands.h"
#include "Bytes/BytesImg.hpp"
#include <Utils/Utils.hpp>
#include <imgui_freetype.h>
#pragma comment(lib, "freetype.lib")

bool Fonts::LoadTextureFromPNG(const unsigned char* png_data, size_t png_size, GLuint* out_texture, int* out_width, int* out_height) {
    if (!png_data || png_size == 0 || !out_texture) return false;

    // CoInitializeEx tolerates being called multiple times on the same thread
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool coInit = SUCCEEDED(hr);

    IWICImagingFactory* pFactory = NULL;
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) {
        if (coInit) CoUninitialize();
        return false;
    }

    IWICStream* pStream = NULL;
    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr)) {
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pStream->InitializeFromMemory((BYTE*)png_data, (DWORD)png_size);
    if (FAILED(hr)) {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* pFrame = NULL;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) {
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    UINT width = 0, height = 0;
    pFrame->GetSize(&width, &height);

    IWICFormatConverter* pConverter = NULL;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) {
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pStream->Release();
        pFactory->Release();
        if (coInit) CoUninitialize();
        return false;
    }

    UINT rowStride = width * 4;
    UINT bufferSize = rowStride * height;
    std::vector<unsigned char> rgba(bufferSize);
    hr = pConverter->CopyPixels(NULL, rowStride, bufferSize, rgba.data());

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();
    if (coInit) CoUninitialize();

    if (FAILED(hr)) return false;

    // Load into OpenGL
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    *out_texture = texture_id;
    if (out_width) *out_width = (int)width;
    if (out_height) *out_height = (int)height;

    return true;
}

void Fonts::Initialize() {
    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();

    io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;

    ImFontConfig Config{};
    Config.OversampleH = 2;
    Config.OversampleV = 1;
    static const ImWchar kRangesPtBrComb[] = {
         0x0020, 0x00FF, // Latin + Latin-1 (ã/õ/ç/á…)
         0x0100, 0x024F, // Latin Extended A/B
         0x0300, 0x036F, // **Combining diacritics (inclui U+0303)**
         0
    };

    Config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_NoHinting;
    Verdana = io.Fonts->AddFontFromMemoryCompressedTTF(verdana_compressed_data, verdana_compressed_size, 16.0f, &Config, kRangesPtBrComb);
    InterBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 16, &Config, kRangesPtBrComb);
    InterMedium = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 16, &Config, kRangesPtBrComb);
    InterRegular = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 16, &Config, kRangesPtBrComb);

    ImFontConfig GffConfig;
    GffConfig.OversampleH = 2;
    GffConfig.OversampleV = 1;
    GffConfig.MergeMode = true;
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,   // Latin básico + acentos
        0x0100, 0x024F,   // Latin Extended-A e B
        0x0250, 0x02AF,   // IPA Extensions
        0x0300, 0x036F,   // Combining Diacritical Marks
        0x0370, 0x03FF,   // Grego básico
        0x1F00, 0x1FFF,   // Grego estendido
        0x0400, 0x04FF,   // Cirílico
        0x0500, 0x052F,   // Cirílico estendido
        0x0530, 0x058F,   // Armênio
        0x0590, 0x05FF,   // Hebraico
        0x0600, 0x06FF,   // Árabe
        0x0700, 0x074F,   // Sírios
        0x0900, 0x097F,   // Devanagari
        0x0980, 0x09FF,   // Bengali
        0x0A00, 0x0A7F,   // Gurmukhi
        0x0A80, 0x0AFF,   // Gujarati
        0x0B00, 0x0B7F,   // Oriya
        0x0B80, 0x0BFF,   // Tâmil
        0x0C00, 0x0C7F,   // Telugu
        0x0C80, 0x0CFF,   // Kannada
        0x0D00, 0x0D7F,   // Malaiala
        0x0E00, 0x0E7F,   // Tailandês
        0x0E80, 0x0EFF,   // Lao
        0x0F00, 0x0FFF,   // Tibetano
        0x1000, 0x109F,   // Myanmar
        0x1100, 0x11FF,   // Hangul Jamo
        0x1E00, 0x1EFF,   // Latin Extended Additional
        0x1F00, 0x1FFF,   // Grego estendido
        0x2000, 0x206F,   // Pontuação geral
        0x2070, 0x209F,   // Sub/Superscript
        0x20A0, 0x20CF,   // Moedas
        0x2100, 0x214F,   // Letras adicionais
        0x2150, 0x218F,   // Numerais
        0x2190, 0x21FF,   // Setas
        0x2200, 0x22FF,   // Símbolos matemáticos
        0x2300, 0x23FF,   // Símbolos técnicos
        0x2400, 0x243F,   // Control Pictures
        0x2440, 0x245F,   // Box Drawing
        0x2460, 0x24FF,   // Enclosed Alphanumerics
        0x2500, 0x257F,   // Box Drawing
        0x2580, 0x259F,   // Block Elements
        0x25A0, 0x25FF,   // Geometric Shapes
        0x2600, 0x26FF,   // Misc Symbols
        0x2700, 0x27BF,   // Dingbats
        0x2E80, 0x2EFF,   // CJK Radicals Supplement
        0x2F00, 0x2FDF,   // Kangxi Radicals
        0x2FF0, 0x2FFF,   // Ideographic Description
        0x3000, 0x303F,   // CJK Symbols & Punctuation
        0x3040, 0x309F,   // Hiragana
        0x30A0, 0x30FF,   // Katakana
        0x3100, 0x312F,   // Bopomofo
        0x3130, 0x318F,   // Hangul Compatibility Jamo
        0x3190, 0x319F,   // Kanbun
        0x31A0, 0x31BF,   // Bopomofo Extended
        0x31F0, 0x31FF,   // Katakana Phonetic Extensions
        0x3200, 0x32FF,   // Enclosed CJK Letters
        0x3300, 0x33FF,   // CJK Compatibility
        0x3400, 0x4DBF,   // CJK Unified Ideographs Extension A
        0x4E00, 0x9FFF,   // CJK Unified Ideographs
        0xF900, 0xFAFF,   // CJK Compatibility Ideographs
        0xFE00, 0xFE0F,   // Variation Selectors
        0xFF00, 0xFFEF,   // Halfwidth/Halfwidth + Fullwidth
        0x1F300, 0x1F5FF, // Emojis
        0x1F600, 0x1F64F, // Emojis adicionais (faces)
        0x1F680, 0x1F6FF, // Transport and map symbols
        0x1F700, 0x1F77F, // Alchemical Symbols
        0x0000           // Termina a lista
    };

    GffConfig.GlyphRanges = ranges;
    GffConfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_NoHinting;
    Gff = io.Fonts->AddFontFromMemoryCompressedTTF(seguiemj_compressed_data, seguiemj_compressed_size, 18.0f, &GffConfig, GffConfig.GlyphRanges);

    ImFontConfig IconWeaponConfig;
    IconWeaponConfig.OversampleH = 2;
    IconWeaponConfig.OversampleV = 1;
    IconWeaponConfig.PixelSnapH = true;
    IconWeaponConfig.MergeMode = true;
    IconWeaponConfig.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_NoHinting;
    static const ImWchar IconWeaponRanges[] = { 0xe000, 0xe204, 0x00 };
    IconWeapon = io.Fonts->AddFontFromMemoryCompressedTTF(weapon_compressed_data, weapon_compressed_size, 41.0f, &IconWeaponConfig, IconWeaponRanges);

    static const ImWchar FontAwesomeRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, ICON_MAX_FA, 0 };
    static const ImWchar FontAwesomeRangesBrands[] = { ICON_MIN_FAB, ICON_MAX_16_FAB, ICON_MAX_FAB, 0 };
    ImFontConfig FontAwesomeConfig;
    FontAwesomeConfig.GlyphMinAdvanceX = 25.f * (2.0f / 3.0f);
    FontAwesomeConfig.PixelSnapH = true;
    //FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF(fa_regular_400_compressed_data, fa_regular_400_compressed_size, 25.f * (2.0f / 3.0f), &FontAwesomeConfig, &FontAwesomeRanges[0]);
    FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, 27.f * (2.0f / 3.0f), &FontAwesomeConfig, &FontAwesomeRanges[0]);
    //FontAwesomeBrands = io.Fonts->AddFontFromMemoryCompressedTTF(fa_brands_400_compressed_data, fa_brands_400_compressed_size, 17.f, &FontAwesomeConfig, &FontAwesomeRangesBrands[0]);

    // Load PNG logo dynamically using native WIC decoder
    int logoW = 0, logoH = 0;
    if (LoadTextureFromPNG(LogoMenuRawRGBA, LogoMenuRawRGBASize, &LogoTexture, &logoW, &logoH)) {
        LogoMenuWidth = logoW;
        LogoMenuHeight = logoH;
    } else {
        // Fallback to raw load if it fails
        LoadTextureFromRawRGBA(LogoMenuRawRGBA, LogoWidth, LogoHeight, &LogoTexture, false);
        LogoMenuWidth = LogoWidth;
        LogoMenuHeight = LogoHeight;
    }

    ImGuiFreeType::BuildFontAtlas(io.Fonts);
}

bool Fonts::LoadTextureFromRawRGBA(const unsigned char* rgba_data, int width, int height, GLuint* out_texture, bool flip_vertical) {

    if (!rgba_data || width <= 0 || height <= 0 || !out_texture) {
        printf("[Fonts] ERROR: Invalid parameters\n");
        return false;
    }

    printf("[Fonts] Loading texture from raw RGBA: %dx%d\n", width, height);

    size_t total_size = (size_t)width * height * 4;
    unsigned char* final_data = nullptr;

    if (flip_vertical) {
        final_data = new unsigned char[total_size];
        if (!final_data) {
            printf("[Fonts] ERROR: Failed to allocate memory\n");
            return false;
        }

        int row_size = width * 4;
        for (int y = 0; y < height; y++) {
            int src_row = height - 1 - y;
            memcpy(final_data + (y * row_size), rgba_data + (src_row * row_size), row_size);
        }

        printf("[Fonts] Image flipped vertically\n");
    }
    else {
        final_data = (unsigned char*)rgba_data;
    }

    GLuint texture_id;
    glGenTextures(1, &texture_id);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("[Fonts] ERROR: glGenTextures failed: 0x%X\n", error);
        if (flip_vertical && final_data != rgba_data) {
            delete[] final_data;
        }
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);

    error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("[Fonts] ERROR: glBindTexture failed: 0x%X\n", error);
        glDeleteTextures(1, &texture_id);
        if (flip_vertical && final_data != rgba_data) {
            delete[] final_data;
        }
        return false;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, final_data);

    error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("[Fonts] ERROR: glTexImage2D failed: 0x%X\n", error);
        glDeleteTextures(1, &texture_id);
        if (flip_vertical && final_data != rgba_data) {
            delete[] final_data;
        }
        return false;
    }

    if (flip_vertical && final_data != rgba_data) {
        delete[] final_data;
    }

    *out_texture = texture_id;

    printf("[Fonts] Texture created successfully (ID: %d)\n", texture_id);
    return true;
}

void Fonts::CleanupTextures() {
    if (LogoTexture != 0) {
        glDeleteTextures(1, &LogoTexture);
        LogoTexture = 0;
        LogoMenuWidth = 0;
        LogoMenuHeight = 0;
    }
}