#include "ac_sim/accept_gate.hpp"

namespace sim {

void AcceptGate::initialize() noexcept {
    m_thresholds = GateThresholds{};
    reset();
}

void AcceptGate::initialize(const GateThresholds& thresholds) noexcept {
    m_thresholds = thresholds;
    if (m_thresholds.requiredConditions < 1) m_thresholds.requiredConditions = 1;
    if (m_thresholds.requiredConditions > 7) m_thresholds.requiredConditions = 7;
    reset();
}

GateConditions AcceptGate::metrics_to_conditions(const GateMetrics& m,
                                                 const GateThresholds& t) noexcept {
    GateConditions c{};
    const bool multi =
        (m.chunkCount >= t.minChunks) && (m.remoteEntityCount >= t.minRemotes);
    const bool ratio = m.chunkRatio >= t.minChunkRatio;

    // ac1: multi-chunk remotes OR ratio alone for simplified callers
    c.multiChunkRemotes = multi;
    c.chunkRatioOk = multi || ratio;

    c.bandEnforced = m.bandEnforced;
    c.cvarWalkOk = m.cvarWalkOk;
    c.cvarInit = m.cvarWalkOk;
    c.overlaySizeOk = m.overlayMinPx >= t.minOverlayPx;
    c.zeroPushRatioOk = m.zeroPushRatio <= t.maxZeroPushRatio;
    c.cvarInitTimeOk = m.cvarInitSeconds <= t.maxCvarInitSeconds;
    c.heartbeatOk = m.heartbeatOk;
    return c;
}

bool AcceptGate::finalize(GateConditions c) noexcept {
    // Normalize aliases: either naming style may be set by callers.
    if (c.cvarWalkOk && !c.cvarInit) c.cvarInit = true;
    if (c.cvarInit && !c.cvarWalkOk) c.cvarWalkOk = true;
    if (c.multiChunkRemotes && !c.chunkRatioOk) c.chunkRatioOk = true;

    m_conditions = c;
    m_metCount = 0;
    m_failedMask = 0;

    // Seven primary conditions matching the original accept gate contract:
    // 0 chunkRatioOk (ac1/ac5 combined for boolean API)
    // 1 bandEnforced
    // 2 cvarInit
    // 3 overlaySizeOk
    // 4 zeroPushRatioOk  — note: original ordered zeroPush before cvarInitTime
    // 5 cvarInitTimeOk
    // 6 multiChunkRemotes OR second ratio check — keep 7 slots:
    //
    // Original boolean API counted exactly these seven fields:
    //   chunkRatioOk, bandEnforced, cvarInit, overlaySizeOk,
    //   zeroPushRatioOk, cvarInitTimeOk, heartbeatOk
    // We preserve that set so radar_pipeline and tests stay compatible.

    auto check = [&](bool ok, int bit) {
        if (ok) {
            ++m_metCount;
        } else {
            m_failedMask |= (1u << static_cast<std::uint32_t>(bit));
        }
    };

    check(c.chunkRatioOk, 0);
    check(c.bandEnforced, 1);
    check(c.cvarInit, 2);
    check(c.overlaySizeOk, 3);
    check(c.zeroPushRatioOk, 4);
    check(c.cvarInitTimeOk, 5);
    check(c.heartbeatOk, 6);

    m_accepted = m_metCount >= m_thresholds.requiredConditions;
    return m_accepted;
}

bool AcceptGate::evaluate(const GateMetrics& metrics) noexcept {
    m_metrics = metrics;
    return finalize(metrics_to_conditions(metrics, m_thresholds));
}

bool AcceptGate::evaluate(GateConditions conditions) noexcept {
    return finalize(conditions);
}

void AcceptGate::reset() noexcept {
    m_conditions = GateConditions{};
    m_metrics = GateMetrics{};
    m_metCount = 0;
    m_accepted = false;
    m_failedMask = 0;
}

} // namespace sim
