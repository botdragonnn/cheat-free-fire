#pragma once
#include <Math/Math.hpp>

struct Vector3
{
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
        };
        float data[3];
    };

    Vector3();
    Vector3(float data[]);
    Vector3(float value);
    Vector3(float x, float y);
    Vector3(float x, float y, float z);

    static Vector3 Zero();
    static Vector3 One();
    static Vector3 Right();
    static Vector3 Left();
    static Vector3 Up();
    static Vector3 Down();
    static Vector3 Forward();
    static Vector3 Backward();

    static float Angle(Vector3 a, Vector3 b);
    static Vector3 ClampMagnitude(Vector3 vector, float maxLength);
    static float Component(Vector3 a, Vector3 b);
    static Vector3 Cross(Vector3 lhs, Vector3 rhs);
    static float Distance(Vector3 a, Vector3 b);
    static float Dot(Vector3 lhs, Vector3 rhs);
    static Vector3 FromSpherical(float rad, float theta, float phi);
    static Vector3 Lerp(Vector3 a, Vector3 b, float t);
    static Vector3 LerpUnclamped(Vector3 a, Vector3 b, float t);
    static float Magnitude(Vector3 v);
    static Vector3 Max(Vector3 a, Vector3 b);
    static Vector3 Min(Vector3 a, Vector3 b);
    static Vector3 MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta);
    static Vector3 Normalized(Vector3 v);
    static Vector3 Orthogonal(Vector3 v);
    static void OrthoNormalize(Vector3& normal, Vector3& tangent, Vector3& binormal);
    static Vector3 Project(Vector3 a, Vector3 b);
    static Vector3 ProjectOnPlane(Vector3 vector, Vector3 planeNormal);
    static Vector3 Reflect(Vector3 vector, Vector3 planeNormal);
    static Vector3 Reject(Vector3 a, Vector3 b);
    static Vector3 RotateTowards(Vector3 current, Vector3 target, float maxRadiansDelta, float maxMagnitudeDelta);
    static Vector3 Scale(Vector3 a, Vector3 b);
    static Vector3 Slerp(Vector3 a, Vector3 b, float t);
    static Vector3 SlerpUnclamped(Vector3 a, Vector3 b, float t);
    static float SqrMagnitude(Vector3 v);
    static void ToSpherical(Vector3 vector, float& rad, float& theta, float& phi);

    Vector3& operator+=(float rhs);
    Vector3& operator-=(float rhs);
    Vector3& operator*=(float rhs);
    Vector3& operator/=(float rhs);
    Vector3& operator+=(Vector3 rhs);
    Vector3& operator-=(Vector3 rhs);
};

Vector3 operator-(Vector3 rhs);
Vector3 operator+(Vector3 lhs, float rhs);
Vector3 operator-(Vector3 lhs, float rhs);
Vector3 operator*(Vector3 lhs, float rhs);
Vector3 operator/(Vector3 lhs, float rhs);
Vector3 operator+(float lhs, Vector3 rhs);
Vector3 operator-(float lhs, Vector3 rhs);
Vector3 operator*(float lhs, Vector3 rhs);
Vector3 operator/(float lhs, Vector3 rhs);
Vector3 operator+(Vector3 lhs, Vector3 rhs);
Vector3 operator-(Vector3 lhs, Vector3 rhs);
Vector3 operator/(Vector3 lhs, Vector3 rhs);
bool operator==(Vector3 lhs, Vector3 rhs);
bool operator!=(Vector3 lhs, Vector3 rhs);