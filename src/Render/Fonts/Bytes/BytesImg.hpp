#pragma once

const int LogoWidth = 250;
const int LogoHeight = 250;

#include "../../../../logo.c"

// Alias rawData as LogoMenuRawRGBA to keep everything else compiling perfectly
const unsigned char* LogoMenuRawRGBA = rawData;
const size_t LogoMenuRawRGBASize = sizeof(rawData);