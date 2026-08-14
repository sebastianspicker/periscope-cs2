#pragma once

// ACCEPT readiness gate — 7 independent conditions must all pass before the
// radar is considered "live" (ready for overlay push / ranked eligibility).
//
// ac1 multi-chunk with remotes   (chunkRatioOk / multiChunkRemotes)
// ac2 band enforced
// ac3 cvar walk path ok          (cvarInit / cvarWalkOk)
// ac4 overlay >= 280px
// ac5 chunk ratio >= 0.4         (also exposed as multi-chunk metrics)
// ac6 zero-push ratio <= 0.5
// ac7 cvar init <= 15s
//
// CC-ledger CC.6

#include <cstdint>

namespace sim {

struct GateMetrics {
    int chunkCount{0};
    int remoteEntityCount{0};
    bool bandEnforced{false};
    bool cvarWalkOk{false};
    int overlayMinPx{0};
    float chunkRatio{0.f};
    float zeroPushRatio{1.f};
    float cvarInitSeconds{1e9f};
    bool heartbeatOk{false};
};

/// Boolean condition set. Field names keep radar_pipeline / legacy call sites working.
struct GateConditions {
    bool chunkRatioOk{false};     // ac1 (and ac5 when metrics-driven)
    bool bandEnforced{false};     // ac2
    bool cvarInit{false};         // ac3 (alias of cvar walk OK)
    bool overlaySizeOk{false};    // ac4
    bool zeroPushRatioOk{false};  // ac6
    bool cvarInitTimeOk{false};   // ac7
    bool heartbeatOk{false};      // required companion

    // Extended names (same storage via helpers in evaluate path).
    bool multiChunkRemotes{false}; // explicit ac1 when using metrics
    bool cvarWalkOk{false};        // explicit ac3
};

struct GateThresholds {
    int minChunks{2};
    int minRemotes{1};
    int minOverlayPx{280};
    float minChunkRatio{0.4f};
    float maxZeroPushRatio{0.5f};
    float maxCvarInitSeconds{15.f};
    int requiredConditions{7};
};

class AcceptGate {
public:
    void initialize() noexcept;
    void initialize(const GateThresholds& thresholds) noexcept;

    bool evaluate(const GateMetrics& metrics) noexcept;
    bool evaluate(GateConditions conditions) noexcept;

    GateConditions last_conditions() const noexcept { return m_conditions; }
    GateMetrics last_metrics() const noexcept { return m_metrics; }
    GateThresholds thresholds() const noexcept { return m_thresholds; }

    int met_count() const noexcept { return m_metCount; }
    int required_count() const noexcept { return m_thresholds.requiredConditions; }
    bool accepted() const noexcept { return m_accepted; }
    std::uint32_t failed_mask() const noexcept { return m_failedMask; }

    void reset() noexcept;

private:
    GateConditions m_conditions{};
    GateMetrics m_metrics{};
    GateThresholds m_thresholds{};
    int m_metCount{0};
    bool m_accepted{false};
    std::uint32_t m_failedMask{0};

    static GateConditions metrics_to_conditions(const GateMetrics& m,
                                                const GateThresholds& t) noexcept;
    bool finalize(GateConditions c) noexcept;
};

} // namespace sim
