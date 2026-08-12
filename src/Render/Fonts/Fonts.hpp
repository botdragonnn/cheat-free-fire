#pragma once
#include "Includes.hpp"

namespace Fonts {
    //inline ImFont* FontAwesomeRegular = nullptr;
    inline ImFont* FontAwesomeSolid = nullptr;
    //inline ImFont* FontAwesomeBrands = nullptr;

    inline ImFont* Verdana = nullptr;
    inline ImFont* Gff = nullptr;
    inline ImFont* IconWeapon = nullptr;
    inline ImFont* InterBold = nullptr;
    inline ImFont* InterMedium = nullptr;
    inline ImFont* InterRegular = nullptr;

    inline GLuint LogoTexture = 0;
    inline int LogoMenuWidth = 0;
    inline int LogoMenuHeight = 0;

    void Initialize();
    void CleanupTextures();
    bool LoadTextureFromRawRGBA(const unsigned char* rgba_data, int width, int height, GLuint* out_texture, bool flip_vertical = true);
    bool LoadTextureFromPNG(const unsigned char* png_data, size_t png_size, GLuint* out_texture, int* out_width = nullptr, int* out_height = nullptr);
}