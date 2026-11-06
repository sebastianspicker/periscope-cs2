#pragma once

// Batch read engine — aggregates up to 16 memory reads, Fisher-Yates shuffles
// order, injects decoy reads, and enforces an adaptive RPS cap with micro-delays.
//
// CC-ledger CC.2  |  Reference: Periscope MemoryAccessObfuscator

#include <cstddef>
#include <cstdint>

namespace sim {

class BatchEngine {
public:
    static constexpr std::size_t kMaxBatchSize = 16;
    static constexpr int kMinRps = 48;
    static constexpr int kMaxRps = 120;

    struct Request {
        std::uint64_t address{0};
        void* buffer{nullptr};
        std::size_t size{0};
        bool isDecoy{false};
    };

    struct Stats {
        std::uint64_t staged{0};
        std::uint64_t executed{0};
        std::uint64_t succeeded{0};
        std::uint64_t decoys{0};
        std::uint64_t dropped{0};
        std::uint64_t batches{0};
    };

    /// Stage a real read. Returns false if batch is full or args invalid.
    bool stage(std::uint64_t addr, void* buf, std::size_t size) noexcept;

    /// Stage an explicit decoy read (buffer may be scratch; result discarded).
    bool stage_decoy(std::uint64_t addr, void* buf, std::size_t size) noexcept;

    /// Fisher-Yates shuffle, optional random drop, adaptive inter-read delay,
    /// then invoke readFn for each non-decoy, non-dropped request.
    /// Returns number of successful real reads.
    std::size_t execute(bool (*readFn)(std::uint64_t, void*, std::size_t)) noexcept;

    void set_rate_limit(int rps) noexcept;
    void set_jitter_params(float baseMs, float varianceMs) noexcept;
    void set_decoy_ratio(float ratio) noexcept;
    void set_drop_enabled(bool enabled) noexcept { m_dropEnabled = enabled; }
    void set_seed(std::uint64_t seed) noexcept { m_rng = seed ? seed : 1; }

    void clear() noexcept;
    std::size_t pending() const noexcept { return m_count; }
    const Stats& stats() const noexcept { return m_stats; }
    int rate_limit() const noexcept { return m_rps; }
    float decoy_ratio() const noexcept { return m_decoyRatio; }

private:
    Request m_requests[kMaxBatchSize]{};
    std::size_t m_count{0};
    int m_rps{120};
    float m_jitterBaseMs{1.0f};
    float m_jitterVarianceMs{0.5f};
    float m_decoyRatio{0.05f};
    bool m_dropEnabled{true};
    std::uint64_t m_rng{0xDEADBEEFCAFEBABEULL};
    Stats m_stats{};

    // Scratch for decoy reads when caller does not supply buffers.
    std::uint8_t m_decoyScratch[64]{};

    std::uint64_t next_u64() noexcept;
    void shuffle_requests() noexcept;
    void inject_decoys() noexcept;
    void adaptive_delay() noexcept;
    void micro_busy_wait_us(std::uint32_t us) noexcept;
};

} // namespace sim
