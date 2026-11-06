#pragma once

// Runtime health ladder — 3-level healing escalator.
// Soft  : 3 bad frames   → invalidate HUD + recollect
// Medium: 10 soft acts   → re-scan patterns + re-resolve schema
// Hard  : 3 medium acts  → re-acquire hijack + full rescan
//
// Good frames de-escalate one level after a quiet stretch.
//
// CC-ledger CC.5

#include <cstdint>

namespace sim {

struct HealthLadderConfig {
    int softBadFrameThreshold{3};
    int mediumSoftActionThreshold{10};
    int hardMediumActionThreshold{3};
    int goodFramesToDeescalate{30};
};

enum class HealthLevel : std::uint8_t {
    Healthy = 0,
    Soft    = 1,
    Medium  = 2,
    Hard    = 3
};

enum class HealthAction : std::uint8_t {
    None = 0,
    InvalidateHudRecollect,
    RescanResolve,
    ReacquireHijack
};

struct HealthLadderStats {
    int badFrames{0};
    int softActions{0};
    int mediumActions{0};
    int hardActions{0};
    int goodStreak{0};
    int totalFrames{0};
    int totalBadFrames{0};
};

class HealthLadder {
public:
    void initialize() noexcept;
    void initialize(const HealthLadderConfig& config) noexcept;

    void record_bad_frame() noexcept;
    void record_good_frame() noexcept;
    void reset_bad_frames() noexcept;

    HealthLevel current_level() const noexcept { return m_level; }
    HealthAction last_action() const noexcept { return m_lastAction; }
    const HealthLadderStats& stats() const noexcept { return m_stats; }
    const HealthLadderConfig& config() const noexcept { return m_config; }

    /// Evaluate after frames have been recorded. May escalate and return action.
    HealthAction evaluate() noexcept;

    void reset() noexcept;

private:
    HealthLadderConfig m_config{};
    HealthLevel m_level{HealthLevel::Healthy};
    HealthAction m_lastAction{HealthAction::None};
    HealthLadderStats m_stats{};

    void escalate_from_threshold() noexcept;
    void try_deescalate() noexcept;
};

} // namespace sim
