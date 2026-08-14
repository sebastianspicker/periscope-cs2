#include "ac_sim/xorshift.hpp"

#include <cmath>
#include <cstring>

#include "real/platform.hpp"

#if defined(__has_include)
#  if __has_include("build_keys.hpp")
#    include "build_keys.hpp"
#    define AC_SIM_HAS_BUILD_KEYS 1
#  endif
#endif

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#  include <intrin.h>
#endif

namespace sim {
namespace {

// SplitMix64 — used only to expand a single base seed into 10 lane seeds.
std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t entropy_fallback() noexcept {
#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
    unsigned aux = 0;
    return static_cast<std::uint64_t>(__rdtscp(&aux));
#else
    // Address of static gives process-unique entropy without crypto RNG APIs.
    static volatile std::uint64_t counter = 0xC0FFEEULL;
    counter ^= (counter << 7) | 1;
    return counter ^ reinterpret_cast<std::uintptr_t>(&counter);
#endif
}

} // namespace

void XorShift128::seed(std::uint64_t s0, std::uint64_t s1) noexcept {
    m_s0 = s0;
    m_s1 = s1;
    m_hasSpare = false;
    m_spareNormal = 0.f;
    // XorShift128+ requires non-zero state.
    if (m_s0 == 0 && m_s1 == 0) {
        m_s0 = 0xA3B1C5D7E9F0A2B4ULL;
        m_s1 = 0xC6D8E0F2A4B6C8D0ULL;
    }
}

std::uint64_t XorShift128::next() noexcept {
    // xorshift128+ (Vigna)
    std::uint64_t s1 = m_s0;
    const std::uint64_t s0 = m_s1;
    const std::uint64_t result = s0 + s1;
    m_s0 = s0;
    s1 ^= s1 << 23;
    m_s1 = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return result;
}

std::uint64_t XorShift128::next_range(std::uint64_t max) noexcept {
    if (max == 0 || max == 1) return 0;
    // Rejection sampling for unbiased remainder. threshold = 2^64 % max.
    // Accept samples in [0, 2^64 - threshold); each residue 0..max-1 is equiprobable.
    const std::uint64_t threshold = (0ULL - max) % max; // 2^64 % max
    for (;;) {
        const std::uint64_t x = next();
        if (threshold == 0 || x >= threshold) {
            return x % max;
        }
    }
}

float XorShift128::next_float() noexcept {
    // Top 24 bits → [0, 1) with 2^-24 granularity.
    return static_cast<float>((next() >> 40) & 0xFFFFFFu) / 16777216.0f;
}

float XorShift128::next_float_range(float lo, float hi) noexcept {
    if (hi <= lo) return lo;
    return lo + (hi - lo) * next_float();
}

float XorShift128::next_normal(float mean, float stddev) noexcept {
    if (m_hasSpare) {
        m_hasSpare = false;
        return mean + stddev * m_spareNormal;
    }
    // Box-Muller
    float u1 = next_float();
    float u2 = next_float();
    if (u1 < 1e-12f) u1 = 1e-12f;
    const float r = std::sqrt(-2.0f * std::log(u1));
    const float theta = 6.28318530718f * u2;
    m_spareNormal = r * std::sin(theta);
    m_hasSpare = true;
    return mean + stddev * (r * std::cos(theta));
}

int XorShift128::next_int(int lo, int hi) noexcept {
    if (hi < lo) return lo;
    const std::uint64_t span =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) -
                                   static_cast<std::int64_t>(lo)) +
        1ULL;
    return lo + static_cast<int>(next_range(span));
}

// ── Pool ──────────────────────────────────────────────────────────────

XorShift128 XorShiftPool::s_instances[XorShiftPool::kInstanceCount];
bool XorShiftPool::s_seeded = false;

XorShift128& XorShiftPool::instance(XorShiftInstance idx) noexcept {
    auto i = static_cast<std::size_t>(idx);
    if (i >= kInstanceCount) i = 0;
    return s_instances[i];
}

void XorShiftPool::seed_all(std::uint64_t baseSeed, bool force) noexcept {
    if (s_seeded && !force) return;

    std::uint64_t state = baseSeed ? baseSeed : entropy_fallback();
    for (std::size_t i = 0; i < kInstanceCount; ++i) {
        // Decorrelate streams with per-lane salt.
        std::uint64_t a = splitmix64(state) ^ (0xD1B54A32D192ED03ULL * (i + 1));
        std::uint64_t b = splitmix64(state) ^ (0x94D049BB133111EBULL * (i + 1));
        s_instances[i].seed(a, b);
    }
    s_seeded = true;
}

void XorShiftPool::seed_from_build_keys() noexcept {
    std::uint64_t base = entropy_fallback();
#if defined(AC_SIM_HAS_BUILD_KEYS)
    base ^= static_cast<std::uint64_t>(build::kXorKeySeed);
    base ^= static_cast<std::uint64_t>(build::kTimingSalt) << 1;
    base ^= static_cast<std::uint64_t>(build::kCompileSalt);
#endif
    seed_all(base, /*force=*/true);
}

} // namespace sim
