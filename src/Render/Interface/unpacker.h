#pragma once
// unpacker.h - Descomprime e descriptografa em memória
// Usa RtlDecompressBuffer do Windows (sem dependência externa)

#include <Windows.h>
#include <winternl.h>

// Definição para RtlDecompressBuffer
typedef NTSTATUS(NTAPI* fnRtlDecompressBuffer)(
    USHORT CompressionFormat,
    PUCHAR UncompressedBuffer,
    ULONG  UncompressedBufferSize,
    PUCHAR CompressedBuffer,
    ULONG  CompressedBufferSize,
    PULONG FinalUncompressedSize
    );

// Formato de compressão LZNT1 (compatível com Windows)
#define COMPRESSION_FORMAT_LZNT1 0x0002
#define COMPRESSION_ENGINE_MAXIMUM 0x0100

class Unpacker {
private:
    static void XorDecrypt(const unsigned char* input, unsigned char* output,
        size_t size, const unsigned char* key, size_t keyLen) {
        for (size_t i = 0; i < size; i++) {
            output[i] = input[i] ^ key[i % keyLen];
        }
    }

public:
    // Descomprime e descriptografa os bytes
    // Retorna ponteiro alocado (caller deve liberar com VirtualFree)
    static unsigned char* Unpack(const unsigned char* packedData,
        unsigned int packedSize,
        const unsigned char* xorKey,
        unsigned int xorKeySize,
        unsigned int originalSize,
        unsigned int* outSize) {
        if (!packedData || !xorKey || !outSize) return nullptr;

        // 1. Alocar buffer para dados descriptografados (ainda comprimidos)
        unsigned char* decrypted = (unsigned char*)VirtualAlloc(
            NULL, packedSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!decrypted) return nullptr;

        // 2. Descriptografar (XOR)
        XorDecrypt(packedData, decrypted, packedSize, xorKey, xorKeySize);

        // 3. Alocar buffer para dados descomprimidos
        unsigned char* decompressed = (unsigned char*)VirtualAlloc(
            NULL, originalSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!decompressed) {
            VirtualFree(decrypted, 0, MEM_RELEASE);
            return nullptr;
        }

        // 4. Descomprimir usando zlib inline ou API do Windows
        // Vamos usar decompressão simples (os dados vêm com zlib do Python)

        // Para zlib, precisamos usar uma implementação inline
        // Aqui uso uma abordagem simples: inflate básico

        unsigned long destLen = originalSize;
        int result = InflateZlib(decrypted, packedSize, decompressed, &destLen);

        // Limpar buffer criptografado
        SecureZeroMemory(decrypted, packedSize);
        VirtualFree(decrypted, 0, MEM_RELEASE);

        if (result != 0) {
            SecureZeroMemory(decompressed, originalSize);
            VirtualFree(decompressed, 0, MEM_RELEASE);
            return nullptr;
        }

        *outSize = (unsigned int)destLen;
        return decompressed;
    }

    // Limpa buffer de memória
    static void Free(unsigned char* buffer, unsigned int size) {
        if (buffer) {
            SecureZeroMemory(buffer, size);
            VirtualFree(buffer, 0, MEM_RELEASE);
        }
    }

private:
    // Mini implementação de inflate (zlib decompress)
    // Baseado em tinflate - domínio público
    static int InflateZlib(const unsigned char* src, unsigned long srcLen,
        unsigned char* dst, unsigned long* dstLen);
};

// ============================================================================
// TINFLATE - Minimal zlib/deflate decompressor (public domain)
// ============================================================================

#define TINF_OK             0
#define TINF_DATA_ERROR    -3

typedef struct {
    const unsigned char* source;
    unsigned int tag;
    unsigned int bitcount;
    unsigned char* dest;
    unsigned int* destLen;
    unsigned int destMax;
} tinf_data;

static const unsigned char tinf_length_bits[30] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0
};
static const unsigned short tinf_length_base[30] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0
};
static const unsigned char tinf_dist_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};
static const unsigned short tinf_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const unsigned char tinf_clcidx[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

typedef struct {
    unsigned short table[16];
    unsigned short trans[288];
} tinf_tree;

static unsigned int tinf_getbit(tinf_data* d) {
    if (!d->bitcount--) {
        d->tag = *d->source++;
        d->bitcount = 7;
    }
    unsigned int bit = d->tag & 1;
    d->tag >>= 1;
    return bit;
}

static unsigned int tinf_read_bits(tinf_data* d, int num, int base) {
    unsigned int val = 0;
    if (num) {
        unsigned int limit = 1 << num;
        for (unsigned int mask = 1; mask < limit; mask <<= 1)
            if (tinf_getbit(d)) val += mask;
    }
    return val + base;
}

static void tinf_build_tree(tinf_tree* t, const unsigned char* lengths, unsigned int num) {
    unsigned short offs[16];
    for (int i = 0; i < 16; ++i) t->table[i] = 0;
    for (unsigned int i = 0; i < num; ++i) t->table[lengths[i]]++;
    t->table[0] = 0;
    unsigned int sum = 0;
    for (int i = 0; i < 16; ++i) { offs[i] = (unsigned short)sum; sum += t->table[i]; }
    for (unsigned int i = 0; i < num; ++i)
        if (lengths[i]) t->trans[offs[lengths[i]]++] = (unsigned short)i;
}

static int tinf_decode_symbol(tinf_data* d, tinf_tree* t) {
    int sum = 0, cur = 0, len = 0;
    do {
        cur = 2 * cur + tinf_getbit(d);
        ++len;
        sum += t->table[len];
        cur -= t->table[len];
    } while (cur >= 0);
    return t->trans[sum + cur];
}

static void tinf_decode_trees(tinf_data* d, tinf_tree* lt, tinf_tree* dt) {
    unsigned char lengths[288 + 32];
    unsigned int hlit = tinf_read_bits(d, 5, 257);
    unsigned int hdist = tinf_read_bits(d, 5, 1);
    unsigned int hclen = tinf_read_bits(d, 4, 4);

    for (unsigned int i = 0; i < 19; ++i) lengths[i] = 0;
    for (unsigned int i = 0; i < hclen; ++i)
        lengths[tinf_clcidx[i]] = (unsigned char)tinf_read_bits(d, 3, 0);

    tinf_tree code_tree;
    tinf_build_tree(&code_tree, lengths, 19);

    for (unsigned int num = 0; num < hlit + hdist; ) {
        int sym = tinf_decode_symbol(d, &code_tree);
        if (sym < 16) { lengths[num++] = (unsigned char)sym; }
        else if (sym == 16) {
            unsigned char prev = lengths[num - 1];
            for (unsigned int len = tinf_read_bits(d, 2, 3); len; --len) lengths[num++] = prev;
        }
        else if (sym == 17) {
            for (unsigned int len = tinf_read_bits(d, 3, 3); len; --len) lengths[num++] = 0;
        }
        else {
            for (unsigned int len = tinf_read_bits(d, 7, 11); len; --len) lengths[num++] = 0;
        }
    }
    tinf_build_tree(lt, lengths, hlit);
    tinf_build_tree(dt, lengths + hlit, hdist);
}

static int tinf_inflate_block(tinf_data* d, tinf_tree* lt, tinf_tree* dt) {
    while (1) {
        int sym = tinf_decode_symbol(d, lt);
        if (sym == 256) return TINF_OK;
        if (sym < 256) {
            if (*d->destLen >= d->destMax) return TINF_DATA_ERROR;
            d->dest[(*d->destLen)++] = (unsigned char)sym;
        }
        else {
            sym -= 257;
            unsigned int length = tinf_read_bits(d, tinf_length_bits[sym], tinf_length_base[sym]);
            int dist_sym = tinf_decode_symbol(d, dt);
            unsigned int offs = tinf_read_bits(d, tinf_dist_bits[dist_sym], tinf_dist_base[dist_sym]);
            for (unsigned int i = 0; i < length; ++i) {
                if (*d->destLen >= d->destMax) return TINF_DATA_ERROR;
                d->dest[*d->destLen] = d->dest[*d->destLen - offs];
                (*d->destLen)++;
            }
        }
    }
}

static void tinf_build_fixed_trees(tinf_tree* lt, tinf_tree* dt) {
    unsigned char lengths[288];
    for (int i = 0; i < 144; ++i) lengths[i] = 8;
    for (int i = 144; i < 256; ++i) lengths[i] = 9;
    for (int i = 256; i < 280; ++i) lengths[i] = 7;
    for (int i = 280; i < 288; ++i) lengths[i] = 8;
    tinf_build_tree(lt, lengths, 288);
    for (int i = 0; i < 30; ++i) lengths[i] = 5;
    tinf_build_tree(dt, lengths, 30);
}

inline int Unpacker::InflateZlib(const unsigned char* src, unsigned long srcLen,
    unsigned char* dst, unsigned long* dstLen) {
    tinf_data d;
    tinf_tree lt, dt;

    // Skip zlib header (2 bytes: CMF + FLG)
    if (srcLen < 2) return TINF_DATA_ERROR;
    src += 2;
    srcLen -= 2;

    d.source = src;
    d.bitcount = 0;
    d.dest = dst;
    d.destLen = (unsigned int*)dstLen;
    d.destMax = *dstLen;
    *dstLen = 0;

    int bfinal;
    do {
        bfinal = tinf_getbit(&d);
        unsigned int btype = tinf_read_bits(&d, 2, 0);

        if (btype == 0) {
            // Stored block
            d.bitcount = 0;
            unsigned int length = d.source[0] | (d.source[1] << 8);
            d.source += 4;
            for (unsigned int i = 0; i < length; ++i) {
                if (*d.destLen >= d.destMax) return TINF_DATA_ERROR;
                d.dest[(*d.destLen)++] = *d.source++;
            }
        }
        else if (btype == 1) {
            // Fixed Huffman
            tinf_build_fixed_trees(&lt, &dt);
            int res = tinf_inflate_block(&d, &lt, &dt);
            if (res != TINF_OK) return res;
        }
        else if (btype == 2) {
            // Dynamic Huffman
            tinf_decode_trees(&d, &lt, &dt);
            int res = tinf_inflate_block(&d, &lt, &dt);
            if (res != TINF_OK) return res;
        }
        else {
            return TINF_DATA_ERROR;
        }
    } while (!bfinal);

    return TINF_OK;
}
