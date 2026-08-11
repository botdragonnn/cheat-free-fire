#include "Vector3.hpp"

Vector3::Vector3() : X(0), Y(0), Z(0) {}
Vector3::Vector3(float data[]) : X(data[0]), Y(data[1]), Z(data[2]) {}
Vector3::Vector3(float value) : X(value), Y(value), Z(value) {}
Vector3::Vector3(float x, float y) : X(x), Y(y), Z(0) {}
Vector3::Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

Vector3 Vector3::Zero() { return Vector3(0, 0, 0); }
Vector3 Vector3::One() { return Vector3(1, 1, 1); }
Vector3 Vector3::Right() { return Vector3(1, 0, 0); }
Vector3 Vector3::Left() { return Vector3(-1, 0, 0); }
Vector3 Vector3::Up() { return Vector3(0, 1, 0); }
Vector3 Vector3::Down() { return Vector3(0, -1, 0); }
Vector3 Vector3::Forward() { return Vector3(0, 0, 1); }
Vector3 Vector3::Backward() { return Vector3(0, 0, -1); }

float Vector3::Angle(Vector3 a, Vector3 b)
{
    float v = Dot(a, b) / (Magnitude(a) * Magnitude(b));
    v = Math::max(v, -1.0f);
    v = Math::min(v, 1.0f);
    return Math::acos(v);
}

Vector3 Vector3::ClampMagnitude(Vector3 vector, float maxLength)
{
    float length = Magnitude(vector);
    if (length > maxLength)
        vector *= maxLength / length;
    return vector;
}

float Vector3::Component(Vector3 a, Vector3 b)
{
    return Dot(a, b) / Magnitude(b);
}

Vector3 Vector3::Cross(Vector3 lhs, Vector3 rhs)
{
    float x = lhs.Y * rhs.Z - lhs.Z * rhs.Y;
    float y = lhs.Z * rhs.X - lhs.X * rhs.Z;
    float z = lhs.X * rhs.Y - lhs.Y * rhs.X;
    return Vector3(x, y, z);
}

float Vector3::Distance(Vector3 a, Vector3 b)
{
    return Magnitude(a - b);
}

float Vector3::Dot(Vector3 lhs, Vector3 rhs)
{
    return lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z;
}

Vector3 Vector3::FromSpherical(float rad, float theta, float phi)
{
    Vector3 v;
    v.X = rad * Math::sin(theta) * Math::cos(phi);
    v.Y = rad * Math::sin(theta) * Math::sin(phi);
    v.Z = rad * Math::cos(theta);
    return v;
}

Vector3 Vector3::Lerp(Vector3 a, Vector3 b, float t)
{
    if (t < 0) return a;
    else if (t > 1) return b;
    return LerpUnclamped(a, b, t);
}

Vector3 Vector3::LerpUnclamped(Vector3 a, Vector3 b, float t)
{
    return (b - a) * t + a;
}

float Vector3::Magnitude(Vector3 v)
{
    return Math::sqrt(SqrMagnitude(v));
}

Vector3 Vector3::Max(Vector3 a, Vector3 b)
{
    float x = a.X > b.X ? a.X : b.X;
    float y = a.Y > b.Y ? a.Y : b.Y;
    float z = a.Z > b.Z ? a.Z : b.Z;
    return Vector3(x, y, z);
}

Vector3 Vector3::Min(Vector3 a, Vector3 b)
{
    float x = a.X > b.X ? b.X : a.X;
    float y = a.Y > b.Y ? b.Y : a.Y;
    float z = a.Z > b.Z ? b.Z : a.Z;
    return Vector3(x, y, z);
}

Vector3 Vector3::MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta)
{
    Vector3 d = target - current;
    float m = Magnitude(d);
    if (m < maxDistanceDelta || m == 0)
        return target;
    return current + (d * maxDistanceDelta / m);
}

Vector3 Vector3::Normalized(Vector3 v)
{
    float mag = Magnitude(v);
    if (mag == 0)
        return Zero();
    return v / mag;
}

Vector3 Vector3::Orthogonal(Vector3 v)
{
    return v.Z < v.X ? Vector3(v.Y, -v.X, 0) : Vector3(0, -v.Z, v.Y);
}

void Vector3::OrthoNormalize(Vector3& normal, Vector3& tangent, Vector3& binormal)
{
    normal = Normalized(normal);
    tangent = ProjectOnPlane(tangent, normal);
    tangent = Normalized(tangent);
    binormal = ProjectOnPlane(binormal, tangent);
    binormal = ProjectOnPlane(binormal, normal);
    binormal = Normalized(binormal);
}

Vector3 Vector3::Project(Vector3 a, Vector3 b)
{
    float m = Magnitude(b);
    return Dot(a, b) / (m * m) * b;
}

Vector3 Vector3::ProjectOnPlane(Vector3 vector, Vector3 planeNormal)
{
    return Reject(vector, planeNormal);
}

Vector3 Vector3::Reflect(Vector3 vector, Vector3 planeNormal)
{
    return vector - 2 * Project(vector, planeNormal);
}

Vector3 Vector3::Reject(Vector3 a, Vector3 b)
{
    return a - Project(a, b);
}

Vector3 Vector3::RotateTowards(Vector3 current, Vector3 target, float maxRadiansDelta, float maxMagnitudeDelta)
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

    Vector3 axis = Cross(current, target);
    float magAxis = Magnitude(axis);
    if (magAxis == 0)
        axis = Normalized(Cross(current, current + Vector3(3.95f, 5.32f, -4.24f)));
    else
        axis /= magAxis;

    current = Normalized(current);
    Vector3 newVector = current * Math::cos(maxRadiansDelta) + Cross(axis, current) * Math::sin(maxRadiansDelta);
    return newVector * newMag;
}

Vector3 Vector3::Scale(Vector3 a, Vector3 b)
{
    return Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
}

Vector3 Vector3::Slerp(Vector3 a, Vector3 b, float t)
{
    if (t < 0) return a;
    else if (t > 1) return b;
    return SlerpUnclamped(a, b, t);
}

Vector3 Vector3::SlerpUnclamped(Vector3 a, Vector3 b, float t)
{
    float magA = Magnitude(a);
    float magB = Magnitude(b);
    a /= magA;
    b /= magB;
    float dot = Dot(a, b);
    dot = Math::max(dot, -1.0f);
    dot = Math::min(dot, 1.0f);
    float theta = Math::acos(dot) * t;
    Vector3 relativeVec = Normalized(b - a * dot);
    Vector3 newVec = a * Math::cos(theta) + relativeVec * Math::sin(theta);
    return newVec * (magA + (magB - magA) * t);
}

float Vector3::SqrMagnitude(Vector3 v)
{
    return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
}

void Vector3::ToSpherical(Vector3 vector, float& rad, float& theta, float& phi)
{
    rad = Magnitude(vector);
    float v = vector.Z / rad;
    v = Math::max(v, -1.0f);
    v = Math::min(v, 1.0f);
    theta = Math::acos(v);
    phi = Math::atan2(vector.Y, vector.X);
}

Vector3& Vector3::operator+=(float rhs) { X += rhs; Y += rhs; Z += rhs; return *this; }
Vector3& Vector3::operator-=(float rhs) { X -= rhs; Y -= rhs; Z -= rhs; return *this; }
Vector3& Vector3::operator*=(float rhs) { X *= rhs; Y *= rhs; Z *= rhs; return *this; }
Vector3& Vector3::operator/=(float rhs) { X /= rhs; Y /= rhs; Z /= rhs; return *this; }
Vector3& Vector3::operator+=(Vector3 rhs) { X += rhs.X; Y += rhs.Y; Z += rhs.Z; return *this; }
Vector3& Vector3::operator-=(Vector3 rhs) { X -= rhs.X; Y -= rhs.Y; Z -= rhs.Z; return *this; }

Vector3 operator-(Vector3 rhs) { return rhs * -1; }
Vector3 operator+(Vector3 lhs, float rhs) { return lhs += rhs; }
Vector3 operator-(Vector3 lhs, float rhs) { return lhs -= rhs; }
Vector3 operator*(Vector3 lhs, float rhs) { return lhs *= rhs; }
Vector3 operator/(Vector3 lhs, float rhs) { return lhs /= rhs; }
Vector3 operator+(float lhs, Vector3 rhs) { return rhs += lhs; }
Vector3 operator-(float lhs, Vector3 rhs) { return Vector3(lhs, lhs, lhs) - rhs; }
Vector3 operator*(float lhs, Vector3 rhs) { return rhs *= lhs; }
Vector3 operator/(float lhs, Vector3 rhs) { return Vector3(lhs, lhs, lhs) / rhs; }
Vector3 operator+(Vector3 lhs, Vector3 rhs) { return lhs += rhs; }
Vector3 operator-(Vector3 lhs, Vector3 rhs) { return lhs -= rhs; }
Vector3 operator/(Vector3 lhs, Vector3 rhs) {
    return Vector3(
        (Math::abs(rhs.X) > Math::EPSILON) ? lhs.X / rhs.X : 0.0f,
        (Math::abs(rhs.Y) > Math::EPSILON) ? lhs.Y / rhs.Y : 0.0f,
        (Math::abs(rhs.Z) > Math::EPSILON) ? lhs.Z / rhs.Z : 0.0f
    );
}
bool operator==(Vector3 lhs, Vector3 rhs)
{
    return Math::approximately(lhs.X, rhs.X) &&
        Math::approximately(lhs.Y, rhs.Y) &&
        Math::approximately(lhs.Z, rhs.Z);
}

bool operator!=(Vector3 lhs, Vector3 rhs)
{
    return !(lhs == rhs);
}