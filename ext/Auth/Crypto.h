#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace LuxCrypto {

    // SHA-256
    static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint32_t sigma0(uint32_t x) { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
    static inline uint32_t sigma1(uint32_t x) { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
    static inline uint32_t gamma0(uint32_t x) { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
    static inline uint32_t gamma1(uint32_t x) { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

    static const uint32_t K256[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    struct Sha256Ctx {
        uint32_t state[8];
        uint64_t bitcount;
        uint8_t  buffer[64];
        size_t   buflen;
    };

    static inline uint32_t load32be(const uint8_t* p) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    static inline void store32be(uint8_t* p, uint32_t v) {
        p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
    }

    static void sha256_transform(Sha256Ctx* ctx, const uint8_t block[64]) {
        uint32_t W[64];
        for (int i = 0; i < 16; i++) W[i] = load32be(block + i * 4);
        for (int i = 16; i < 64; i++) W[i] = gamma1(W[i - 2]) + W[i - 7] + gamma0(W[i - 15]) + W[i - 16];
        uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
        uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K256[i] + W[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
        ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
    }

    static void sha256_init(Sha256Ctx* ctx) {
        ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
        ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
        ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
        ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
        ctx->bitcount = 0; ctx->buflen = 0;
    }

    static void sha256_update(Sha256Ctx* ctx, const uint8_t* data, size_t len) {
        ctx->bitcount += (uint64_t)len * 8;
        while (len > 0) {
            size_t space = 64 - ctx->buflen;
            size_t chunk = (len < space) ? len : space;
            memcpy(ctx->buffer + ctx->buflen, data, chunk);
            ctx->buflen += chunk; data += chunk; len -= chunk;
            if (ctx->buflen == 64) { sha256_transform(ctx, ctx->buffer); ctx->buflen = 0; }
        }
    }

    static void sha256_final(Sha256Ctx* ctx, uint8_t hash[32]) {
        uint64_t bits_save = ctx->bitcount; // salva ANTES do padding
        uint8_t pad = 0x80; sha256_update(ctx, &pad, 1); pad = 0;
        while (ctx->buflen != 56) sha256_update(ctx, &pad, 1);
        uint8_t bits[8];
        for (int i = 7; i >= 0; i--) { bits[i] = (uint8_t)(bits_save & 0xFF); bits_save >>= 8; }
        sha256_update(ctx, bits, 8);
        for (int i = 0; i < 8; i++) store32be(hash + i * 4, ctx->state[i]);
    }

    static void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
        Sha256Ctx ctx; sha256_init(&ctx); sha256_update(&ctx, data, len); sha256_final(&ctx, out);
    }

    // HMAC-SHA256
    static void hmac_sha256(const uint8_t* key, size_t key_len,
        const uint8_t* msg, size_t msg_len, uint8_t out[32])
    {
        uint8_t k_pad[64], k_hash[32];
        if (key_len > 64) { sha256(key, key_len, k_hash); key = k_hash; key_len = 32; }
        memset(k_pad, 0x36, 64);
        for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
        Sha256Ctx ctx; sha256_init(&ctx);
        sha256_update(&ctx, k_pad, 64); sha256_update(&ctx, msg, msg_len);
        uint8_t inner[32]; sha256_final(&ctx, inner);
        memset(k_pad, 0x5c, 64);
        for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
        sha256_init(&ctx);
        sha256_update(&ctx, k_pad, 64); sha256_update(&ctx, inner, 32);
        sha256_final(&ctx, out);
        memset(k_pad, 0, 64); memset(inner, 0, 32);
    }

    // HMAC com inputs concatenados (evita malloc)
    static void hmac_sha256_2(const uint8_t* key, size_t key_len,
        const uint8_t* a, size_t a_len,
        const uint8_t* b, size_t b_len,
        uint8_t out[32])
    {
        uint8_t k_pad[64], k_hash[32];
        if (key_len > 64) { sha256(key, key_len, k_hash); key = k_hash; key_len = 32; }
        memset(k_pad, 0x36, 64);
        for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
        Sha256Ctx ctx; sha256_init(&ctx);
        sha256_update(&ctx, k_pad, 64);
        sha256_update(&ctx, a, a_len);
        sha256_update(&ctx, b, b_len);
        uint8_t inner[32]; sha256_final(&ctx, inner);
        memset(k_pad, 0x5c, 64);
        for (size_t i = 0; i < key_len; i++) k_pad[i] ^= key[i];
        sha256_init(&ctx);
        sha256_update(&ctx, k_pad, 64); sha256_update(&ctx, inner, 32);
        sha256_final(&ctx, out);
        memset(k_pad, 0, 64); memset(inner, 0, 32);
    }

    // Stream cipher: SHA-256 CTR
    static void stream_cipher(const uint8_t* key, size_t key_len,
        const uint8_t* nonce, size_t nonce_len,
        uint8_t* data, size_t data_len)
    {
        size_t offset = 0; uint32_t counter = 0;
        while (offset < data_len) {
            Sha256Ctx ctx; sha256_init(&ctx);
            sha256_update(&ctx, key, key_len);
            sha256_update(&ctx, nonce, nonce_len);
            uint8_t ctr[4] = { (uint8_t)(counter),(uint8_t)(counter >> 8),(uint8_t)(counter >> 16),(uint8_t)(counter >> 24) };
            sha256_update(&ctx, ctr, 4);
            uint8_t block[32]; sha256_final(&ctx, block);
            size_t n = data_len - offset; if (n > 32)n = 32;
            for (size_t i = 0; i < n; i++) data[offset + i] ^= block[i];
            offset += n; counter++;
        }
    }

    // session_key = HMAC-SHA256(master_key, client_nonce || server_nonce)
    static void derive_session_key(const uint8_t* master_key, size_t mk_len,
        const uint8_t cn[16], const uint8_t sn[16],
        uint8_t sk[32])
    {
        hmac_sha256_2(master_key, mk_len, cn, 16, sn, 16, sk);
    }

    // Nonce via RtlGenRandom (Advapi32)
    extern "C" { __declspec(dllimport) BOOLEAN WINAPI SystemFunction036(PVOID, ULONG); }
#define RtlGenRandom SystemFunction036

    static bool gen_nonce(uint8_t* buf, size_t len) {
        return RtlGenRandom(buf, (ULONG)len) != FALSE;
    }

    // Timing-safe compare
    static bool secure_cmp(const uint8_t* a, const uint8_t* b, size_t len) {
        volatile uint8_t d = 0;
        for (size_t i = 0; i < len; i++) d |= a[i] ^ b[i];
        return d == 0;
    }

} // namespace LuxCrypto