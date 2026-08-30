#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::behavioral_decoupler {

struct BlueResult {
    bool detected_skip_patterns{false};
    bool detected_latency_consistency{false};
    bool detected_fuzz_pattern{false};
    bool detected_vacnet_risk{false};
    int detection_count{0};
    std::string detail;
};

class Blue {
public:
    BlueResult detect(const sim::World& w) noexcept;
    BlueResult mitigate(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Detects behavioral decoupling via unnatural skip patterns, latency consistency, fuzz pattern detection, and VACnet risk scoring";
};

} // namespace
