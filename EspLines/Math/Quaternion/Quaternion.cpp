#include "Quaternion.hpp"

#define SMALL_float 0.0000000001f

Vector3 Quaternion::RotateVector(const Vector3& v) const {
    float num = X * 2.0f;
    float num2 = Y * 2.0f;
    float num3 = Z * 2.0f;
    float num4 = X * num;
    float num5 = Y * num2;
    float num6 = Z * num3;
    float num7 = X * num2;
    float num8 = X * num3;
    float num9 = Y * num3;
    float num10 = W * num;
    float num11 = W * num2;
    float num12 = W * num3;

    return Vector3(
        (1.0f - (num5 + num6)) * v.X + (num7 - num12) * v.Y + (num8 + num11) * v.Z,
        (num7 + num12) * v.X + (1.0f - (num4 + num6)) * v.Y + (num9 - num10) * v.Z,
        (num8 - num11) * v.X + (num9 + num10) * v.Y + (1.0f - (num4 + num5)) * v.Z
    );
}

Quaternion::Quaternion() : X(0), Y(0), Z(0), W(1) {}
Quaternion::Quaternion(float data[]) : X(data[0]), Y(data[1]), Z(data[2]), W(data[3]) {}
Quaternion::Quaternion(Vector3 vector, float scalar) : X(vector.X), Y(vector.Y), Z(vector.Z), W(scalar) {}
Quaternion::Quaternion(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

Quaternion Quaternion::Identity() { return Quaternion(0, 0, 0, 1); }

Vector3 Quaternion::RotatePoint(const Quaternion& rotation, const Vector3& point) {
    Quaternion p(0.0f, point.X, point.Y, point.Z);
    Quaternion conjugate = Quaternion::Conjugate(rotation);
    Quaternion rotatedPoint = rotation * p * conjugate;
    return Vector3(rotatedPoint.X, rotatedPoint.Y, rotatedPoint.Z);
}

float Quaternion::Angle(Quaternion a, Quaternion b)
{
    float dot = Dot(a, b);
    float absDot = dot > 0.0f ? dot : -dot;
    absDot = absDot > 1.0f ? 1.0f : absDot;
    return Math::acosf(absDot) * 2.0f;
}

Quaternion Quaternion::Conjugate(Quaternion rotation)
{
    return Quaternion(-rotation.X, -rotation.Y, -rotation.Z, rotation.W);
}

float Quaternion::Dot(Quaternion lhs, Quaternion rhs)
{
    return lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z + lhs.W * rhs.W;
}

Quaternion Quaternion::ExpMapSmooth(const Quaternion& start, const Quaternion& end, float t) {
    float factor = 1.0f / (1.0f + t);
    return start + (end - start) * factor;
}

Quaternion Quaternion::FromAngleAxis(float angle, Vector3 axis)
{
    Quaternion q;
    float m = Math::sqrtf(axis.X * axis.X + axis.Y * axis.Y + axis.Z * axis.Z);
    if (m > 0.0f) {
        float s = Math::sinf(angle * 0.5f) / m;
        q.X = axis.X * s;
        q.Y = axis.Y * s;
        q.Z = axis.Z * s;
        q.W = Math::cosf(angle * 0.5f);
    }
    else {
        q.W = 1.0f;
    }
    return q;
}

Quaternion Quaternion::FromEuler(Vector3 rotation)
{
    return FromEuler(rotation.X, rotation.Y, rotation.Z);
}

Quaternion Quaternion::FromEuler(float x, float y, float z)
{
    float cx = Math::cosf(x * 0.5f);
    float cy = Math::cosf(y * 0.5f);
    float cz = Math::cosf(z * 0.5f);
    float sx = Math::sinf(x * 0.5f);
    float sy = Math::sinf(y * 0.5f);
    float sz = Math::sinf(z * 0.5f);

    Quaternion q;
    q.X = cx * sy * sz + cy * cz * sx;
    q.Y = cx * cz * sy - cy * sx * sz;
    q.Z = cx * cy * sz - cz * sx * sy;
    q.W = sx * sy * sz + cx * cy * cz;
    return q;
}

Quaternion Quaternion::FromToRotation(Vector3 fromVector, Vector3 toVector)
{
    float dot = Vector3::Dot(fromVector, toVector);
    float k = Math::sqrtf(Vector3::SqrMagnitude(fromVector) * Vector3::SqrMagnitude(toVector));

    if (Math::fabsf(dot / k + 1.0f) < 0.00001f)
    {
        Vector3 ortho = Vector3::Orthogonal(fromVector);
        return Quaternion(Vector3::Normalized(ortho).X, Vector3::Normalized(ortho).Y, Vector3::Normalized(ortho).Z, 0);
    }

    Vector3 cross = Vector3::Cross(fromVector, toVector);
    return Normalized(Quaternion(cross.X, cross.Y, cross.Z, dot + k));
}

Quaternion Quaternion::Inverse(Quaternion rotation)
{
    float n = Norm(rotation);
    if (n > 0.0f) {
        return Conjugate(rotation) / (n * n);
    }
    return Identity();
}

Quaternion Quaternion::Lerp(Quaternion a, Quaternion b, float t)
{
    if (t < 0.0f) return Normalized(a);
    else if (t > 1.0f) return Normalized(b);
    return LerpUnclamped(a, b, t);
}

Quaternion Quaternion::LerpUnclamped(Quaternion a, Quaternion b, float t)
{
    Quaternion quaternion;
    if (Dot(a, b) >= 0.0f)
        quaternion = a * (1.0f - t) + b * t;
    else
        quaternion = a * (1.0f - t) - b * t;
    return Normalized(quaternion);
}

Quaternion Quaternion::LookRotation(Vector3 forward)
{
    return LookRotation(forward, Vector3(0, 1, 0));
}

Quaternion Quaternion::LookRotation(Vector3 forward, Vector3 upwards)
{
    forward = Vector3::Normalized(forward);
    upwards = Vector3::Normalized(upwards);

    if (Vector3::SqrMagnitude(forward) < SMALL_float || Vector3::SqrMagnitude(upwards) < SMALL_float)
        return Quaternion::Identity();

    if (1.0f - Math::fabsf(Vector3::Dot(forward, upwards)) < SMALL_float)
        return FromToRotation(Vector3(0, 0, 1), forward);

    Vector3 right = Vector3::Normalized(Vector3::Cross(upwards, forward));
    upwards = Vector3::Cross(forward, right);

    Quaternion quaternion;
    float radicand = right.X + upwards.Y + forward.Z;

    if (radicand > 0.0f)
    {
        quaternion.W = Math::sqrtf(1.0f + radicand) * 0.5f;
        float recip = 1.0f / (4.0f * quaternion.W);
        quaternion.X = (upwards.Z - forward.Y) * recip;
        quaternion.Y = (forward.X - right.Z) * recip;
        quaternion.Z = (right.Y - upwards.X) * recip;
    }
    else if (right.X >= upwards.Y && right.X >= forward.Z)
    {
        quaternion.X = Math::sqrtf(1.0f + right.X - upwards.Y - forward.Z) * 0.5f;
        float recip = 1.0f / (4.0f * quaternion.X);
        quaternion.W = (upwards.Z - forward.Y) * recip;
        quaternion.Z = (forward.X + right.Z) * recip;
        quaternion.Y = (right.Y + upwards.X) * recip;
    }
    else if (upwards.Y > forward.Z)
    {
        quaternion.Y = Math::sqrtf(1.0f - right.X + upwards.Y - forward.Z) * 0.5f;
        float recip = 1.0f / (4.0f * quaternion.Y);
        quaternion.Z = (upwards.Z + forward.Y) * recip;
        quaternion.W = (forward.X - right.Z) * recip;
        quaternion.X = (right.Y + upwards.X) * recip;
    }
    else
    {
        quaternion.Z = Math::sqrtf(1.0f - right.X - upwards.Y + forward.Z) * 0.5f;
        float recip = 1.0f / (4.0f * quaternion.Z);
        quaternion.Y = (upwards.Z + forward.Y) * recip;
        quaternion.X = (forward.X + right.Z) * recip;
        quaternion.W = (right.Y - upwards.X) * recip;
    }
    return quaternion;
}

float Quaternion::Norm(Quaternion rotation)
{
    return Math::sqrtf(rotation.X * rotation.X + rotation.Y * rotation.Y +
        rotation.Z * rotation.Z + rotation.W * rotation.W);
}

Quaternion Quaternion::Normalized(Quaternion rotation)
{
    float n = Norm(rotation);
    if (n > 0.0f) {
        return rotation / n;
    }
    return Identity();
}

//static Quaternion Normalized(const Quaternion& rotation) {
//    float norm = Norm(rotation);
//    return Quaternion(rotation.X / norm, rotation.Y / norm, rotation.Z / norm, rotation.W / norm);
//}

Quaternion Quaternion::RotateTowards(Quaternion from, Quaternion to, float maxRadiansDelta)
{
    float angle = Quaternion::Angle(from, to);
    if (angle == 0.0f)
        return to;

    float t = maxRadiansDelta / angle;
    t = t > 1.0f ? 1.0f : t;
    return Quaternion::SlerpUnclamped(from, to, t);
}

float Quaternion::SmoothStep(float edge0, float edge1, float x) {
    x = (x - edge0) / (edge1 - edge0);
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    return x * x * (3.0f - 2.0f * x);
}

Quaternion Quaternion::Slerp(Quaternion a, Quaternion b, float t)
{
    if (t < 0.0f) return Normalized(a);
    else if (t > 1.0f) return Normalized(b);
    return SlerpUnclamped(a, b, t);
}

Quaternion Quaternion::SlerpUnclamped(Quaternion a, Quaternion b, float t)
{
    float n3 = Dot(a, b);
    bool flag = false;

    if (n3 < 0.0f)
    {
        flag = true;
        n3 = -n3;
    }

    float n2, n1;

    if (n3 > 0.999999f)
    {
        n2 = 1.0f - t;
        n1 = flag ? -t : t;
    }
    else
    {
        float n4 = Math::acosf(n3);
        float n5 = 1.0f / Math::sinf(n4);
        n2 = Math::sinf((1.0f - t) * n4) * n5;
        n1 = flag ? -Math::sinf(t * n4) * n5 : Math::sinf(t * n4) * n5;
    }

    Quaternion quaternion;
    quaternion.X = (n2 * a.X) + (n1 * b.X);
    quaternion.Y = (n2 * a.Y) + (n1 * b.Y);
    quaternion.Z = (n2 * a.Z) + (n1 * b.Z);
    quaternion.W = (n2 * a.W) + (n1 * b.W);
    return Normalized(quaternion);
}

void Quaternion::ToAngleAxis(Quaternion rotation, float& angle, Vector3& axis)
{
    if (rotation.W > 1.0f)
        rotation = Normalized(rotation);

    angle = 2.0f * Math::acosf(rotation.W);
    float s = Math::sqrtf(1.0f - rotation.W * rotation.W);

    if (s < 0.00001f) {
        axis.X = 1.0f;
        axis.Y = 0.0f;
        axis.Z = 0.0f;
    }
    else {
        axis.X = rotation.X / s;
        axis.Y = rotation.Y / s;
        axis.Z = rotation.Z / s;
    }
}

Vector3 Quaternion::ToEuler(Quaternion rotation)
{
    float sqw = rotation.W * rotation.W;
    float sqx = rotation.X * rotation.X;
    float sqy = rotation.Y * rotation.Y;
    float sqz = rotation.Z * rotation.Z;

    float test = rotation.X * rotation.W - rotation.Y * rotation.Z;
    Vector3 v;

    if (test > 0.4995f * (sqx + sqy + sqz + sqw))
    {
        v.Y = 2.0f * Math::atan2f(rotation.Y, rotation.X);
        v.X = Math::PI_2;
        v.Z = 0.0f;
        return v;
    }

    if (test < -0.4995f * (sqx + sqy + sqz + sqw))
    {
        v.Y = -2.0f * Math::atan2f(rotation.Y, rotation.X);
        v.X = -Math::PI_2;
        v.Z = 0.0f;
        return v;
    }

    v.Y = Math::atan2f(2.0f * rotation.W * rotation.Y + 2.0f * rotation.Z * rotation.X,
        1.0f - 2.0f * (rotation.X * rotation.X + rotation.Y * rotation.Y));

    v.X = Math::asinf(2.0f * (rotation.W * rotation.X - rotation.Y * rotation.Z));

    v.Z = Math::atan2f(2.0f * rotation.W * rotation.Z + 2.0f * rotation.X * rotation.Y,
        1.0f - 2.0f * (rotation.Z * rotation.Z + rotation.X * rotation.X));

    return v;
}

Quaternion& Quaternion::operator+=(float rhs)
{
    X += rhs; Y += rhs; Z += rhs; W += rhs;
    return *this;
}

Quaternion& Quaternion::operator-=(float rhs)
{
    X -= rhs; Y -= rhs; Z -= rhs; W -= rhs;
    return *this;
}

Quaternion& Quaternion::operator*=(float rhs)
{
    X *= rhs; Y *= rhs; Z *= rhs; W *= rhs;
    return *this;
}

Quaternion& Quaternion::operator/=(float rhs)
{
    if (rhs != 0.0f) {
        X /= rhs; Y /= rhs; Z /= rhs; W /= rhs;
    }
    return *this;
}

Quaternion& Quaternion::operator+=(Quaternion rhs)
{
    X += rhs.X; Y += rhs.Y; Z += rhs.Z; W += rhs.W;
    return *this;
}

Quaternion& Quaternion::operator-=(Quaternion rhs)
{
    X -= rhs.X; Y -= rhs.Y; Z -= rhs.Z; W -= rhs.W;
    return *this;
}

Quaternion& Quaternion::operator*=(Quaternion rhs)
{
    float w = W * rhs.W - X * rhs.X - Y * rhs.Y - Z * rhs.Z;
    float x = X * rhs.W + W * rhs.X + Y * rhs.Z - Z * rhs.Y;
    float y = W * rhs.Y - X * rhs.Z + Y * rhs.W + Z * rhs.X;
    float z = W * rhs.Z + X * rhs.Y - Y * rhs.X + Z * rhs.W;

    X = x; Y = y; Z = z; W = w;
    return *this;
}

Quaternion operator-(Quaternion rhs) { return rhs * -1.0f; }
Quaternion operator+(Quaternion lhs, float rhs) { return lhs += rhs; }
Quaternion operator-(Quaternion lhs, float rhs) { return lhs -= rhs; }
Quaternion operator*(Quaternion lhs, float rhs) { return lhs *= rhs; }
Quaternion operator/(Quaternion lhs, float rhs) { return lhs /= rhs; }
Quaternion operator+(float lhs, Quaternion rhs) { return rhs += lhs; }
Quaternion operator-(float lhs, Quaternion rhs) { return Quaternion(lhs, lhs, lhs, lhs) - rhs; }
Quaternion operator*(float lhs, Quaternion rhs) { return rhs *= lhs; }
Quaternion operator/(float lhs, Quaternion rhs) { return Quaternion(lhs / rhs.X, lhs / rhs.Y, lhs / rhs.Z, lhs / rhs.W); }
Quaternion operator+(Quaternion lhs, Quaternion rhs) { return lhs += rhs; }
Quaternion operator-(Quaternion lhs, Quaternion rhs) { return lhs -= rhs; }
Quaternion operator*(Quaternion lhs, Quaternion rhs) { return lhs *= rhs; }

Vector3 operator*(Quaternion lhs, Vector3 rhs)
{
    Vector3 u(lhs.X, lhs.Y, lhs.Z);
    float s = lhs.W;
    float dot = Vector3::Dot(u, rhs);

    return u * (dot * 2.0f) +
        rhs * (s * s - Vector3::Dot(u, u)) +
        Vector3::Cross(u, rhs) * (2.0f * s);
}

bool operator==(Quaternion lhs, Quaternion rhs)
{
    return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z && lhs.W == rhs.W;
}

bool operator!=(Quaternion lhs, Quaternion rhs)
{
    return !(lhs == rhs);
}