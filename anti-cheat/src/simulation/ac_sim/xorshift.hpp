#pragma once

// Multi-instance XorShift128+ RNG pool for the radar evasion stack.
// Five independent streams (temporal / batch / render / semantic / memory)
// keep statistical coupling out of multi-subsystem timing analysis.
//
// Reference: Marsaglia XorShift128+, Vigna xorshift128+
// CC-ledger CC.11

#include <cstddef>
#include <cstdint>

namespace sim {

enum class XorShiftInstance : std::uint8_t {
    Temporal = 0,
    Batch    = 1,
    Render   = 2,
    Semantic = 3,
    Memory   = 4,
    Count    = 5
};

/// XorShift128+ generator — period 2^128 − 1, full 64-bit output.
class XorShift128 {
public:
    void seed(std::uint64_t s0, std::uint64_t s1) noexcept;

    /// Next raw 64-bit sample.
    std::uint64_t next() noexcept;

    /// Uniform integer in [0, max). Rejection-sampled for unbiased ranges.
    /// Returns 0 when max == 0.
    std::uint64_t next_range(std::uint64_t max) noexcept;

    /// Uniform float in [0, 1).
    float next_float() noexcept;

    /// Uniform float in [lo, hi).
    float next_float_range(float lo, float hi) noexcept;

    /// Box-Muller normal sample N(mean, stddev).
    float next_normal(float mean, float stddev) noexcept;

    /// Inclusive integer in [lo, hi]. Returns lo when hi < lo.
    int next_int(int lo, int hi) noexcept;

    std::uint64_t state0() const noexcept { return m_s0; }
    std::uint64_t state1() const noexcept { return m_s1; }

private:
    std::uint64_t m_s0{0xA3B1C5D7E9F0A2B4ULL};
    std::uint64_t m_s1{0xC6D8E0F2A4B6C8D0ULL};
    float m_spareNormal{0.f};
    bool m_hasSpare{false};
};

/// Global pool of five independent generators.
class XorShiftPool {
public:
    static constexpr std::size_t kInstanceCount =
        static_cast<std::size_t>(XorShiftInstance::Count);

    static XorShift128& instance(XorShiftInstance idx) noexcept;

    /// Seed all five streams from a single deterministic base seed.
    /// Subsequent calls re-seed (idempotent only when force=false and already seeded).
    static void seed_all(std::uint64_t baseSeed, bool force = false) noexcept;

    static bool seeded() noexcept { return s_seeded; }

    /// Seed from compile-time build keys when available, else a fixed fallback.
    static void seed_from_build_keys() noexcept;

private:
    static XorShift128 s_instances[kInstanceCount];
    static bool s_seeded;
};

} // namespace sim
