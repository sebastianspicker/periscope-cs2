// obf.hpp — Self-contained compile-time XOR string obfuscation.
//
// Drop-in replacement for the repository's real/win/xorstr.hpp so that this
// firmware example builds standalone (no dependency on radar/adapters includes).
// Same API: OBF("literal") / OBFW(L"literal") return const char*/wchar_t*
// pointing at a runtime-decrypted, compile-time-encrypted buffer.
//
// Technique: golden-ratio key rotation per position + per-build seed, so the
// encrypted form differs for every position and every build.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(_MSC_VER)
#  define OBF_FORCEINLINE __forceinline
#else
#  define OBF_FORCEINLINE inline __attribute__((always_inline))
#endif

namespace obf {

// Per-build XOR key seed. Overridable via /DOBF_XOR_KEY=... or -DOBF_XOR_KEY=
#if defined(OBF_XOR_KEY)
inline constexpr std::uint64_t kXorKeySeed = OBF_XOR_KEY;
#else
inline constexpr std::uint64_t kXorKeySeed = 0xB16B00B5CAFEBABEULL;
#endif

// Golden ratio constant used for per-position key rotation
inline constexpr std::uint64_t kGoldenRatio = 0x9E3779B97F4A7C15ULL;

/// Compile-time encrypted, runtime-decrypted string.
template<typename CharT, std::size_t N, std::uint64_t Key = kXorKeySeed>
class encrypted_string {
public:
    // Compile-time encryption
    constexpr encrypted_string(const CharT (&str)[N + 1]) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            m_data[i] = static_cast<CharT>(
                static_cast<std::uint64_t>(str[i]) ^
                (Key + static_cast<std::uint64_t>(i) * kGoldenRatio));
        }
    }

    // Access encrypted data (for stack-based decryption)
    constexpr const CharT* data() const noexcept { return m_data; }

    // Runtime decryption (one-time)
    const CharT* decrypt() noexcept {
        if (!m_decrypted) {
            for (std::size_t i = 0; i < N; ++i) {
                m_buffer[i] = static_cast<CharT>(
                    static_cast<std::uint64_t>(m_data[i]) ^
                    (Key + static_cast<std::uint64_t>(i) * kGoldenRatio));
            }
            m_buffer[N] = CharT{};
            m_decrypted = true;
        }
        return m_buffer;
    }

    // Force re-encrypt (for anti-memory-scan rotation)
    void re_encrypt() noexcept {
        if (m_decrypted) {
            for (std::size_t i = 0; i < N; ++i) {
                m_data[i] = static_cast<CharT>(
                    static_cast<std::uint64_t>(m_buffer[i]) ^
                    (Key + static_cast<std::uint64_t>(i) * kGoldenRatio));
            }
            m_buffer[N] = CharT{};
            m_decrypted = false;
        }
    }

    static constexpr std::size_t size() noexcept { return N; }

private:
    CharT m_data[N]{};
    CharT m_buffer[N + 1]{};
    bool m_decrypted{false};
};

// MSVC needs an explicit deduction guide for encrypted_string("literal").
template<typename CharT, std::size_t N>
encrypted_string(const CharT (&)[N]) -> encrypted_string<CharT, N - 1>;

// Convenience macro — usage: OBF("string") returns const char*
#define OBF(str)                                                           \
    ([]() -> const char* {                                                 \
        static auto _obf_ = ::obf::encrypted_string<                       \
            char, sizeof(str) - 1>(str);                                   \
        return _obf_.decrypt();                                            \
    }())

// Wide variant
#define OBFW(str)                                                          \
    ([]() -> const wchar_t* {                                              \
        static auto _obf_ = ::obf::encrypted_string<                       \
            wchar_t, (sizeof(str) / sizeof(wchar_t)) - 1>(str);            \
        return _obf_.decrypt();                                            \
    }())

// Stack-based decryption: decrypts into a thread_local buffer that is
// overwritten on each call. The decrypted string exists only per-thread.
#define OBF_STACK(str)                                                      \
    ([]() -> const char* {                                                  \
        constexpr auto _enc_s_ = ::obf::encrypted_string<                   \
            char, sizeof(str) - 1>(str);                                    \
        thread_local char _buf_s_[sizeof(str)];                             \
        for (std::size_t _i_s_ = 0; _i_s_ < sizeof(str) - 1; ++_i_s_) {    \
            _buf_s_[_i_s_] = static_cast<char>(                             \
                static_cast<unsigned long long>(_enc_s_.data()[_i_s_]) ^    \
                (kXorKeySeed +                                              \
                 static_cast<unsigned long long>(_i_s_) *                   \
                 ::obf::kGoldenRatio));                                     \
        }                                                                   \
        _buf_s_[sizeof(str) - 1] = 0;                                       \
        return _buf_s_;                                                     \
    }())

} // namespace obf

// Provide the repository-compatible alias so code written against
// win::obf::encrypted_string still compiles when this folder is consumed
// inside the full repository build.
namespace win::obf {
    using ::obf::encrypted_string;
    using ::obf::kGoldenRatio;
}
