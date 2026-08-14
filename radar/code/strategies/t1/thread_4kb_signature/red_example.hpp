#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t1_thread_4kb_signature {

struct RedResult {
    bool start_in_legit_module{false};
    bool code_clean{false};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription = "Create thread with 4KB of clean code at start address, no suspicious patterns";
};

} // namespace
