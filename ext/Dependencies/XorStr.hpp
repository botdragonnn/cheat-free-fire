#ifndef FW_XORSTR_HPP
#define FW_XORSTR_HPP

#if defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM) || defined(__arm__)
#include <arm_neon.h>
#elif defined(_M_X64) || defined(__amd64__) || defined(_M_IX86) || defined(__i386__)
#include <immintrin.h>
#else
#error Unsupported platform
#endif

#include <cstdint>
#include <cstddef>
#include <utility>
#include <type_traits>

#ifdef _MSC_VER
#define FW_FORCEINLINE __forceinline
#define FW_NOINLINE   __declspec(noinline)
#pragma warning(push)
#pragma warning(disable : 4244 4307)
#else
#define FW_FORCEINLINE __attribute__((always_inline)) inline
#define FW_NOINLINE    __attribute__((noinline))
#endif

// ============================================================================
//  Macros — cada call-site gera keys unicas via __COUNTER__ + __LINE__
// ============================================================================

#define xorstr(str)                                                            \
    ::FrameWork::xor_string(                                                   \
        []() { return str; },                                                  \
        std::integral_constant<std::size_t, sizeof(str) / sizeof(*str)>{},     \
        std::make_index_sequence<                                              \
            ::FrameWork::XorStr::_buffer_size<sizeof(str)>()>{},               \
        std::integral_constant<std::uint32_t, __COUNTER__>{},                  \
        std::integral_constant<std::uint32_t, __LINE__>{})

#define xorstr_(str)    xorstr(str).crypt_get()
#define XorStr(str)     xorstr_(str)

#define xorstr_u8(str)                                                         \
    ::FrameWork::xor_string(                                                   \
        []() { return u8##str; },                                              \
        std::integral_constant<std::size_t,                                    \
            sizeof(u8##str) / sizeof(*u8##str)>{},                             \
        std::make_index_sequence<                                              \
            ::FrameWork::XorStr::_buffer_size<sizeof(u8##str)>()>{},           \
        std::integral_constant<std::uint32_t, __COUNTER__>{},                  \
        std::integral_constant<std::uint32_t, __LINE__>{})

#define xorstr_u8_(str)  xorstr_u8(str).crypt_get()
#define XorStrU8(str)    reinterpret_cast<const char*>(xorstr_u8_(str))

namespace FrameWork {
    namespace XorStr {

        // ============================================================================
        //  Buffer size: quantos uint64_t precisamos (alinhado a 128-bit / 16 bytes)
        // ============================================================================
        template<std::size_t Size>
        FW_FORCEINLINE constexpr std::size_t _buffer_size()
        {
            return ((Size / 16) + (Size % 16 != 0)) * 2;
        }

        // ============================================================================
        //  Key generation — FNV-1a com mixing agressivo
        //  Seed incorpora __TIME__, __COUNTER__ e __LINE__ do call-site
        //  Cada instância de xorstr() em linhas diferentes gera keys completamente
        //  diferentes, mesmo que a string seja igual.
        // ============================================================================

        namespace detail {

            // murmurhash3 finalizer — excelente avalanche
            FW_FORCEINLINE consteval std::uint32_t murmur_mix(std::uint32_t h) noexcept
            {
                h ^= h >> 16;
                h *= 0x85ebca6bu;
                h ^= h >> 13;
                h *= 0xc2b2ae35u;
                h ^= h >> 16;
                return h;
            }

            FW_FORCEINLINE consteval std::uint64_t murmur_mix64(std::uint64_t k) noexcept
            {
                k ^= k >> 33;
                k *= 0xff51afd7ed558ccdULL;
                k ^= k >> 33;
                k *= 0xc4ceb9fe1a85ec53ULL;
                k ^= k >> 33;
                return k;
            }

        } // namespace detail

        template<std::uint32_t Seed, std::uint32_t Counter, std::uint32_t Line>
        FW_FORCEINLINE consteval std::uint32_t key4() noexcept
        {
            // seed base: combina Seed + Counter + Line antes do FNV
            std::uint32_t value = Seed;
            value ^= detail::murmur_mix(Counter);
            value ^= detail::murmur_mix(Line * 2654435761u);

            // FNV-1a sobre __TIME__ 
            for (char c : __TIME__)
                value = static_cast<std::uint32_t>((value ^ static_cast<std::uint32_t>(c)) * 16777619u);

            // finalizer extra pra garantir avalanche
            return detail::murmur_mix(value);
        }

        template<std::size_t S, std::uint32_t Counter, std::uint32_t Line>
        FW_FORCEINLINE consteval std::uint64_t key8()
        {
            constexpr auto lo = key4<2166136261u + static_cast<std::uint32_t>(S), Counter, Line>();
            constexpr auto hi = key4<lo, Counter ^ 0xDEADBEEF, Line ^ 0xCAFEBABE>();
            return detail::murmur_mix64(
                (static_cast<std::uint64_t>(hi) << 32) | static_cast<std::uint64_t>(lo));
        }

        // ============================================================================
        //  XOR da string em blocos de 8 bytes em compile-time
        // ============================================================================
        template<std::size_t N, class CharT>
        FW_FORCEINLINE consteval std::uint64_t
            load_xored_str8(std::uint64_t key, std::size_t idx, const CharT* str) noexcept
        {
            using cast_type = typename std::make_unsigned<CharT>::type;
            constexpr auto value_size = sizeof(CharT);
            constexpr auto idx_offset = 8 / value_size;

            std::uint64_t value = key;
            for (std::size_t i = 0; i < idx_offset && i + idx * idx_offset < N; ++i)
                value ^= (std::uint64_t{ static_cast<cast_type>(str[i + idx * idx_offset]) }
            << ((i % idx_offset) * 8 * value_size));
            return value;
        }

        // ============================================================================
        //  Anti-optimizer: força valor a passar por registrador 
        //  Impede o compilador de propagar constantes e expor a string em plaintext
        // ============================================================================
        FW_FORCEINLINE std::uint64_t load_from_reg(std::uint64_t value) noexcept
        {
#if defined(__clang__) || defined(__GNUC__)
            asm volatile("" : "=r"(value) : "0"(value) : "memory");
            return value;
#else
            // MSVC: volatile + barreira de memória
            volatile std::uint64_t reg = value;
            _ReadWriteBarrier();
            return reg;
#endif
        }

        // ============================================================================
        //  Secure zero — não pode ser otimizado fora pelo compiler
        // ============================================================================
        FW_FORCEINLINE void secure_zero(volatile void* ptr, std::size_t size) noexcept
        {
            volatile std::uint8_t* p = static_cast<volatile std::uint8_t*>(ptr);
            while (size--)
                *p++ = 0;
#if defined(__clang__) || defined(__GNUC__)
            asm volatile("" ::: "memory");
#else
            _ReadWriteBarrier();
#endif
        }

    } // namespace XorStr

    // ============================================================================
    //  Core class — xor_string
    // ============================================================================
    template<class CharT, std::size_t Size, class Keys, class Indices>
    class xor_string;

    template<class CharT, std::size_t Size, std::uint64_t... Keys, std::size_t... Indices>
    class xor_string<CharT, Size,
        std::integer_sequence<std::uint64_t, Keys...>,
        std::index_sequence<Indices...>>
    {
        // SSE only — 16-byte alignment sempre
        constexpr static inline std::uint64_t alignment = 16;

        alignas(alignment) std::uint64_t _storage[sizeof...(Keys)];
        bool _decrypted = false;

    public:
        using value_type = CharT;
        using size_type = std::size_t;
        using pointer = CharT*;
        using const_pointer = const CharT*;

        // ctor: salva os blocos XOR'd (encrypted) no storage
        template<class L, std::uint32_t C, std::uint32_t Ln>
        FW_FORCEINLINE xor_string(
            L l,
            std::integral_constant<std::size_t, Size>,
            std::index_sequence<Indices...>,
            std::integral_constant<std::uint32_t, C>,
            std::integral_constant<std::uint32_t, Ln>) noexcept
            : _storage{
                XorStr::load_from_reg(
                    (std::integral_constant<std::uint64_t,
                        XorStr::load_xored_str8<Size>(Keys, Indices, l())
                    >::value))...
            }
        {
        }

        FW_FORCEINLINE constexpr size_type size() const noexcept
        {
            return Size - 1;
        }

        // ========================================================================
        //  Anti-pattern helpers — operações identity que geram opcodes diferentes
        //  pra quebrar a signature linear load→xor→store
        //  Chamadas antes do XOR no keys array, sem lambdas (MSVC compat)
        // ========================================================================
    private:
        FW_FORCEINLINE static void _obfuscate_keys(
            std::uint64_t* keys, std::size_t count) noexcept
        {
#if defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM) || defined(__arm__)
            for (std::size_t i = 0; i < count; i += 2) {
                uint64x2_t k = vld1q_u64(keys + i);
                // double byte-reverse = identity, mas gera instrução real
                k = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(k)));
                k = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(k)));
                vst1q_u64(keys + i, k);
            }
#else
            // x86 SSE2 — aplica transformações identity variadas por bloco
            std::size_t blocks = count / 2;
            for (std::size_t i = 0; i < blocks; ++i) {
                __m128i k = _mm_load_si128(
                    reinterpret_cast<const __m128i*>(keys) + i);

                switch (i % 3) {
                case 0:
                    // double swap = identity
                    k = _mm_shuffle_epi32(k, _MM_SHUFFLE(1, 0, 3, 2));
                    k = _mm_shuffle_epi32(k, _MM_SHUFFLE(1, 0, 3, 2));
                    break;
                case 1:
                    // xor com zero + or com self = identity
                    k = _mm_xor_si128(k, _mm_setzero_si128());
                    k = _mm_or_si128(k, _mm_and_si128(k, k));
                    break;
                case 2: {
                    // add zero + sub zero = identity
                    __m128i z = _mm_setzero_si128();
                    k = _mm_add_epi64(k, z);
                    k = _mm_sub_epi64(k, z);
                    break;
                }
                }

                _mm_store_si128(reinterpret_cast<__m128i*>(keys) + i, k);
            }
#endif
        }

    public:
        // ========================================================================
        //  Decrypt in-place (SSE / NEON) — sem lambdas em fold expressions
        // ========================================================================
        FW_FORCEINLINE void crypt() noexcept
        {
#if defined(__clang__)
            alignas(alignment)
                std::uint64_t arr[]{ XorStr::load_from_reg(Keys)... };
            std::uint64_t* keys =
                (std::uint64_t*)XorStr::load_from_reg((std::uint64_t)arr);
#else
            alignas(alignment)
                std::uint64_t keys[]{ XorStr::load_from_reg(Keys)... };
#endif

            // anti-pattern: transforma as keys com ops identity variadas
            // gera opcodes reais que mudam o pattern no binário
            _obfuscate_keys(keys, sizeof...(Keys));

#if defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM) || defined(__arm__)
            ((Indices >= sizeof(_storage) / 16 ? static_cast<void>(0) : vst1q_u64(
                reinterpret_cast<uint64_t*>(_storage) + Indices * 2,
                veorq_u64(
                    vld1q_u64(reinterpret_cast<const uint64_t*>(_storage) + Indices * 2),
                    vld1q_u64(reinterpret_cast<const uint64_t*>(keys) + Indices * 2))
            )), ...);
#else
            ((Indices >= sizeof(_storage) / 16 ? static_cast<void>(0) : _mm_store_si128(
                reinterpret_cast<__m128i*>(_storage) + Indices,
                _mm_xor_si128(
                    _mm_load_si128(reinterpret_cast<const __m128i*>(_storage) + Indices),
                    _mm_load_si128(reinterpret_cast<const __m128i*>(keys) + Indices))
            )), ...);
#endif
            _decrypted = true;
        }

        // ========================================================================
        //  Wipe — limpa o storage de forma segura (não pode ser optimized out)
        // ========================================================================
        FW_FORCEINLINE void wipe() noexcept
        {
            XorStr::secure_zero(_storage, sizeof(_storage));
            _decrypted = false;
        }

        // ========================================================================
        //  Accessors
        // ========================================================================
        FW_FORCEINLINE const_pointer get() const noexcept
        {
            return reinterpret_cast<const_pointer>(_storage);
        }

        FW_FORCEINLINE pointer get() noexcept
        {
            return reinterpret_cast<pointer>(_storage);
        }

        // decrypt + return (clássico)
        FW_FORCEINLINE pointer crypt_get() noexcept
        {
            crypt();
            return reinterpret_cast<pointer>(_storage);
        }

        // ========================================================================
        //  use() — decrypt, chama callback, wipe automático
        //  Uso: xorstr("secret").use([](const char* s) { api(s); });
        //  A string NUNCA persiste na memória após o lambda retornar.
        // ========================================================================
        template<typename F>
        FW_FORCEINLINE auto use(F&& fn) noexcept(noexcept(fn(std::declval<const_pointer>())))
            -> decltype(fn(std::declval<const_pointer>()))
        {
            crypt();
            if constexpr (std::is_void_v<decltype(fn(get()))>) {
                fn(get());
                wipe();
            }
            else {
                auto result = fn(get());
                wipe();
                return result;
            }
        }

        // dtor: se foi descriptografada e não foi limpa, limpa agora
        FW_FORCEINLINE ~xor_string() noexcept
        {
            if (_decrypted)
                wipe();
        }
    };

    // ============================================================================
    //  RAII guard — pra quem quer controle manual de lifetime
    //  {
    //      auto s = xorstr("string");
    //      FrameWork::xor_guard g(s);
    //      api_call(g.get());
    //  } // wipe automático aqui
    // ============================================================================
    template<class XorT>
    class xor_guard {
        XorT& _ref;
        typename XorT::pointer _ptr;

    public:
        FW_FORCEINLINE xor_guard(XorT& x) noexcept
            : _ref(x), _ptr(x.crypt_get()) {
        }

        FW_FORCEINLINE ~xor_guard() noexcept { _ref.wipe(); }

        xor_guard(const xor_guard&) = delete;
        xor_guard& operator=(const xor_guard&) = delete;

        FW_FORCEINLINE operator typename XorT::const_pointer() const noexcept { return _ptr; }
        FW_FORCEINLINE typename XorT::const_pointer get() const noexcept { return _ptr; }
        FW_FORCEINLINE typename XorT::pointer get() noexcept { return _ptr; }
    };

    // CTAD guide
    template<class XorT>
    xor_guard(XorT&) -> xor_guard<XorT>;

    // ============================================================================
    //  Deduction guide — conecta lambda + keys + indices
    //  Agora recebe Counter e Line como template args
    // ============================================================================
    template<class L, std::size_t Size, std::size_t... Indices,
        std::uint32_t Counter, std::uint32_t Line>
    xor_string(
        L l,
        std::integral_constant<std::size_t, Size>,
        std::index_sequence<Indices...>,
        std::integral_constant<std::uint32_t, Counter>,
        std::integral_constant<std::uint32_t, Line>)
        -> xor_string<
        std::remove_const_t<std::remove_reference_t<decltype(l()[0])>>,
        Size,
        std::integer_sequence<std::uint64_t,
        XorStr::key8<Indices, Counter, Line>()...>,
        std::index_sequence<Indices...>>;

} // namespace FrameWork

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // FW_XORSTR_HPP