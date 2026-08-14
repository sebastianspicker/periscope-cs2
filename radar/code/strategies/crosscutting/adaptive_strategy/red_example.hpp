#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::adaptive_strategy {

struct RedResult {
    bool active{false};
    std::string chosen_tier;
    int sensors_profiled{0};
    double evasion_probability{0.0};
    bool multi_tick_opsec{false};
    bool c2_resilient{false};
    std::vector<std::string> recommended;
    std::vector<std::string> avoided;
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Adaptive multi-tier strategy: probes blue sensors, selects optimal evasion tier, varies patterns across ticks, manages C2 resilience";
};

} // namespace
