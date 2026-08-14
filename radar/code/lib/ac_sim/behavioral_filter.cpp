#include "ac_sim/behavioral_filter.hpp"

#include "real/platform.hpp"

#include <cmath>
#include <cstring>

#if LR_PLATFORM_WINDOWS
#  include "real/win/timing.hpp"
#endif

namespace sim {

float GaussianNoise::uniform() noexcept {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    if (state == 0) state = 0xA5A5A5A5A5A5A5A5ULL;
    return static_cast<float>((state >> 40) & 0xFFFFFFu) / 16777216.0f;
}

float GaussianNoise::sample(float mean, float stddev) noexcept {
    float u1 = uniform();
    float u2 = uniform();
    if (u1 <= 0.f) u1 = 1.f / 16777216.f;
    if (u2 <= 0.f) u2 = 1.f / 16777216.f;
    const float r = std::sqrt(-2.0f * std::log(u1));
    const float theta = 6.2831853f * u2;
    return mean + stddev * r * std::cos(theta);
}

void BehavioralFilter::initialize(const BehavioralFilterConfig& config,
                                   std::uint64_t seed) noexcept {
    m_config = config;
    m_seed = seed ? seed : 0xBE11A710ULL;
    m_tickCount = 0;
    m_hasLastOrigin = false;
    m_stats = BehavioralFilterStats{};
    std::memset(m_firstSeenTick, 0, sizeof(m_firstSeenTick));
}

void BehavioralFilter::omit_entity(ac::EntitySnapshot& e) noexcept {
    if (!m_config.mutateEntities) return;
    e.dormant = true;
    e.alive = false;
    e.health = 0;
    ++m_stats.entitiesOmitted;
}

void BehavioralFilter::fuzz_origin(ac::EntitySnapshot& e, float distance,
                                   GaussianNoise& rng) noexcept {
    if (!m_config.mutateEntities) return;
    // Distance-tiered world-space fuzz (metres-ish).
    float sigma = 2.0f;
    if (distance > m_config.nearDist) sigma = 5.0f;
    if (distance > m_config.midDist) sigma = 12.0f;
    if (distance > m_config.farDist) sigma = 25.0f;
    e.origin.x += rng.sample(0.f, sigma);
    e.origin.y += rng.sample(0.f, sigma);
    e.origin.z += rng.sample(0.f, sigma * 0.3f);
    ++m_stats.entitiesFuzzed;
}

bool BehavioralFilter::should_omit_entity(float distance, float player_speed,
                                          bool is_occluded,
                                          GaussianNoise& rng) noexcept {
    float base = static_cast<float>(m_config.omissionRate);
    if (distance > m_config.nearDist) base = 0.02f;
    if (distance > m_config.midDist) base = 0.08f;
    if (distance > m_config.farDist) base = 0.15f;

    float speed_factor = 1.0f + (player_speed / 250.0f) * 2.0f;
    float occlusion_factor =
        is_occluded ? static_cast<float>(m_config.occlusionBoost) : 1.0f;

    float rate = base * speed_factor * occlusion_factor;
    if (rate > 0.35f) rate = 0.35f;
    return rng.uniform() < rate;
}

void BehavioralFilter::filter_entities(ac::EntitySnapshot* entities,
                                       std::size_t count,
                                       const ac::Vec3& localOrigin,
                                       float player_speed, bool is_occluded,
                                       float view_angle_h,
                                       float view_angle_v) noexcept {
    if (!entities || count == 0) return;

    ++m_tickCount;
    ++m_stats.ticks;
    m_stats.entitiesSeen += count;

    float movement_speed = player_speed;
    if (m_hasLastOrigin) {
        const float dx = localOrigin.x - m_lastOrigin.x;
        const float dy = localOrigin.y - m_lastOrigin.y;
        const float dz = localOrigin.z - m_lastOrigin.z;
        const float frame_speed = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (frame_speed > movement_speed) movement_speed = frame_speed;
    }
    m_lastOrigin = localOrigin;
    m_hasLastOrigin = true;

    // Gamma(4, ~50–100) reaction-time delay — mean ~200–400 ms.
    GaussianNoise rng(m_tickCount ^ m_seed);
    float gamma_sum = 0.f;
    const float scale =
        static_cast<float>((m_config.delayMsMin + m_config.delayMsMax) * 0.25);
    for (int gi = 0; gi < 4; ++gi) {
        float u = rng.uniform();
        if (u < 0.0001f) u = 0.0001f;
        gamma_sum += -std::log(u) * (scale / 4.0f);
    }
    float base_delay = gamma_sum;
    if (base_delay < static_cast<float>(m_config.delayMsMin))
        base_delay = static_cast<float>(m_config.delayMsMin);
    if (base_delay > static_cast<float>(m_config.delayMsMax) * 2.0f)
        base_delay = static_cast<float>(m_config.delayMsMax) * 2.0f;

    if (is_occluded) {
        base_delay *= (1.0f + static_cast<float>(m_config.occlusionBoost) * 0.5f);
    }
    float move_factor = 1.0f + (movement_speed / 500.0f);
    if (move_factor > 2.0f) move_factor = 2.0f;
    m_stats.lastDelayMs = static_cast<double>(base_delay * move_factor);

#if LR_PLATFORM_WINDOWS
    if (m_config.applyDelay && m_stats.lastDelayMs > 0.0) {
        real::win::fuzzed_sleep(static_cast<std::int32_t>(m_stats.lastDelayMs),
                                20);
    }
#else
    (void)m_config.applyDelay;
#endif

    const float half_fov_h = view_angle_h * 0.5f;
    const float half_fov_v = view_angle_v * 0.5f;

    for (std::size_t i = 0; i < count; ++i) {
        auto& e = entities[i];
        if (e.is_local_player) continue;

        const float dx = e.origin.x - localOrigin.x;
        const float dy = e.origin.y - localOrigin.y;
        const float dz = e.origin.z - localOrigin.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        const float angle_h = std::atan2(dy, dx);
        const float angle_v =
            std::atan2(dz, std::sqrt(dx * dx + dy * dy) + 1e-6f);
        const bool in_fov = (std::fabs(angle_h) < half_fov_h) &&
                            (std::fabs(angle_v) < half_fov_v);

        // Outside FoV — elevated omission.
        if (!in_fov && m_tickCount > 10) {
            if (rng.uniform() < 0.3f) {
                omit_entity(e);
                continue;
            }
        }

        if (should_omit_entity(distance, movement_speed, is_occluded, rng)) {
            omit_entity(e);
            continue;
        }

        // First-seen reaction-time gate (~12 ticks ≈ 200 ms at 60 Hz).
        if (e.id > 0 && e.id < kMaxEntities) {
            auto& entry = m_firstSeenTick[e.id];
            if (entry == 0) entry = m_tickCount;
            const std::uint64_t age = m_tickCount - entry;
            if (age < 12 && !is_occluded) {
                omit_entity(e);
                continue;
            }
        }

        // Peripheral vision: near FoV edge, sometimes omit.
        if (in_fov) {
            const float angle_dist =
                std::sqrt(angle_h * angle_h + angle_v * angle_v);
            if (angle_dist > 0.4f && rng.uniform() < 0.15f) {
                omit_entity(e);
                continue;
            }
        }

        // Surviving entities get distance-tiered position fuzz.
        fuzz_origin(e, distance, rng);
    }
}

void BehavioralFilter::fuzz_screen(float& screen_x, float& screen_y, float dist,
                                   bool is_visible,
                                   GaussianNoise& rng) noexcept {
    auto fuzz_coord = [&](float pos) -> float {
        float fuzz_px;
        if (is_visible) {
            fuzz_px = rng.sample(1.0f, 1.0f);
            if (fuzz_px < 0.f) fuzz_px = 0.f;
            if (fuzz_px > 3.f) fuzz_px = 3.f;
        } else {
            float max_fuzz = 3.f + (dist / 100.f) * 5.f;
            if (max_fuzz > 10.f) max_fuzz = 10.f;
            fuzz_px = rng.sample(4.0f, 2.0f);
            if (fuzz_px < 1.f) fuzz_px = 1.f;
            if (fuzz_px > max_fuzz) fuzz_px = max_fuzz;
        }
        return pos + (rng.uniform() < 0.5f ? fuzz_px : -fuzz_px);
    };
    screen_x = fuzz_coord(screen_x);
    screen_y = fuzz_coord(screen_y);
}

void BehavioralFilter::reset() noexcept {
    m_tickCount = 0;
    m_hasLastOrigin = false;
    m_stats = BehavioralFilterStats{};
    std::memset(m_firstSeenTick, 0, sizeof(m_firstSeenTick));
}

} // namespace sim
