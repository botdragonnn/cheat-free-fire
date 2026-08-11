#include "Vector4.hpp"

Vector4::Vector4()
{
    x = 0;
    y = 0;
    z = 0;
    w = 0;
}

Vector4::Vector4(float f_x, float f_y, float f_z, float f_w)
{
    x = f_x;
    y = f_y;
    z = f_z;
    w = f_w;
}

Vector4::Vector4(const float* f_n)
{
    x = f_n[0];
    y = f_n[1];
    z = f_n[2];
    w = f_n[3];
}

bool Vector4::is_valid() const
{
    auto is_finite = [](float f) -> bool {
        return f == f && f <= 1e30f && f >= -1e30f;    
    };

    return is_finite(x) && is_finite(y) && is_finite(z) && is_finite(w);
}

float Vector4::length() const
{
    return Math::sqrt(x * x + y * y + z * z + w * w);
}

float Vector4::length_sqr() const
{
    return (x * x + y * y + z * z + w * w);
}

float Vector4::dist_to_sqr(const Vector4& v4_n) const
{
    Vector4 delta;
    delta.x = x - v4_n.x;
    delta.y = y - v4_n.y;
    delta.z = z - v4_n.z;
    delta.w = w - v4_n.w;
    return delta.length_sqr();
}

void Vector4::normalize()
{
    float f_length = this->length();
    if (f_length > Math::EPSILON) {
        float inv_length = 1.0f / f_length;
        x *= inv_length;
        y *= inv_length;
        z *= inv_length;
        w *= inv_length;
    }
}

Vector4& Vector4::operator=(const Vector4& v4_n)
{
    x = v4_n.x;
    y = v4_n.y;
    z = v4_n.z;
    w = v4_n.w;
    return *this;
}

Vector4 Vector4::operator-() const
{
    return Vector4(-x, -y, -z, -w);
}

Vector4 Vector4::operator+(const Vector4& v4_n) const
{
    return Vector4(x + v4_n.x, y + v4_n.y, z + v4_n.z, w + v4_n.w);
}

Vector4 Vector4::operator+(float f_n) const
{
    return Vector4(x + f_n, y + f_n, z + f_n, w + f_n);
}

Vector4 Vector4::operator-(const Vector4& v4_n) const
{
    return Vector4(x - v4_n.x, y - v4_n.y, z - v4_n.z, w - v4_n.w);
}

Vector4 Vector4::operator-(float f_n) const
{
    return Vector4(x - f_n, y - f_n, z - f_n, w - f_n);
}

Vector4 Vector4::operator*(float f_n) const
{
    return Vector4(x * f_n, y * f_n, z * f_n, w * f_n);
}

Vector4 Vector4::operator*(const Vector4& v4_n) const
{
    return Vector4(x * v4_n.x, y * v4_n.y, z * v4_n.z, w * v4_n.w);
}

Vector4 Vector4::operator/(float f_n) const
{
    if (Math::abs(f_n) > Math::EPSILON) {
        float inv_f = 1.0f / f_n;
        return Vector4(x * inv_f, y * inv_f, z * inv_f, w * inv_f);
    }
    return Vector4(0, 0, 0, 0);
}

Vector4 Vector4::operator/(const Vector4& v4_n) const
{
    return Vector4(
        (Math::abs(v4_n.x) > Math::EPSILON) ? x / v4_n.x : 0,
        (Math::abs(v4_n.y) > Math::EPSILON) ? y / v4_n.y : 0,
        (Math::abs(v4_n.z) > Math::EPSILON) ? z / v4_n.z : 0,
        (Math::abs(v4_n.w) > Math::EPSILON) ? w / v4_n.w : 0
    );
}

bool Vector4::operator==(const Vector4& v4_n) const
{
    return Math::approximately(x, v4_n.x) &&
        Math::approximately(y, v4_n.y) &&
        Math::approximately(z, v4_n.z) &&
        Math::approximately(w, v4_n.w);
}

bool Vector4::operator!=(const Vector4& v4_n) const
{
    return !(*this == v4_n);
}

Vector4& Vector4::operator+=(const Vector4& v4_n)
{
    x += v4_n.x;
    y += v4_n.y;
    z += v4_n.z;
    w += v4_n.w;
    return *this;
}

Vector4& Vector4::operator-=(const Vector4& v4_n)
{
    x -= v4_n.x;
    y -= v4_n.y;
    z -= v4_n.z;
    w -= v4_n.w;
    return *this;
}

Vector4& Vector4::operator*=(float f_n)
{
    x *= f_n;
    y *= f_n;
    z *= f_n;
    w *= f_n;
    return *this;
}

Vector4& Vector4::operator*=(const Vector4& v4_n)
{
    x *= v4_n.x;
    y *= v4_n.y;
    z *= v4_n.z;
    w *= v4_n.w;
    return *this;
}

Vector4& Vector4::operator/=(const Vector4& v4_n)
{
    if (Math::abs(v4_n.x) > Math::EPSILON) x /= v4_n.x;
    if (Math::abs(v4_n.y) > Math::EPSILON) y /= v4_n.y;
    if (Math::abs(v4_n.z) > Math::EPSILON) z /= v4_n.z;
    if (Math::abs(v4_n.w) > Math::EPSILON) w /= v4_n.w;
    return *this;
}

Vector4& Vector4::operator+=(float f_n)
{
    x += f_n;
    y += f_n;
    z += f_n;
    w += f_n;
    return *this;
}

Vector4& Vector4::operator/=(float f_n)
{
    if (Math::abs(f_n) > Math::EPSILON) {
        float inv_f = 1.0f / f_n;
        x *= inv_f;
        y *= inv_f;
        z *= inv_f;
        w *= inv_f;
    }
    return *this;
}

Vector4& Vector4::operator-=(float f_n)
{
    x -= f_n;
    y -= f_n;
    z -= f_n;
    w -= f_n;
    return *this;
}

float& Vector4::operator[](int i)
{
    return ((float*)this)[i];
}

float Vector4::operator[](int i) const
{
    return ((float*)this)[i];
}