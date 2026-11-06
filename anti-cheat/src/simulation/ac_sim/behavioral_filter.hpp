#pragma once

// Behavioral filter — ResearchSafe profile for VACnet camouflage.
// Delay 200–400 ms (Gamma reaction-time), probabilistic omission (~3.5%/s
// base, distance-tiered), screen-space Gaussian fuzz, first-seen reaction model.
//
// CC-ledger CC.13

#include "ac/types.hpp"

#include <cstddef>
#include <cstdint>

namespace sim {

struct BehavioralFilterConfig {
    double delayMsMin{200.0};
    double delayMsMax{400.0};
    double omissionRate{0.035};     // base per-entity omit probability
    double occlusionBoost{2.0};     // multiplier when occluded
    bool applyDelay{true};          // set false in unit tests to avoid sleep
    bool mutateEntities{true};      // mark omitted as dormant / fuzz origins
    float nearDist{15.f};
    float midDist{50.f};
    float farDist{80.f};
};

struct GaussianNoise {
    std::uint64_t state{1};

    explicit GaussianNoise(std::uint64_t seed) noexcept
        : state(seed ? seed : 1) {}

    float sample(float mean, float stddev) noexcept;
    float uniform() noexcept;
};

struct BehavioralFilterStats {
    std::uint64_t ticks{0};
    std::uint64_t entitiesSeen{0};
    std::uint64_t entitiesOmitted{0};
    std::uint64_t entitiesFuzzed{0};
    double lastDelayMs{0.0};
};

class BehavioralFilter {
public:
    void initialize(const BehavioralFilterConfig& config,
                    std::uint64_t seed) noexcept;

    /// Filter an entity array in place. Omitted entities are marked dormant
    /// (and health cleared) when mutateEntities is set. Positions may be fuzzed.
    void filter_entities(ac::EntitySnapshot* entities, std::size_t count,
                         const ac::Vec3& localOrigin, float player_speed,
                         bool is_occluded, float view_angle_h,
                         float view_angle_v) noexcept;

    void fuzz_screen(float& screen_x, float& screen_y, float dist,
                     bool is_visible, GaussianNoise& rng) noexcept;

    void reset() noexcept;

    const BehavioralFilterStats& stats() const noexcept { return m_stats; }
    const BehavioralFilterConfig& config() const noexcept { return m_config; }
    double last_delay_ms() const noexcept { return m_stats.lastDelayMs; }

private:
    bool should_omit_entity(float distance, float player_speed, bool is_occluded,
                            GaussianNoise& rng) noexcept;

    void omit_entity(ac::EntitySnapshot& e) noexcept;
    void fuzz_origin(ac::EntitySnapshot& e, float distance,
                     GaussianNoise& rng) noexcept;

    BehavioralFilterConfig m_config{};
    std::uint64_t m_seed{0};
    std::uint64_t m_tickCount{0};
    BehavioralFilterStats m_stats{};
    static constexpr std::size_t kMaxEntities = 64;
    std::uint64_t m_firstSeenTick[kMaxEntities]{};
    ac::Vec3 m_lastOrigin{};
    bool m_hasLastOrigin{false};
};

} // namespace sim
