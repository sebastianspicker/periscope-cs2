#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t1_etw_dual_provider {

struct RedResult {
    bool ti_blinded{false};
    bool secondary_alive{false};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Blind primary ETW TI provider, unaware of secondary redundant providers";
};

} // namespace
