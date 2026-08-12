#include "UTF8.hpp"
#include <Main/Memory/Memory.hpp>

static const UTF32 HalfMask = 0x3FFUL;
static const UTF32 HalfBase = 0x0010000UL;
static const UTF8 FirstByteMark [ 7 ] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const UTF32 OffsetsFromUTF8 [ 6 ] = { 0x00000000UL, 0x00003080UL, 0x000E2080UL, 0x03C82080UL, 0xFA082080UL, 0x82082080UL };
static const char TrailingBytesForUTF8 [ 256 ] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

int Utf16ToUtf8( const UTF16* sourceStart, UTF8* targetStart, size_t outLen, ConversionFlags flags )
{
	int result = 0;
	const UTF16* source = sourceStart;
	UTF8* target = targetStart;
	UTF8* targetEnd = targetStart + outLen;
	if ( ( NULL == source ) || ( NULL == targetStart ) )
	{
		return ConversionFailed;
	}

	while ( *source )
	{
		UTF32 ch;
		unsigned short bytesToWrite = 0;
		const UTF32 byteMask = 0xBF;
		const UTF32 byteMark = 0x80;
		const UTF16* oldSource = source;
		ch = *source++;
		if ( ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END )
		{
			if ( *source )
			{
				UTF32 ch2 = *source;
				if ( ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END )
				{
					ch = ( ( ch - UNI_SUR_HIGH_START ) << HalfShift ) + ( ch2 - UNI_SUR_LOW_START ) + HalfBase;
					++source;
				}
				else if ( flags == StrictConversion )
				{
					--source;
					result = SourceIllegal;
					break;
				}
			}
			else
			{
				--source;
				result = SourceExhausted;
				break;
			}
		}
		else if ( flags == StrictConversion )
		{
			if ( ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END )
			{
				--source;
				result = SourceIllegal;
				break;
			}
		}

		if ( ch < ( UTF32 )0x80 )
		{
			bytesToWrite = 1;
		}
		else if ( ch < ( UTF32 )0x800 )
		{
			bytesToWrite = 2;
		}
		else if ( ch < ( UTF32 )0x10000 )
		{
			bytesToWrite = 3;
		}
		else if ( ch < ( UTF32 )0x110000 )
		{
			bytesToWrite = 4;
		}
		else
		{
			bytesToWrite = 3;
			ch = UNI_REPLACEMENT_CHAR;
		}

		target += bytesToWrite;
		if ( target > targetEnd )
		{
			source = oldSource;
			target -= bytesToWrite; result = TargetExhausted; break;
		}
		switch ( bytesToWrite )
		{
			case 4: *--target = ( UTF8 )( ( ch | byteMark ) & byteMask ); ch >>= 6;
			case 3: *--target = ( UTF8 )( ( ch | byteMark ) & byteMask ); ch >>= 6;
			case 2: *--target = ( UTF8 )( ( ch | byteMark ) & byteMask ); ch >>= 6;
			case 1: *--target = ( UTF8 )( ch | FirstByteMark [ bytesToWrite ] );
		}
		target += bytesToWrite;
	}
	return result;
}

std::string ObterStr( uintptr_t address, int count )
{
	const int MAX_COUNT = 64;
	if ( count <= 0 || count > MAX_COUNT )
	{
		return "";
	}

	int classname;
	int m = 0;
	char a [ 500 ];
	UTF8 buf88 [ 256 ] = "";
	UTF16 buf16 [ MAX_COUNT * 2 + 2 ] = { 0 };
	int hex [ 2 ] = { 0 };

	for ( int i = 0; i < count; i++ )
	{
		classname = g_FreeFireMemory.Read<uintptr_t>( address + i * 4 );
		hex [ 0 ] = ( classname & 0xfffff000 ) >> 16;
		hex [ 1 ] = classname & 0xffff;
		buf16 [ m ] = hex [ 1 ];
		buf16 [ m + 1 ] = hex [ 0 ];
		m += 2;
	}

	Utf16ToUtf8( buf16, buf88, sizeof( buf88 ), StrictConversion );
	sprintf_s( a, ( "%s" ), buf88 );
	return a;
}