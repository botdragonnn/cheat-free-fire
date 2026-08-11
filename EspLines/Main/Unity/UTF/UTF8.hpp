#ifndef __UTF_H__
#define __UTF_H__

#include <cstdint>
#include <string>
#include <vector>

#define FALSE 0
#define TRUE 1

#define HalfShift 10
#define UNI_SUR_HIGH_START (UTF32)0xD800
#define UNI_SUR_HIGH_END (UTF32)0xDBFF
#define UNI_SUR_LOW_START (UTF32)0xDC00
#define UNI_SUR_LOW_END (UTF32)0xDFFF
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

typedef unsigned char boolean;
typedef unsigned int CharType;
typedef char UTF8;
typedef unsigned short UTF16;
typedef unsigned int UTF32;

extern const UTF32 HalfMask;
extern const UTF32 HalfBase;
extern const UTF8 FirstByteMark [ 7 ];
extern const UTF32 OffsetsFromUTF8 [ 6 ];
extern const char TrailingBytesForUTF8 [ 256 ];

typedef enum
{
	StrictConversion = 0,
	LenientConversion
} ConversionFlags;

typedef enum
{
	ConversionOK,
	SourceExhausted,
	TargetExhausted,
	SourceIllegal,
	ConversionFailed
} ConversionResult;

int Utf16ToUtf8( const UTF16* sourceStart, UTF8* targetStart, size_t outLen, ConversionFlags flags = StrictConversion );
std::string ObterStr( uintptr_t address, int count );

#endif // __UTF_H__