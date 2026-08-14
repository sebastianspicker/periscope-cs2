#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::adaptive_strategy {

struct BlueResult {
    bool detected_tier_hopping{false};
    bool detected_opsec_pattern{false};
    bool detected_c2_fallback{false};
    int detection_count{0};
    std::string detail;
};

class Blue {
public:
    BlueResult detect(const sim::World& w) noexcept;
    BlueResult mitigate(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Detects adaptive multi-tier evasion via tier-hopping, opsec pattern analysis, and C2 fallback monitoring";
};

} // namespace
