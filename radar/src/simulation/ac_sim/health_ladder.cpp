#include "ac_sim/health_ladder.hpp"

namespace sim {

void HealthLadder::initialize() noexcept {
    m_config = HealthLadderConfig{};
    reset();
}

void HealthLadder::initialize(const HealthLadderConfig& config) noexcept {
    m_config = config;
    if (m_config.softBadFrameThreshold < 1) m_config.softBadFrameThreshold = 1;
    if (m_config.mediumSoftActionThreshold < 1) m_config.mediumSoftActionThreshold = 1;
    if (m_config.hardMediumActionThreshold < 1) m_config.hardMediumActionThreshold = 1;
    if (m_config.goodFramesToDeescalate < 1) m_config.goodFramesToDeescalate = 1;
    reset();
}

void HealthLadder::record_bad_frame() noexcept {
    ++m_stats.badFrames;
    ++m_stats.totalBadFrames;
    ++m_stats.totalFrames;
    m_stats.goodStreak = 0;
}

void HealthLadder::record_good_frame() noexcept {
    ++m_stats.totalFrames;
    ++m_stats.goodStreak;
    // A good frame clears the immediate bad-frame counter but does not
    // erase cumulative soft/medium action history.
    m_stats.badFrames = 0;
    try_deescalate();
}

void HealthLadder::reset_bad_frames() noexcept {
    m_stats.badFrames = 0;
}

void HealthLadder::try_deescalate() noexcept {
    if (m_stats.goodStreak < m_config.goodFramesToDeescalate) return;
    m_stats.goodStreak = 0;
    switch (m_level) {
    case HealthLevel::Hard:
        m_level = HealthLevel::Medium;
        break;
    case HealthLevel::Medium:
        m_level = HealthLevel::Soft;
        break;
    case HealthLevel::Soft:
        m_level = HealthLevel::Healthy;
        break;
    case HealthLevel::Healthy:
        break;
    }
}

void HealthLadder::escalate_from_threshold() noexcept {
    // Called when softBadFrameThreshold is hit.
    m_stats.badFrames = 0;

    switch (m_level) {
    case HealthLevel::Healthy:
        m_level = HealthLevel::Soft;
        ++m_stats.softActions;
        m_lastAction = HealthAction::InvalidateHudRecollect;
        break;

    case HealthLevel::Soft:
        ++m_stats.softActions;
        if (m_stats.softActions >= m_config.mediumSoftActionThreshold) {
            m_level = HealthLevel::Medium;
            m_stats.mediumActions = 0;
            m_lastAction = HealthAction::RescanResolve;
        } else {
            m_lastAction = HealthAction::InvalidateHudRecollect;
        }
        break;

    case HealthLevel::Medium:
        ++m_stats.mediumActions;
        if (m_stats.mediumActions >= m_config.hardMediumActionThreshold) {
            m_level = HealthLevel::Hard;
            ++m_stats.hardActions;
            m_lastAction = HealthAction::ReacquireHijack;
        } else {
            m_lastAction = HealthAction::RescanResolve;
        }
        break;

    case HealthLevel::Hard:
        ++m_stats.hardActions;
        m_lastAction = HealthAction::ReacquireHijack;
        break;
    }
}

HealthAction HealthLadder::evaluate() noexcept {
    if (m_stats.badFrames < m_config.softBadFrameThreshold) {
        m_lastAction = HealthAction::None;
        return HealthAction::None;
    }
    escalate_from_threshold();
    return m_lastAction;
}

void HealthLadder::reset() noexcept {
    m_level = HealthLevel::Healthy;
    m_lastAction = HealthAction::None;
    m_stats = HealthLadderStats{};
}

} // namespace sim
