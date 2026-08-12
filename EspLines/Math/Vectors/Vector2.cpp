#include "Vector2.hpp"

Vector2::Vector2() : X(0), Y(0) {}
Vector2::Vector2(float data[]) : X(data[0]), Y(data[1]) {}
Vector2::Vector2(float value) : X(value), Y(value) {}
Vector2::Vector2(float x, float y) : X(x), Y(y) {}

Vector2 Vector2::Zero() { return Vector2(0, 0); }
Vector2 Vector2::One() { return Vector2(1, 1); }
Vector2 Vector2::Right() { return Vector2(1, 0); }
Vector2 Vector2::Left() { return Vector2(-1, 0); }
Vector2 Vector2::Up() { return Vector2(0, 1); }
Vector2 Vector2::Down() { return Vector2(0, -1); }

float Vector2::Angle(Vector2 a, Vector2 b)
{
    float v = Dot(a, b) / (Magnitude(a) * Magnitude(b));
    v = Math::max(v, -1.0f);
    v = Math::min(v, 1.0f);
    return Math::acos(v);
}

Vector2 Vector2::ClampMagnitude(Vector2 vector, float maxLength)
{
    float length = Magnitude(vector);
    if (length > maxLength)
        vector *= maxLength / length;
    return vector;
}

float Vector2::Component(Vector2 a, Vector2 b)
{
    return Dot(a, b) / Magnitude(b);
}

float Vector2::Distance(Vector2 a, Vector2 b)
{
    return Magnitude(a - b);
}

float Vector2::Dot(Vector2 lhs, Vector2 rhs)
{
    return lhs.X * rhs.X + lhs.Y * rhs.Y;
}

Vector2 Vector2::FromPolar(float rad, float theta)
{
    Vector2 v;
    v.X = rad * Math::cos(theta);
    v.Y = rad * Math::sin(theta);
    return v;
}

Vector2 Vector2::Lerp(Vector2 a, Vector2 b, float t)
{
    if (t < 0) return a;
    else if (t > 1) return b;
    return LerpUnclamped(a, b, t);
}

Vector2 Vector2::LerpUnclamped(Vector2 a, Vector2 b, float t)
{
    return (b - a) * t + a;
}

float Vector2::Magnitude(Vector2 v)
{
    return Math::sqrt(SqrMagnitude(v));
}

Vector2 Vector2::Max(Vector2 a, Vector2 b)
{
    float x = a.X > b.X ? a.X : b.X;
    float y = a.Y > b.Y ? a.Y : b.Y;
    return Vector2(x, y);
}

Vector2 Vector2::Min(Vector2 a, Vector2 b)
{
    float x = a.X > b.X ? b.X : a.X;
    float y = a.Y > b.Y ? b.Y : a.Y;
    return Vector2(x, y);
}

Vector2 Vector2::MoveTowards(Vector2 current, Vector2 target, float maxDistanceDelta)
{
    Vector2 d = target - current;
    float m = Magnitude(d);
    if (m < maxDistanceDelta || m == 0)
        return target;
    return current + (d * maxDistanceDelta / m);
}

Vector2 Vector2::Normalized(Vector2 v)
{
    float mag = Magnitude(v);
    if (mag == 0)
        return Zero();
    return v / mag;
}

void Vector2::OrthoNormalize(Vector2& normal, Vector2& tangent)
{
    normal = Normalized(normal);
    tangent = Reject(tangent, normal);
    tangent = Normalized(tangent);
}

Vector2 Vector2::Project(Vector2 a, Vector2 b)
{
    float m = Magnitude(b);
    return Dot(a, b) / (m * m) * b;
}

Vector2 Vector2::Reflect(Vector2 vector, Vector2 planeNormal)
{
    return vector - 2 * Project(vector, planeNormal);
}

Vector2 Vector2::Reject(Vector2 a, Vector2 b)
{
    return a - Project(a, b);
}

Vector2 Vector2::RotateTowards(Vector2 current, Vector2 target, float maxRadiansDelta, float maxMagnitudeDelta)
{
    float magCur = Magnitude(current);
    float magTar = Magnitude(target);
    float newMag = magCur + maxMagnitudeDelta * ((magTar > magCur) - (magCur > magTar));
    newMag = Math::min(newMag, Math::max(magCur, magTar));
    newMag = Math::max(newMag, Math::min(magCur, magTar));

    float totalAngle = Angle(current, target) - maxRadiansDelta;
    if (totalAngle <= 0)
        return Normalized(target) * newMag;
    else if (totalAngle >= Math::PI)
        return Normalized(-target) * newMag;

    float axis = current.X * target.Y - current.Y * target.X;
    axis = axis / Math::abs(axis);
    if (!(1 - Math::abs(axis) < Math::EPSILON))
        axis = 1;

    current = Normalized(current);
    Vector2 newVector = current * Math::cos(maxRadiansDelta) + Vector2(-current.Y, current.X) * Math::sin(maxRadiansDelta) * axis;
    return newVector * newMag;
}

Vector2 Vector2::Scale(Vector2 a, Vector2 b)
{
    return Vector2(a.X * b.X, a.Y * b.Y);
}

Vector2 Vector2::Slerp(Vector2 a, Vector2 b, float t)
{
    if (t < 0) return a;
    else if (t > 1) return b;
    return SlerpUnclamped(a, b, t);
}

Vector2 Vector2::SlerpUnclamped(Vector2 a, Vector2 b, float t)
{
    float magA = Magnitude(a);
    float magB = Magnitude(b);
    a /= magA;
    b /= magB;
    float dot = Dot(a, b);
    dot = Math::max(dot, -1.0f);
    dot = Math::min(dot, 1.0f);
    float theta = Math::acos(dot) * t;
    Vector2 relativeVec = Normalized(b - a * dot);
    Vector2 newVec = a * Math::cos(theta) + relativeVec * Math::sin(theta);
    return newVec * (magA + (magB - magA) * t);
}

float Vector2::SqrMagnitude(Vector2 v)
{
    return v.X * v.X + v.Y * v.Y;
}

void Vector2::ToPolar(Vector2 vector, float& rad, float& theta)
{
    rad = Magnitude(vector);
    theta = Math::atan2(vector.Y, vector.X);
}

// Operator implementations
Vector2& Vector2::operator+=(float rhs) { X += rhs; Y += rhs; return *this; }
Vector2& Vector2::operator-=(float rhs) { X -= rhs; Y -= rhs; return *this; }
Vector2& Vector2::operator*=(float rhs) { X *= rhs; Y *= rhs; return *this; }
Vector2& Vector2::operator/=(float rhs) { X /= rhs; Y /= rhs; return *this; }
Vector2& Vector2::operator+=(Vector2 rhs) { X += rhs.X; Y += rhs.Y; return *this; }
Vector2& Vector2::operator-=(Vector2 rhs) { X -= rhs.X; Y -= rhs.Y; return *this; }

Vector2 operator-(Vector2 rhs) { return rhs * -1; }
Vector2 operator+(Vector2 lhs, float rhs) { return lhs += rhs; }
Vector2 operator-(Vector2 lhs, float rhs) { return lhs -= rhs; }
Vector2 operator*(Vector2 lhs, float rhs) { return lhs *= rhs; }
Vector2 operator/(Vector2 lhs, float rhs) { return lhs /= rhs; }
Vector2 operator+(float lhs, Vector2 rhs) { return rhs += lhs; }
Vector2 operator-(float lhs, Vector2 rhs) { return rhs = Vector2(lhs, lhs) - rhs; }
Vector2 operator*(float lhs, Vector2 rhs) { return rhs *= lhs; }
Vector2 operator/(float lhs, Vector2 rhs) { return rhs = Vector2(lhs, lhs) / rhs; }
Vector2 operator+(Vector2 lhs, Vector2 rhs) { return lhs += rhs; }
Vector2 operator-(Vector2 lhs, Vector2 rhs) { return lhs -= rhs; }
Vector2 operator/(Vector2 lhs, Vector2 rhs) {
    return Vector2(
        (Math::abs(rhs.X) > Math::EPSILON) ? lhs.X / rhs.X : 0.0f,
        (Math::abs(rhs.Y) > Math::EPSILON) ? lhs.Y / rhs.Y : 0.0f
    );
}
bool operator==(Vector2 lhs, Vector2 rhs) {
    return Math::approximately(lhs.X, rhs.X) && Math::approximately(lhs.Y, rhs.Y);
}

bool operator!=(Vector2 lhs, Vector2 rhs) {
    return !(lhs == rhs);
}