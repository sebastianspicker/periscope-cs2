#pragma once
#include "real/error.hpp"
#include <cstdint>

namespace real {

// Individual subsystem lifecycle handle.
// Wraps a subsystem with status tracking, dependency ordering, and error propagation.
struct SubsystemHandle {
    const char* name;
    bool (*init_fn)() noexcept;  // returns true on success
    void (*shutdown_fn)() noexcept;
    bool (*tick_fn)(uint64_t frame) noexcept;  // optional per-frame tick
    int dependency_count;
    const char** dependencies;  // names of subsystems that must init first

    bool initialized = false;
    bool tick_enabled = true;
};

} // namespace real
