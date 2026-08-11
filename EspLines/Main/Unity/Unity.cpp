#include "Unity.hpp"

int ScreenWidth = 0;
int ScreenHeight = 0;

Vector3 W2S::World2Screen( Matrix4x4 ViewMatrix, Vector3 Pos )
{
	Vector3 screen;

	float clipW = ViewMatrix._14 * Pos.X + ViewMatrix._24 * Pos.Y + ViewMatrix._34 * Pos.Z + ViewMatrix._44;
	if ( clipW < 0.01f )
	{
		screen.Z = -1.0f;
		return screen;
	}

	float clipX = ViewMatrix._11 * Pos.X + ViewMatrix._21 * Pos.Y + ViewMatrix._31 * Pos.Z + ViewMatrix._41;
	float clipY = ViewMatrix._12 * Pos.X + ViewMatrix._22 * Pos.Y + ViewMatrix._32 * Pos.Z + ViewMatrix._42;

	screen.X = ( ScreenWidth / 2.0f ) + ( ScreenWidth / 2.0f ) * ( clipX / clipW );
	screen.Y = ( ScreenHeight / 2.0f ) - ( ScreenHeight / 2.0f ) * ( clipY / clipW );
	screen.Z = clipW;

	return screen;
}

bool W2S::IsOnScreen( const Vector3& Bone )
{
	return ( Bone.Z > 0.0f && Bone.X >= 0 && Bone.X <= ScreenWidth && Bone.Y >= 0 && Bone.Y <= ScreenHeight && !isnan( Bone.X ) && !isnan( Bone.Y ) && !isnan( Bone.Z ) );
}

bool W2S::FovCheck( Matrix4x4 matrix, Vector3 aimPos, float SilentFov )
{
	Vector3 screen = World2Screen( matrix, aimPos );
	if ( screen == Vector3::Zero( ) ) return false;
	float dist = Vector3::Distance( Vector3( ScreenWidth / 2.0f, ScreenHeight / 2.0f, 0.0f ), Vector3( screen.X, screen.Y, 0.0f ) );
	return dist <= SilentFov;
}

Vector3 Transform::get_position_Injected( uintptr_t Transform, bool N32 )
{
	auto ReadPtr = [ N32 ] ( uintptr_t addr ) -> uintptr_t
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
	};

	uintptr_t transObj = ReadPtr( Transform + Offsets::GetPosWorld::transObj );
	uintptr_t matrix = ReadPtr( transObj + Offsets::GetPosWorld::matrix );
	uintptr_t index = ReadPtr( transObj + Offsets::GetPosWorld::index );
	uintptr_t matrix_list = ReadPtr( matrix + Offsets::GetPosWorld::matrix_list );
	uintptr_t matrix_indices = ReadPtr( matrix + Offsets::GetPosWorld::matrix_indices );

	if ( !transObj || !matrix || !matrix_list || !matrix_indices ) return Vector3::Zero( );

	Vector3 result = g_FreeFireMemory.Read<Vector3>( matrix_list + sizeof( TMatrix ) * index );
	int transformIndex = g_FreeFireMemory.Read<int>( matrix_indices + sizeof( int ) * index );

	int curIndex = 0;
	while ( transformIndex >= 0 && curIndex++ < 60 )
	{
		TMatrix tMatrix = g_FreeFireMemory.Read<TMatrix>( matrix_list + sizeof( TMatrix ) * transformIndex );

		float rotX = tMatrix.Rotation.x;
		float rotY = tMatrix.Rotation.y;
		float rotZ = tMatrix.Rotation.z;
		float rotW = tMatrix.Rotation.w;

		float scaleX = result.X * tMatrix.Scale.x;
		float scaleY = result.Y * tMatrix.Scale.y;
		float scaleZ = result.Z * tMatrix.Scale.z;

		result.X = tMatrix.Position.x + scaleX + ( scaleX * ( ( rotY * rotY * -2.0 ) - ( rotZ * rotZ * 2.0 ) ) ) + ( scaleY * ( ( rotW * rotZ * -2.0 ) - ( rotY * rotX * -2.0 ) ) ) + ( scaleZ * ( ( rotZ * rotX * 2.0 ) - ( rotW * rotY * -2.0 ) ) );
		result.Y = tMatrix.Position.y + scaleY + ( scaleX * ( ( rotX * rotY * 2.0 ) - ( rotW * rotZ * -2.0 ) ) ) + ( scaleY * ( ( rotZ * rotZ * -2.0 ) - ( rotX * rotX * 2.0 ) ) ) + ( scaleZ * ( ( rotW * rotX * -2.0 ) - ( rotZ * rotY * -2.0 ) ) );
		result.Z = tMatrix.Position.z + scaleZ + ( scaleX * ( ( rotW * rotY * -2.0 ) - ( rotX * rotZ * -2.0 ) ) ) + ( scaleY * ( ( rotY * rotZ * 2.0 ) - ( rotW * rotX * -2.0 ) ) ) + ( scaleZ * ( ( rotX * rotX * -2.0 ) - ( rotY * rotY * 2.0 ) ) );

		transformIndex = g_FreeFireMemory.Read<int>( matrix_indices + sizeof( int ) * transformIndex );
	}

	return result;
}

Vector3 Transform::GetHeadPosition( uintptr_t Entity, bool N32 )
{
	auto ReadPtr = [ N32 ] ( uintptr_t addr ) -> uintptr_t
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
	};

	bool IsFemale = g_FreeFireMemory.Read<bool>( Entity + Offsets::Player::IsFemale );
	uintptr_t HeadCollider = IsFemale ? Offsets::GetPosWorld::HeadColliderFemale : Offsets::GetPosWorld::HeadColliderMale;

	uintptr_t GetTransform = ReadPtr( Entity + Offsets::Player::m_fireColliders );
	if ( GetTransform != 0 )
	{
		uintptr_t Transform = ReadPtr( GetTransform + Offsets::GetPosWorld::ColliderTransform );
		if ( Transform != 0 )
		{
			uintptr_t Location = ReadPtr( Transform + HeadCollider );
			if ( Location != 0 )
			{
				uintptr_t H1 = ReadPtr( Location + Offsets::GetPosWorld::ColliderTransform );
				if ( H1 != 0 )
				{
					uintptr_t H2 = ReadPtr( H1 + Offsets::GetPosWorld::BoundsCenter_1 );
					if ( H2 != 0 )
					{
						uintptr_t H3 = ReadPtr( H2 + Offsets::GetPosWorld::BoundsCenter_2 );
						if ( H3 != 0 )
						{
							return g_FreeFireMemory.Read<Vector3>( H3 + Offsets::GetPosWorld::BoundsCenter_3 );
						}
					}
				}
			}
		}
	}

	uintptr_t HeadTF = ReadPtr( Entity + Offsets::Player::HeadNode );
	if ( HeadTF != 0 )
	{
		uintptr_t HeadTransform = ReadPtr( HeadTF + Offsets::PlayerTransformNode::Transform );
		if ( HeadTransform != 0 )
		{
			return get_position_Injected( HeadTransform, N32 );
		}
	}

	return Vector3( 0, 0, 0 );
}

Vector3 Transform::GetPosition( uintptr_t Entity, bool N32 )
{
	auto ReadPtr = [ N32 ] ( uintptr_t addr ) -> uintptr_t
	{
		return N32 ? g_FreeFireMemory.Read<uint32_t>( addr ) : g_FreeFireMemory.Read<uint64_t>( addr );
	};

	uintptr_t Transform = ReadPtr( Entity + Offsets::PlayerTransformNode::m_CachedTransform );
	if ( Transform == 0 )
	{
		return Vector3{ 0, 0, 0 };
	}

	return get_position_Injected( Transform, N32 );
}

bool MatrixUtils::Invert( const Matrix4x4& m, Matrix4x4& out )
{
	float inv [ 16 ], det;
	const float* mat = m.v;

	inv [ 0 ] = mat [ 5 ] * mat [ 10 ] * mat [ 15 ] - mat [ 5 ] * mat [ 11 ] * mat [ 14 ] - mat [ 9 ] * mat [ 6 ] * mat [ 15 ] +
		mat [ 9 ] * mat [ 7 ] * mat [ 14 ] + mat [ 13 ] * mat [ 6 ] * mat [ 11 ] - mat [ 13 ] * mat [ 7 ] * mat [ 10 ];
	inv [ 4 ] = -mat [ 4 ] * mat [ 10 ] * mat [ 15 ] + mat [ 4 ] * mat [ 11 ] * mat [ 14 ] + mat [ 8 ] * mat [ 6 ] * mat [ 15 ] -
		mat [ 8 ] * mat [ 7 ] * mat [ 14 ] - mat [ 12 ] * mat [ 6 ] * mat [ 11 ] + mat [ 12 ] * mat [ 7 ] * mat [ 10 ];
	inv [ 8 ] = mat [ 4 ] * mat [ 9 ] * mat [ 15 ] - mat [ 4 ] * mat [ 11 ] * mat [ 13 ] - mat [ 8 ] * mat [ 5 ] * mat [ 15 ] +
		mat [ 8 ] * mat [ 7 ] * mat [ 13 ] + mat [ 12 ] * mat [ 5 ] * mat [ 11 ] - mat [ 12 ] * mat [ 7 ] * mat [ 9 ];
	inv [ 12 ] = -mat [ 4 ] * mat [ 9 ] * mat [ 14 ] + mat [ 4 ] * mat [ 10 ] * mat [ 13 ] + mat [ 8 ] * mat [ 5 ] * mat [ 14 ] -
		mat [ 8 ] * mat [ 6 ] * mat [ 13 ] - mat [ 12 ] * mat [ 5 ] * mat [ 10 ] + mat [ 12 ] * mat [ 6 ] * mat [ 9 ];
	inv [ 1 ] = -mat [ 1 ] * mat [ 10 ] * mat [ 15 ] + mat [ 1 ] * mat [ 11 ] * mat [ 14 ] + mat [ 9 ] * mat [ 2 ] * mat [ 15 ] -
		mat [ 9 ] * mat [ 3 ] * mat [ 14 ] - mat [ 13 ] * mat [ 2 ] * mat [ 11 ] + mat [ 13 ] * mat [ 3 ] * mat [ 10 ];
	inv [ 5 ] = mat [ 0 ] * mat [ 10 ] * mat [ 15 ] - mat [ 0 ] * mat [ 11 ] * mat [ 14 ] - mat [ 8 ] * mat [ 2 ] * mat [ 15 ] +
		mat [ 8 ] * mat [ 3 ] * mat [ 14 ] + mat [ 12 ] * mat [ 2 ] * mat [ 11 ] - mat [ 12 ] * mat [ 3 ] * mat [ 10 ];
	inv [ 9 ] = -mat [ 0 ] * mat [ 9 ] * mat [ 15 ] + mat [ 0 ] * mat [ 11 ] * mat [ 13 ] + mat [ 8 ] * mat [ 1 ] * mat [ 15 ] -
		mat [ 8 ] * mat [ 3 ] * mat [ 13 ] - mat [ 12 ] * mat [ 1 ] * mat [ 11 ] + mat [ 12 ] * mat [ 3 ] * mat [ 9 ];
	inv [ 13 ] = mat [ 0 ] * mat [ 9 ] * mat [ 14 ] - mat [ 0 ] * mat [ 10 ] * mat [ 13 ] - mat [ 8 ] * mat [ 1 ] * mat [ 14 ] +
		mat [ 8 ] * mat [ 2 ] * mat [ 13 ] + mat [ 12 ] * mat [ 1 ] * mat [ 10 ] - mat [ 12 ] * mat [ 2 ] * mat [ 9 ];
	inv [ 2 ] = mat [ 1 ] * mat [ 6 ] * mat [ 15 ] - mat [ 1 ] * mat [ 7 ] * mat [ 14 ] - mat [ 5 ] * mat [ 2 ] * mat [ 15 ] +
		mat [ 5 ] * mat [ 3 ] * mat [ 14 ] + mat [ 13 ] * mat [ 2 ] * mat [ 7 ] - mat [ 13 ] * mat [ 3 ] * mat [ 6 ];
	inv [ 6 ] = -mat [ 0 ] * mat [ 6 ] * mat [ 15 ] + mat [ 0 ] * mat [ 7 ] * mat [ 14 ] + mat [ 4 ] * mat [ 2 ] * mat [ 15 ] -
		mat [ 4 ] * mat [ 3 ] * mat [ 14 ] - mat [ 12 ] * mat [ 2 ] * mat [ 7 ] + mat [ 12 ] * mat [ 3 ] * mat [ 6 ];
	inv [ 10 ] = mat [ 0 ] * mat [ 5 ] * mat [ 15 ] - mat [ 0 ] * mat [ 7 ] * mat [ 13 ] - mat [ 4 ] * mat [ 1 ] * mat [ 15 ] +
		mat [ 4 ] * mat [ 3 ] * mat [ 13 ] + mat [ 12 ] * mat [ 1 ] * mat [ 7 ] - mat [ 12 ] * mat [ 3 ] * mat [ 5 ];
	inv [ 14 ] = -mat [ 0 ] * mat [ 5 ] * mat [ 14 ] + mat [ 0 ] * mat [ 6 ] * mat [ 13 ] + mat [ 4 ] * mat [ 1 ] * mat [ 14 ] -
		mat [ 4 ] * mat [ 2 ] * mat [ 13 ] - mat [ 12 ] * mat [ 1 ] * mat [ 6 ] + mat [ 12 ] * mat [ 2 ] * mat [ 5 ];
	inv [ 3 ] = -mat [ 1 ] * mat [ 6 ] * mat [ 11 ] + mat [ 1 ] * mat [ 7 ] * mat [ 10 ] + mat [ 5 ] * mat [ 2 ] * mat [ 11 ] -
		mat [ 5 ] * mat [ 3 ] * mat [ 10 ] - mat [ 9 ] * mat [ 2 ] * mat [ 7 ] + mat [ 9 ] * mat [ 3 ] * mat [ 6 ];
	inv [ 7 ] = mat [ 0 ] * mat [ 6 ] * mat [ 11 ] - mat [ 0 ] * mat [ 7 ] * mat [ 10 ] - mat [ 4 ] * mat [ 2 ] * mat [ 11 ] +
		mat [ 4 ] * mat [ 3 ] * mat [ 10 ] + mat [ 8 ] * mat [ 2 ] * mat [ 7 ] - mat [ 8 ] * mat [ 3 ] * mat [ 6 ];
	inv [ 11 ] = -mat [ 0 ] * mat [ 5 ] * mat [ 11 ] + mat [ 0 ] * mat [ 7 ] * mat [ 9 ] + mat [ 4 ] * mat [ 1 ] * mat [ 11 ] -
		mat [ 4 ] * mat [ 3 ] * mat [ 9 ] - mat [ 8 ] * mat [ 1 ] * mat [ 7 ] + mat [ 8 ] * mat [ 3 ] * mat [ 5 ];
	inv [ 15 ] = mat [ 0 ] * mat [ 5 ] * mat [ 10 ] - mat [ 0 ] * mat [ 6 ] * mat [ 9 ] - mat [ 4 ] * mat [ 1 ] * mat [ 10 ] +
		mat [ 4 ] * mat [ 2 ] * mat [ 9 ] + mat [ 8 ] * mat [ 1 ] * mat [ 6 ] - mat [ 8 ] * mat [ 2 ] * mat [ 5 ];

	det = mat [ 0 ] * inv [ 0 ] + mat [ 1 ] * inv [ 4 ] + mat [ 2 ] * inv [ 8 ] + mat [ 3 ] * inv [ 12 ];
	if ( det == 0 ) return false;

	det = 1.0f / det;
	for ( int i = 0; i < 16; i++ )
		out.v [ i ] = inv [ i ] * det;
	return true;
}

Vector4 MatrixUtils::Multiply( const Vector4& v, const Matrix4x4& m )
{
	Vector4 r;
	r.x = v.x * m._11 + v.y * m._21 + v.z * m._31 + v.w * m._41;
	r.y = v.x * m._12 + v.y * m._22 + v.z * m._32 + v.w * m._42;
	r.z = v.x * m._13 + v.y * m._23 + v.z * m._33 + v.w * m._43;
	r.w = v.x * m._14 + v.y * m._24 + v.z * m._34 + v.w * m._44;
	return r;
}
