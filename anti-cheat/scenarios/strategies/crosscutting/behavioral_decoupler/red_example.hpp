#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::behavioral_decoupler {

struct RedResult {
    bool active{false};
    std::uint64_t frames_skipped{0};
    std::uint64_t entities_omitted{0};
    std::uint64_t entities_fuzzed{0};
    double avg_latency{0.0};
    double risk_score{0.0};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Behavioral decoupling: frame skip, entity omission, blind spots, position fuzzing, VACnet risk scoring";
};

} // namespace
