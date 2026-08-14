// xorstr.hpp — Compile-time XOR string obfuscation.
// All sensitive strings must use OBF() to defeat static string scanning.
// Key comes from build_keys.hpp (per-build random).
//
// Reference: Periscope prototype/src/conceal/build_keys.hpp.in,
//            Periscope ENC() macro usage throughout.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "real/platform.hpp"

// Per-build XOR key seed from CMake-generated header
// The generated file defines build::kXorKeySeed.
// Include path may expose it as build_keys.hpp (code/build/generated) or
// generated/build_keys.hpp depending on the target include dirs.
#if defined(BUILD_KEYS_HPP_INCLUDED)
// already provided by an earlier include
#elif __has_include("build_keys.hpp")
#  include "build_keys.hpp"
#  define BUILD_KEYS_HPP_INCLUDED 1
#elif __has_include("generated/build_keys.hpp")
#  include "generated/build_keys.hpp"
#  define BUILD_KEYS_HPP_INCLUDED 1
#else
// Fallback: define build::kXorKeySeed directly
namespace build_kxorseed_detail {
    // Use a constexpr variable at namespace scope
    inline constexpr unsigned long long kFallbackKey = 0xDEADBEEFCAFEBABEULL;
}
namespace build {
    inline constexpr unsigned long long kXorKeySeed = build_kxorseed_detail::kFallbackKey;
}
#endif

namespace win::obf {

// Golden ratio constant used for per-position key rotation
inline constexpr std::uint64_t kGoldenRatio = 0x9E3779B97F4A7C15ULL;

/// Compile-time encrypted, runtime-decrypted string.
template<typename CharT, std::size_t N, std::uint64_t Key = build::kXorKeySeed>
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
// Constructor parameter is const CharT (&)[N+1], so deduce N = array_size - 1.
template<typename CharT, std::size_t N>
encrypted_string(const CharT (&)[N]) -> encrypted_string<CharT, N - 1>;

// Convenience macro
// Usage: OBF("NtOpenProcess") returns const char*
#define OBF(str)                                                           \
    ([]() -> const char* {                                                 \
        static auto _obf_ = ::win::obf::encrypted_string<                  \
            char, sizeof(str) - 1>(str);                                   \
        return _obf_.decrypt();                                            \
    }())

// Wide variant
#define OBFW(str)                                                          \
    ([]() -> const wchar_t* {                                              \
        static auto _obf_ = ::win::obf::encrypted_string<                  \
            wchar_t, (sizeof(str) / sizeof(wchar_t)) - 1>(str);            \
        return _obf_.decrypt();                                            \
    }())

// Stack-based decryption: decrypts into a thread_local buffer that is
// overwritten on each call. The decrypted string exists only per-thread
// and does not persist in static storage after resolution completes.
#define OBF_STACK(str)                                                      \
    ([]() -> const char* {                                                  \
        constexpr auto _enc_s_ = ::win::obf::encrypted_string<              \
            char, sizeof(str) - 1>(str);                                    \
        thread_local char _buf_s_[sizeof(str)];                             \
        for (std::size_t _i_s_ = 0; _i_s_ < sizeof(str) - 1; ++_i_s_) {    \
            _buf_s_[_i_s_] = static_cast<char>(                             \
                static_cast<unsigned long long>(_enc_s_.data()[_i_s_]) ^    \
                (build::kXorKeySeed +                                       \
                 static_cast<unsigned long long>(_i_s_) *                    \
                 ::win::obf::kGoldenRatio));                                \
        }                                                                   \
        _buf_s_[sizeof(str) - 1] = 0;                                       \
        return _buf_s_;                                                      \
    }())

/// Compile-time encrypted byte pattern for AOB scanning.
template<std::size_t N>
struct encrypted_pattern {
    encrypted_pattern() = delete;

    constexpr encrypted_pattern(const uint8_t (&raw)[N], std::uint64_t key) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            m_data[i] = static_cast<uint8_t>(
                static_cast<std::uint64_t>(raw[i]) ^
                (key + static_cast<std::uint64_t>(i) * kGoldenRatio));
        }
    }

    void decrypt(uint8_t* out, std::uint64_t key) noexcept {
        for (std::size_t i = 0; i < N; ++i) {
            out[i] = static_cast<uint8_t>(
                static_cast<std::uint64_t>(m_data[i]) ^
                (key + static_cast<std::uint64_t>(i) * kGoldenRatio));
        }
        m_decrypted = true;
    }

    void re_encrypt(uint8_t* buf, std::uint64_t key) noexcept {
        if (m_decrypted) {
            for (std::size_t i = 0; i < N; ++i) {
                m_data[i] = static_cast<uint8_t>(
                    static_cast<std::uint64_t>(buf[i]) ^
                    (key + static_cast<std::uint64_t>(i) * kGoldenRatio));
            }
            m_decrypted = false;
        }
    }

    constexpr std::size_t size() const noexcept { return N; }

private:
    uint8_t m_data[N]{};
    bool m_decrypted{false};
};

} // namespace win::obf
