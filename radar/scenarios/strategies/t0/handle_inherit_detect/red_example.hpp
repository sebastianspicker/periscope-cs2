#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t0_handle_inherit_detect {

struct RedResult {
    bool handle_inherited{false};
    uint32_t child_pid{};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription =
        "Creates a child process that inherits the game handle. "
        "No OpenProcess needed in the child — handle inheritance bypasses handle scans on the child.";
};

} // namespace
