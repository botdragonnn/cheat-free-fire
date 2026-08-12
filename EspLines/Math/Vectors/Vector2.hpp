#pragma once
#include <Math/Math.hpp>

struct Vector2
{
    union
    {
        struct
        {
            float X;
            float Y;
        };
        float data[2];
    };

    Vector2();
    Vector2(float data[]);
    Vector2(float value);
    Vector2(float x, float y);

    static Vector2 Zero();
    static Vector2 One();
    static Vector2 Right();
    static Vector2 Left();
    static Vector2 Up();
    static Vector2 Down();

    static float Angle(Vector2 a, Vector2 b);
    static Vector2 ClampMagnitude(Vector2 vector, float maxLength);
    static float Component(Vector2 a, Vector2 b);
    static float Distance(Vector2 a, Vector2 b);
    static float Dot(Vector2 lhs, Vector2 rhs);
    static Vector2 FromPolar(float rad, float theta);
    static Vector2 Lerp(Vector2 a, Vector2 b, float t);
    static Vector2 LerpUnclamped(Vector2 a, Vector2 b, float t);
    static float Magnitude(Vector2 v);
    static Vector2 Max(Vector2 a, Vector2 b);
    static Vector2 Min(Vector2 a, Vector2 b);
    static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDistanceDelta);
    static Vector2 Normalized(Vector2 v);
    static void OrthoNormalize(Vector2& normal, Vector2& tangent);
    static Vector2 Project(Vector2 a, Vector2 b);
    static Vector2 Reflect(Vector2 vector, Vector2 line);
    static Vector2 Reject(Vector2 a, Vector2 b);
    static Vector2 RotateTowards(Vector2 current, Vector2 target, float maxRadiansDelta, float maxMagnitudeDelta);
    static Vector2 Scale(Vector2 a, Vector2 b);
    static Vector2 Slerp(Vector2 a, Vector2 b, float t);
    static Vector2 SlerpUnclamped(Vector2 a, Vector2 b, float t);
    static float SqrMagnitude(Vector2 v);
    static void ToPolar(Vector2 vector, float& rad, float& theta);

    Vector2& operator+=(float rhs);
    Vector2& operator-=(float rhs);
    Vector2& operator*=(float rhs);
    Vector2& operator/=(float rhs);
    Vector2& operator+=(Vector2 rhs);
    Vector2& operator-=(Vector2 rhs);
};

Vector2 operator-(Vector2 rhs);
Vector2 operator+(Vector2 lhs, float rhs);
Vector2 operator-(Vector2 lhs, float rhs);
Vector2 operator*(Vector2 lhs, float rhs);
Vector2 operator/(Vector2 lhs, float rhs);
Vector2 operator+(float lhs, Vector2 rhs);
Vector2 operator-(float lhs, Vector2 rhs);
Vector2 operator*(float lhs, Vector2 rhs);
Vector2 operator/(float lhs, Vector2 rhs);
Vector2 operator+(Vector2 lhs, Vector2 rhs);
Vector2 operator-(Vector2 lhs, Vector2 rhs);
Vector2 operator/(Vector2 lhs, Vector2 rhs);
bool operator==(Vector2 lhs, Vector2 rhs);
bool operator!=(Vector2 lhs, Vector2 rhs);