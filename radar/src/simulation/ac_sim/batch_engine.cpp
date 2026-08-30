#include "ac_sim/batch_engine.hpp"

#include "ac_sim/xorshift.hpp"
#include <algorithm>
#include <cstring>

namespace sim {
namespace {

constexpr std::uint64_t kDecoyBase = 0x00007FF000000000ULL;

} // namespace

std::uint64_t BatchEngine::next_u64() noexcept {
    // Prefer pool stream when seeded; fall back to local state.
    if (XorShiftPool::seeded()) {
        return XorShiftPool::instance(XorShiftInstance::Batch).next();
    }
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 7;
    m_rng ^= m_rng << 17;
    if (m_rng == 0) m_rng = 0xA5A5A5A5A5A5A5A5ULL;
    return m_rng;
}

bool BatchEngine::stage(std::uint64_t addr, void* buf, std::size_t size) noexcept {
    if (m_count >= kMaxBatchSize) return false;
    if (!buf || size == 0 || addr == 0) return false;

    m_requests[m_count].address = addr;
    m_requests[m_count].buffer = buf;
    m_requests[m_count].size = size;
    m_requests[m_count].isDecoy = false;
    ++m_count;
    ++m_stats.staged;
    return true;
}

bool BatchEngine::stage_decoy(std::uint64_t addr, void* buf, std::size_t size) noexcept {
    if (m_count >= kMaxBatchSize) return false;
    if (size == 0) size = 8;
    if (!buf) {
        buf = m_decoyScratch;
        if (size > sizeof(m_decoyScratch)) size = sizeof(m_decoyScratch);
    }
    if (addr == 0) {
        // Plausible user-space decoy address derived from RNG.
        addr = kDecoyBase + (next_u64() & 0x0000000FFFFF0000ULL);
    }

    m_requests[m_count].address = addr;
    m_requests[m_count].buffer = buf;
    m_requests[m_count].size = size;
    m_requests[m_count].isDecoy = true;
    ++m_count;
    ++m_stats.decoys;
    return true;
}

void BatchEngine::set_rate_limit(int rps) noexcept {
    if (rps < kMinRps) rps = kMinRps;
    if (rps > kMaxRps) rps = kMaxRps;
    m_rps = rps;
}

void BatchEngine::set_jitter_params(float baseMs, float varianceMs) noexcept {
    m_jitterBaseMs = baseMs < 0.f ? 0.f : baseMs;
    m_jitterVarianceMs = varianceMs < 0.f ? 0.f : varianceMs;
}

void BatchEngine::set_decoy_ratio(float ratio) noexcept {
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 0.5f) ratio = 0.5f; // hard cap — too many decoys is a signal
    m_decoyRatio = ratio;
}

void BatchEngine::clear() noexcept {
    m_count = 0;
    std::memset(m_requests, 0, sizeof(m_requests));
}

void BatchEngine::inject_decoys() noexcept {
    if (m_decoyRatio <= 0.f || m_count == 0) return;
    if (m_count >= kMaxBatchSize) return;

    const std::size_t want =
        static_cast<std::size_t>(static_cast<float>(m_count) * m_decoyRatio + 0.5f);
    const std::size_t room = kMaxBatchSize - m_count;
    const std::size_t n = want < room ? want : room;
    for (std::size_t i = 0; i < n; ++i) {
        stage_decoy(0, nullptr, 8);
    }
}

void BatchEngine::shuffle_requests() noexcept {
    // Fisher-Yates using batch RNG stream.
    for (std::size_t i = m_count; i > 1; --i) {
        const std::size_t j = static_cast<std::size_t>(next_u64() % i);
        const Request tmp = m_requests[i - 1];
        m_requests[i - 1] = m_requests[j];
        m_requests[j] = tmp;
    }
}

void BatchEngine::micro_busy_wait_us(std::uint32_t us) noexcept {
    if (us == 0) return;
    std::uint64_t spins = static_cast<std::uint64_t>(us) * 50ULL;
    while (spins != 0) --spins;
}

void BatchEngine::adaptive_delay() noexcept {
    if (m_rps <= 0) return;

    // Inter-read period from RPS, plus configured jitter.
    float periodMs = 1000.0f / static_cast<float>(m_rps);
    if (m_jitterBaseMs > 0.f) {
        periodMs = m_jitterBaseMs;
    }
    const float span = m_jitterVarianceMs > 0.f ? m_jitterVarianceMs : periodMs * 0.2f;
    const float u = static_cast<float>(next_u64() & 0xFFFF) / 65535.0f; // [0,1]
    const float jitter = (u - 0.5f) * 2.0f * span;
    float delayMs = periodMs + jitter;
    if (delayMs < 0.05f) delayMs = 0.05f;

    micro_busy_wait_us(static_cast<std::uint32_t>(delayMs * 1000.0f));
}

std::size_t BatchEngine::execute(bool (*readFn)(std::uint64_t, void*, std::size_t)) noexcept {
    if (!readFn || m_count == 0) return 0;

    inject_decoys();
    shuffle_requests();

    // Randomly drop 0–2 real reads to break fixed-length batch fingerprints.
    bool dropped[kMaxBatchSize] = {};
    std::size_t dropCount = 0;
    if (m_dropEnabled && m_count > 2) {
        dropCount = 1 + static_cast<std::size_t>(next_u64() % 2);
        if (dropCount > m_count / 2) dropCount = m_count / 2;
        for (std::size_t d = 0; d < dropCount; ++d) {
            const std::size_t idx = static_cast<std::size_t>(next_u64() % m_count);
            dropped[idx] = true;
        }
    }

    std::size_t success = 0;
    for (std::size_t i = 0; i < m_count; ++i) {
        if (dropped[i]) {
            ++m_stats.dropped;
            continue;
        }

        const Request& req = m_requests[i];
        ++m_stats.executed;

        // Decoy reads still hit the memory path (camouflage) but do not
        // count toward the success return value.
        const bool ok = readFn(req.address, req.buffer, req.size);
        if (ok && !req.isDecoy) {
            ++success;
            ++m_stats.succeeded;
        }

        if (i + 1 < m_count) {
            adaptive_delay();
        }
    }

    ++m_stats.batches;
    clear();
    return success;
}

} // namespace sim
