#pragma once

// 4-phase temporal jitter engine (Alpha/Beta/Gamma/Delta).
// Box-Muller / Gamma distributions, QPC busy-wait, phase-aware batch sizing.
//
// CC-ledger CC.3  |  Reference: Periscope temporal.hpp

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace sim {

enum class TemporalPhase : std::uint8_t {
    Alpha = 0,
    Beta  = 1,
    Gamma = 2,
    Delta = 3
};

class TemporalEngine {
public:
    void initialize(std::uint64_t seed) noexcept;
    void tick() noexcept;

    TemporalPhase phase() const noexcept { return m_phase; }
    std::uint64_t tick_count() const noexcept { return m_tick; }
    std::uint64_t phase_remaining() const noexcept { return m_phaseRemaining; }

    /// Phase-conditioned inter-op delay in microseconds.
    std::uint32_t next_delay_us() noexcept;

    /// QPC busy-wait (no Sleep / NtDelayExecution in the hot path).
    void busy_wait_us(std::uint32_t us) noexcept;

    /// Convenience: wait next_delay_us().
    void wait_next() noexcept { busy_wait_us(next_delay_us()); }

    template <typename T>
    void shuffle(std::span<T> items) noexcept;

    std::size_t random_batch_size(std::size_t minSize, std::size_t maxSize) noexcept;

    /// Adaptive RPS suggestion for BatchEngine based on current phase.
    int suggested_rps(int minRps = 48, int maxRps = 120) noexcept;

private:
    TemporalPhase m_phase{TemporalPhase::Alpha};
    std::uint64_t m_tick{0};
    std::uint64_t m_seed{0};
    std::uint64_t m_phaseRemaining{0};
    std::uint64_t m_rng{0};
    float m_spareNormal{0.f};
    bool m_hasSpare{false};

    std::uint64_t next_u64() noexcept;
    float next_float() noexcept;
    float normal_random(float mean, float stddev) noexcept;
    float gamma_random(float shape, float scale) noexcept;
    void advance_phase() noexcept;
    void pick_phase_duration() noexcept;
};

template <typename T>
void TemporalEngine::shuffle(std::span<T> items) noexcept {
    if (items.size() <= 1) return;
    for (std::size_t i = items.size(); i > 1; --i) {
        const std::size_t j = static_cast<std::size_t>(next_u64() % i);
        std::swap(items[i - 1], items[j]);
    }
    // Tiny post-shuffle wait breaks constant-time shuffle fingerprints.
    busy_wait_us(next_delay_us() & 0x1Fu);
}

} // namespace sim
