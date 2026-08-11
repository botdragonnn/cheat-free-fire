#pragma once
#include <cstdint>
#include <cstring>
#include <windows.h>

/*
 * ObfuscatedKey - protege a MASTER_KEY no binário e na memória
 *
 * - Em compile-time: XOR de cada byte com chave derivada de __TIME__ + index
 * - No .exe: só os bytes XOR'd aparecem (IDA/x64dbg não acha a key raw)
 * - Em runtime: decrypt → usa → zera automaticamente (RAII)
 *
 * Uso:
 *   {
 *       auto key = ObfuscatedKey::decrypt();  // decifra pra stack
 *       // key.data() aponta pra 32 bytes limpos
 *       ZimoCrypto::derive_session_key(key.data(), 32, cn, sn, sk);
 *   }  // destrutor zera a key do stack automaticamente
 */

namespace ObfKey {

    // Derive XOR pad em compile-time a partir de __TIME__ + seed
    constexpr uint8_t compile_time_byte(uint32_t seed) {
        uint32_t h = seed;
        for (const char c : __TIME__)
            h = (h ^ (uint32_t)c) * 16777619u;
        return (uint8_t)(h ^ (h >> 8) ^ (h >> 16) ^ (h >> 24));
    }

    // XOR pad: 32 bytes, cada um derivado de index + seed diferente
    constexpr uint8_t PAD[32] = {
        compile_time_byte(0x10A),  compile_time_byte(0x20B),
        compile_time_byte(0x30C),  compile_time_byte(0x40D),
        compile_time_byte(0x50E),  compile_time_byte(0x60F),
        compile_time_byte(0x710),  compile_time_byte(0x811),
        compile_time_byte(0x912),  compile_time_byte(0xA13),
        compile_time_byte(0xB14),  compile_time_byte(0xC15),
        compile_time_byte(0xD16),  compile_time_byte(0xE17),
        compile_time_byte(0xF18),  compile_time_byte(0x1019),
        compile_time_byte(0x111A), compile_time_byte(0x121B),
        compile_time_byte(0x131C), compile_time_byte(0x141D),
        compile_time_byte(0x151E), compile_time_byte(0x161F),
        compile_time_byte(0x1720), compile_time_byte(0x1821),
        compile_time_byte(0x1922), compile_time_byte(0x1A23),
        compile_time_byte(0x1B24), compile_time_byte(0x1C25),
        compile_time_byte(0x1D26), compile_time_byte(0x1E27),
        compile_time_byte(0x1F28), compile_time_byte(0x2029),
    };

} // namespace ObfKey

/*
 * MACRO pra gerar os bytes ofuscados em compile-time.
 *
 * Uso:
 *   constexpr uint8_t MASTER_KEY_ENC[32] = OBFUSCATE_KEY_32(
 *       0xA3, 0x1F, 0x7C, 0xE2, ...os 32 bytes reais...
 *   );
 *
 * No binário, MASTER_KEY_ENC contém key XOR pad — não a key real.
 */
#define OBFUSCATE_KEY_32(                                                \
    b0, b1, b2, b3, b4, b5, b6, b7,                                    \
    b8, b9,b10,b11,b12,b13,b14,b15,                                    \
    b16,b17,b18,b19,b20,b21,b22,b23,                                   \
    b24,b25,b26,b27,b28,b29,b30,b31)                                   \
{                                                                        \
    (uint8_t)((b0)  ^ ObfKey::PAD[0]),  (uint8_t)((b1)  ^ ObfKey::PAD[1]),  \
    (uint8_t)((b2)  ^ ObfKey::PAD[2]),  (uint8_t)((b3)  ^ ObfKey::PAD[3]),  \
    (uint8_t)((b4)  ^ ObfKey::PAD[4]),  (uint8_t)((b5)  ^ ObfKey::PAD[5]),  \
    (uint8_t)((b6)  ^ ObfKey::PAD[6]),  (uint8_t)((b7)  ^ ObfKey::PAD[7]),  \
    (uint8_t)((b8)  ^ ObfKey::PAD[8]),  (uint8_t)((b9)  ^ ObfKey::PAD[9]),  \
    (uint8_t)((b10) ^ ObfKey::PAD[10]), (uint8_t)((b11) ^ ObfKey::PAD[11]), \
    (uint8_t)((b12) ^ ObfKey::PAD[12]), (uint8_t)((b13) ^ ObfKey::PAD[13]), \
    (uint8_t)((b14) ^ ObfKey::PAD[14]), (uint8_t)((b15) ^ ObfKey::PAD[15]), \
    (uint8_t)((b16) ^ ObfKey::PAD[16]), (uint8_t)((b17) ^ ObfKey::PAD[17]), \
    (uint8_t)((b18) ^ ObfKey::PAD[18]), (uint8_t)((b19) ^ ObfKey::PAD[19]), \
    (uint8_t)((b20) ^ ObfKey::PAD[20]), (uint8_t)((b21) ^ ObfKey::PAD[21]), \
    (uint8_t)((b22) ^ ObfKey::PAD[22]), (uint8_t)((b23) ^ ObfKey::PAD[23]), \
    (uint8_t)((b24) ^ ObfKey::PAD[24]), (uint8_t)((b25) ^ ObfKey::PAD[25]), \
    (uint8_t)((b26) ^ ObfKey::PAD[26]), (uint8_t)((b27) ^ ObfKey::PAD[27]), \
    (uint8_t)((b28) ^ ObfKey::PAD[28]), (uint8_t)((b29) ^ ObfKey::PAD[29]), \
    (uint8_t)((b30) ^ ObfKey::PAD[30]), (uint8_t)((b31) ^ ObfKey::PAD[31])  \
}

 // RAII wrapper: decifra no stack, zera no destrutor
class ObfuscatedKey {
public:
    __forceinline static ObfuscatedKey decrypt(const uint8_t enc[32]) {
        ObfuscatedKey k;
        for (int i = 0; i < 32; i++)
            k.m_key[i] = enc[i] ^ ObfKey::PAD[i];
        return k;
    }

    __forceinline const uint8_t* data() const { return m_key; }
    __forceinline size_t size() const { return 32; }

    __forceinline ~ObfuscatedKey() {
        // SecureZeroMemory é intrinsic, nunca otimizado fora
        SecureZeroMemory((void*)m_key, 32);
    }

    // Sem cópia
    ObfuscatedKey(const ObfuscatedKey&) = delete;
    ObfuscatedKey& operator=(const ObfuscatedKey&) = delete;
    ObfuscatedKey(ObfuscatedKey&&) = default;

private:
    ObfuscatedKey() = default;
    uint8_t m_key[32];
};