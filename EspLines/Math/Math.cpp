#include "Math.hpp"

#pragma once

namespace Math {
    const float PI = 3.14159265358979323846f;
    const float PI_2 = PI / 2.0f;
    const float DEG2RAD = PI / 180.0f;
    const float RAD2DEG = 180.0f / PI;
    const float EPSILON = 1e-5f;

    float abs(float value) {
        return value < 0 ? -value : value;
    }

    float fabsf(float x) {
        return x < 0.0f ? -x : x;
    }

    float max(float a, float b) {
        return a > b ? a : b;
    }

    float min(float a, float b) {
        return a < b ? a : b;
    }

    bool approximately(float a, float b) {
        return abs(a - b) < EPSILON;
    }

    float sqrt(float value) {
        if (value <= 0) return 0;

        float x = value;
        float y = 1.0f;

        for (int i = 0; i < 10; ++i) {
            x = (x + y) * 0.5f;
            y = value / x;
        }
        return x;
    }

    float sqrtf(float x) {
        if (x == 0.0f) return 0.0f;

        float guess = x;
        for (int i = 0; i < 10; i++) {
            guess = 0.5f * (guess + x / guess);
        }
        return guess;
    }

    float fmodf(float x, float y) {
        if (y == 0.0f) return 0.0f;
        return x - y * static_cast<int>(x / y);
    }

    float sin(float x) {
        while (x > PI) x -= 2 * PI;
        while (x < -PI) x += 2 * PI;

        float x2 = x * x;
        float x3 = x * x2;
        float x5 = x3 * x2;
        float x7 = x5 * x2;

        return x - x3 / 6.0f + x5 / 120.0f - x7 / 5040.0f;
    }

    float sinf(float x) {
        x = fmodf(x, 2.0f * PI);
        if (x > PI) x -= 2.0f * PI;
        if (x < -PI) x += 2.0f * PI;

        float x2 = x * x;
        float x3 = x * x2;
        float x5 = x3 * x2;
        float x7 = x5 * x2;

        return x - x3 / 6.0f + x5 / 120.0f - x7 / 5040.0f;
    }

    float cos(float x) {
        return sin(PI / 2 - x);
    }

    float cosf(float x) {
        x = fmodf(x, 2.0f * PI);
        if (x > PI) x -= 2.0f * PI;
        if (x < -PI) x += 2.0f * PI;

        float x2 = x * x;
        float x4 = x2 * x2;
        float x6 = x4 * x2;

        return 1.0f - x2 / 2.0f + x4 / 24.0f - x6 / 720.0f;
    }

    float atan2(float y, float x) {
        if (x == 0) {
            if (y > 0) return PI / 2;
            if (y < 0) return -PI / 2;
            return 0;
        }

        float atan = y / x;
        if (x < 0) {
            if (y >= 0) return atan + PI;
            return atan - PI;
        }
        return atan;
    }

    float atan2f(float y, float x) {
        if (x == 0.0f) {
            if (y > 0.0f) return PI_2;
            if (y < 0.0f) return -PI_2;
            return 0.0f;
        }

        float ratio = y / x;
        float angle = ratio - (ratio * ratio * ratio) / 3.0f + (ratio * ratio * ratio * ratio * ratio) / 5.0f;

        if (x < 0.0f) {
            if (y >= 0.0f) angle += PI;
            else angle -= PI;
        }

        return angle;
    }

    float acos(float x) {
        if (x >= 1.0f) return 0;
        if (x <= -1.0f) return PI;

        return PI / 2 - x - (x * x * x) / 6.0f - (3.0f * x * x * x * x * x) / 40.0f;
    }

    float acosf(float x) {
        if (x >= 1.0f) return 0.0f;
        if (x <= -1.0f) return PI;

        return PI_2 - atan2f(x, sqrtf(1.0f - x * x));
    }

    float asinf(float x) {
        if (x >= 1.0f) return PI_2;
        if (x <= -1.0f) return -PI_2;

        return x + (x * x * x) / 6.0f + (3.0f * x * x * x * x * x) / 40.0f;
    }
}
