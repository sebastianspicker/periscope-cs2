#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t1_bsecure_allowed_evade {

struct RedResult {
    bool module_stomped{false};
    bool signed_module{false};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Load module that passes BSecureAllowed trust validation using module stomping or signed BYOVD";
};

} // namespace
