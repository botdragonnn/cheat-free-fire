#pragma once

namespace Math {
    extern const float PI;
    extern const float PI_2;
    extern const float DEG2RAD;
    extern const float RAD2DEG;
    extern const float EPSILON;

    float abs(float value);
    float fabsf(float x);
    float (max)(float a, float b);
    float (min)(float a, float b);
    bool approximately(float a, float b);

    float sqrt(float value);
    float sqrtf(float x);

    float fmodf(float x, float y);

    float sin(float x);
    float sinf(float x);
    float cos(float x);
    float cosf(float x);

    float atan2(float y, float x);
    float atan2f(float y, float x);

    float acos(float x);
    float acosf(float x);

    float asinf(float x);
}
