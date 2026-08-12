#pragma once

#include "Math/Vectors/Vector3.hpp"
#include "Math/Math.hpp"

struct Vector3;

struct Quaternion
{
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
            float W;
        };
        float data[4];
    };

    Vector3 RotateVector(const Vector3& v) const;

    Quaternion();
    Quaternion(float data[]);
    Quaternion(Vector3 vector, float scalar);
    Quaternion(float x, float y, float z, float w);

    static Quaternion Identity();
    static float Angle(Quaternion a, Quaternion b);
    static Quaternion Conjugate(Quaternion rotation);
    static float Dot(Quaternion lhs, Quaternion rhs);
    static Quaternion ExpMapSmooth(const Quaternion& start, const Quaternion& end, float t);
    static Quaternion FromAngleAxis(float angle, Vector3 axis);
    static Quaternion FromEuler(Vector3 rotation);
    static Quaternion FromEuler(float x, float y, float z);
    static Quaternion FromToRotation(Vector3 fromVector, Vector3 toVector);
    static Quaternion Inverse(Quaternion rotation);
    static Quaternion Lerp(Quaternion a, Quaternion b, float t);
    static Quaternion LerpUnclamped(Quaternion a, Quaternion b, float t);
    static Quaternion LookRotation(Vector3 forward);
    static Quaternion LookRotation(Vector3 forward, Vector3 upwards);
    static float Norm(Quaternion rotation);
    static Quaternion Normalized(Quaternion rotation);
    static Quaternion RotateTowards(Quaternion from, Quaternion to, float maxRadiansDelta);
    static float SmoothStep(float edge0, float edge1, float x);
    static Quaternion Slerp(Quaternion a, Quaternion b, float t);
    static Quaternion SlerpUnclamped(Quaternion a, Quaternion b, float t);
    static void ToAngleAxis(Quaternion rotation, float& angle, Vector3& axis);
    static Vector3 ToEuler(Quaternion rotation);
    static Vector3 RotatePoint(const Quaternion& rotation, const Vector3& point);

    Quaternion& operator+=(float rhs);
    Quaternion& operator-=(float rhs);
    Quaternion& operator*=(float rhs);
    Quaternion& operator/=(float rhs);
    Quaternion& operator+=(Quaternion rhs);
    Quaternion& operator-=(Quaternion rhs);
    Quaternion& operator*=(Quaternion rhs);
};

Quaternion operator-(Quaternion rhs);
Quaternion operator+(Quaternion lhs, float rhs);
Quaternion operator-(Quaternion lhs, float rhs);
Quaternion operator*(Quaternion lhs, float rhs);
Quaternion operator/(Quaternion lhs, float rhs);
Quaternion operator+(float lhs, Quaternion rhs);
Quaternion operator-(float lhs, Quaternion rhs);
Quaternion operator*(float lhs, Quaternion rhs);
Quaternion operator/(float lhs, Quaternion rhs);
Quaternion operator+(Quaternion lhs, Quaternion rhs);
Quaternion operator-(Quaternion lhs, Quaternion rhs);
Quaternion operator*(Quaternion lhs, Quaternion rhs);
Vector3 operator*(Quaternion lhs, Vector3 rhs);
bool operator==(Quaternion lhs, Quaternion rhs);
bool operator!=(Quaternion lhs, Quaternion rhs);