#pragma once
#include <cstdint>
#include <Main/Memory/Memory.hpp>
#include <Math/Vectors/Vector3.hpp>
#include <Math/Vectors/Vector4.hpp>
#include <Cheat/Globals.hpp>

extern int ScreenWidth;
extern int ScreenHeight;

struct TMatrix
{
	Vector4 Position;
	Vector4 Rotation;
	Vector4 Scale;
};

struct Matrix4x4
{
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m [ 4 ][ 4 ];
		float v [ 16 ];
	};
};

class W2S
{
	public:
	static Vector3 World2Screen( Matrix4x4 ViewMatrix, Vector3 Pos );
	static bool IsOnScreen( const Vector3& Bone );
	static bool FovCheck( Matrix4x4 matrix, Vector3 aimPos, float SilentFov );
};

class Transform
{
	public:
	static Vector3 get_position_Injected( uintptr_t Transform, bool N32 );
	static Vector3 GetHeadPosition( uintptr_t Entity, bool N32 );
	static Vector3 GetPosition( uintptr_t Entity, bool N32 );
};

class MatrixUtils
{
	public:
	static bool Invert( const Matrix4x4& m, Matrix4x4& out );
	static Vector4 Multiply( const Vector4& v, const Matrix4x4& m );
};
