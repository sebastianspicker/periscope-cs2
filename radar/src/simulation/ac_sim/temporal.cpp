#include "ac_sim/temporal.hpp"

#include "ac_sim/xorshift.hpp"
#include <cmath>

namespace sim {
namespace {

// Phase periods (ticks) with overlapping ranges — no prime-number tells.
constexpr std::uint64_t kPhasePeriodMin[4] = {180, 200, 220, 240};
constexpr std::uint64_t kPhasePeriodMax[4] = {260, 290, 320, 350};

// Base delay means (µs) per phase.
constexpr float kPhaseMeanUs[4] = {100.f, 160.f, 220.f, 140.f};
constexpr float kPhaseStdUs[4]  = {25.f,  40.f,  70.f,  50.f};

} // namespace

void TemporalEngine::initialize(std::uint64_t seed) noexcept {
    m_seed = seed ? seed : 0xC0FFEEULL;
    m_rng = m_seed;
    m_tick = 0;
    m_phase = TemporalPhase::Alpha;
    m_hasSpare = false;
    m_spareNormal = 0.f;
    pick_phase_duration();
}

std::uint64_t TemporalEngine::next_u64() noexcept {
    if (XorShiftPool::seeded()) {
        // Mix phase into the temporal stream without permanently mutating pool order
        // beyond normal consumption — temporal is the designated consumer.
        return XorShiftPool::instance(XorShiftInstance::Temporal).next() ^
               (static_cast<std::uint64_t>(m_phase) << 48) ^ m_tick;
    }
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 7;
    m_rng ^= m_rng << 17;
    if (m_rng == 0) m_rng = m_seed | 1ULL;
    return m_rng;
}

float TemporalEngine::next_float() noexcept {
    return static_cast<float>((next_u64() >> 40) & 0xFFFFFFu) / 16777216.0f;
}

void TemporalEngine::tick() noexcept {
    ++m_tick;
    if (m_phaseRemaining == 0 || --m_phaseRemaining == 0) {
        advance_phase();
        pick_phase_duration();
    }
}

void TemporalEngine::pick_phase_duration() noexcept {
    const int idx = static_cast<int>(m_phase);
    const std::uint64_t range = kPhasePeriodMax[idx] - kPhasePeriodMin[idx];
    const std::uint64_t base =
        kPhasePeriodMin[idx] + (range > 0 ? next_u64() % (range + 1) : 0);

    // ±10% jitter on duration.
    const int jitterPct = static_cast<int>(next_u64() % 21) - 10;
    std::int64_t jittered =
        static_cast<std::int64_t>(base) * (100 + jitterPct) / 100;
    if (jittered < 1) jittered = 1;
    m_phaseRemaining = static_cast<std::uint64_t>(jittered);
}

void TemporalEngine::advance_phase() noexcept {
    switch (m_phase) {
    case TemporalPhase::Alpha: m_phase = TemporalPhase::Beta;  break;
    case TemporalPhase::Beta:  m_phase = TemporalPhase::Gamma; break;
    case TemporalPhase::Gamma: m_phase = TemporalPhase::Delta; break;
    case TemporalPhase::Delta: m_phase = TemporalPhase::Alpha; break;
    }
}

float TemporalEngine::normal_random(float mean, float stddev) noexcept {
    if (m_hasSpare) {
        m_hasSpare = false;
        return mean + stddev * m_spareNormal;
    }
    float u1 = next_float();
    float u2 = next_float();
    if (u1 < 1e-12f) u1 = 1e-12f;
    const float r = std::sqrt(-2.0f * std::log(u1));
    const float theta = 6.28318530718f * u2;
    m_spareNormal = r * std::sin(theta);
    m_hasSpare = true;
    return mean + stddev * (r * std::cos(theta));
}

float TemporalEngine::gamma_random(float shape, float scale) noexcept {
    // Marsaglia-Tsang for shape >= 1; for shape < 1 fall back to sum of exponentials.
    if (shape < 1.0f) shape = 1.0f;
    // Approximate Gamma(k, θ) as sum of k Exp(θ) for integer-ish shapes used here.
    const int k = static_cast<int>(shape + 0.5f);
    float sum = 0.f;
    for (int i = 0; i < k; ++i) {
        float u = next_float();
        if (u < 1e-12f) u = 1e-12f;
        sum += -std::log(u) * scale;
    }
    return sum;
}

std::uint32_t TemporalEngine::next_delay_us() noexcept {
    const int idx = static_cast<int>(m_phase);
    float delay = 0.f;

    switch (m_phase) {
    case TemporalPhase::Alpha:
        // Steady low-jitter cadence.
        delay = normal_random(kPhaseMeanUs[idx], kPhaseStdUs[idx]);
        break;
    case TemporalPhase::Beta:
        // Right-skewed reaction-time-like delays.
        delay = gamma_random(2.0f, 50.0f);
        break;
    case TemporalPhase::Gamma:
        // Alternating burst / quiet (confuses fixed-period detectors).
        delay = (m_tick & 1)
                    ? normal_random(280.f, 60.f)
                    : normal_random(40.f, 12.f);
        break;
    case TemporalPhase::Delta:
        // Wide dispersion phase.
        delay = std::abs(normal_random(kPhaseMeanUs[idx], kPhaseStdUs[idx]));
        break;
    }

    if (delay < 10.f) delay = 10.f;
    if (delay > 10000.f) delay = 10000.f;
    return static_cast<std::uint32_t>(delay);
}

void TemporalEngine::busy_wait_us(std::uint32_t us) noexcept {
    if (us == 0) return;
    std::uint64_t spins = static_cast<std::uint64_t>(us) * 50ULL;
    while (spins != 0) --spins;
}

std::size_t TemporalEngine::random_batch_size(std::size_t minSize,
                                             std::size_t maxSize) noexcept {
    if (minSize >= maxSize) return minSize;

    // Phase modulates the mean: Alpha/Delta prefer mid, Gamma prefers small bursts.
    float mean = static_cast<float>(minSize + maxSize) * 0.5f;
    float stddev = static_cast<float>(maxSize - minSize) * 0.25f;
    switch (m_phase) {
    case TemporalPhase::Gamma:
        mean = static_cast<float>(minSize) +
               static_cast<float>(maxSize - minSize) * 0.3f;
        break;
    case TemporalPhase::Beta:
        mean = static_cast<float>(minSize) +
               static_cast<float>(maxSize - minSize) * 0.7f;
        break;
    default:
        break;
    }

    float n = normal_random(mean, stddev);
    std::size_t size = static_cast<std::size_t>(n + 0.5f);
    if (size < minSize) size = minSize;
    if (size > maxSize) size = maxSize;
    return size;
}

int TemporalEngine::suggested_rps(int minRps, int maxRps) noexcept {
    if (minRps > maxRps) {
        const int t = minRps;
        minRps = maxRps;
        maxRps = t;
    }
    // Alpha: mid-high, Beta: high, Gamma: low (bursty elsewhere), Delta: mid.
    float t = 0.5f;
    switch (m_phase) {
    case TemporalPhase::Alpha: t = 0.65f; break;
    case TemporalPhase::Beta:  t = 0.85f; break;
    case TemporalPhase::Gamma: t = 0.25f; break;
    case TemporalPhase::Delta: t = 0.50f; break;
    }
    // Small noise.
    t += (next_float() - 0.5f) * 0.1f;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    const int rps =
        minRps + static_cast<int>(t * static_cast<float>(maxRps - minRps) + 0.5f);
    return rps;
}

} // namespace sim
