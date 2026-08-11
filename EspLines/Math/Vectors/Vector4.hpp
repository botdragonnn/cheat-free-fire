#pragma once
#include <Math/Math.hpp>

class Vector4
{
public:
    float x, y, z, w;

    Vector4();
    Vector4(float f_x, float f_y, float f_z, float f_w);
    Vector4(const float* f_n);

    bool is_valid() const;
    float length() const;
    float length_sqr() const;
    float dist_to_sqr(const Vector4& v4_n) const;
    void normalize();

    Vector4& operator=(const Vector4& v4_n);
    Vector4 operator-() const;
    Vector4 operator+(const Vector4& v4_n) const;
    Vector4 operator+(float f_n) const;
    Vector4 operator-(const Vector4& v4_n) const;
    Vector4 operator-(float f_n) const;
    Vector4 operator*(float f_n) const;
    Vector4 operator*(const Vector4& v4_n) const;
    Vector4 operator/(float f_n) const;
    Vector4 operator/(const Vector4& v4_n) const;
    bool operator==(const Vector4& v4_n) const;
    bool operator!=(const Vector4& v4_n) const;
    Vector4& operator+=(const Vector4& v4_n);
    Vector4& operator-=(const Vector4& v4_n);
    Vector4& operator*=(float f_n);
    Vector4& operator*=(const Vector4& v4_n);
    Vector4& operator/=(const Vector4& v4_n);
    Vector4& operator+=(float f_n);
    Vector4& operator/=(float f_n);
    Vector4& operator-=(float f_n);
    float& operator[](int i);
    float operator[](int i) const;
};