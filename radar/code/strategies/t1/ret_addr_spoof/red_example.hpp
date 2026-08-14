#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t1_ret_addr_spoof {

struct RedResult {
    bool start_address_clean{false};
    bool chain_spoofed{false};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Create thread with clean start address but return address chain leads back to red's code";
};

} // namespace
