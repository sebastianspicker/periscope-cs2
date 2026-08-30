#pragma once
#include "subsystem_handle.hpp"
#include <cstddef>
#include <cstring>

namespace real {

// Dependency-ordered initialization orchestrator.
// Tracks init status per subsystem and provides clean error propagation.
struct InitOrchestrator {
    static constexpr std::size_t kMaxSubsystems = 32;
    SubsystemHandle* subsystems[kMaxSubsystems]{};
    std::size_t count = 0;

    void add(SubsystemHandle* handle) noexcept {
        if (count < kMaxSubsystems) {
            subsystems[count++] = handle;
        }
    }

    // Returns false if any init_fn fails. Dependencies are resolved via
    // iterative passes until all subsystems are either initialized or stuck.
    bool initialize_all() noexcept {
        bool progress = true;
        while (progress) {
            progress = false;
            for (std::size_t i = 0; i < count; i++) {
                auto* h = subsystems[i];
                if (h->initialized) continue;

                bool deps_met = true;
                for (int j = 0; j < h->dependency_count; j++) {
                    if (!is_initialized(h->dependencies[j])) {
                        deps_met = false;
                        break;
                    }
                }
                if (!deps_met) continue;

                if (h->init_fn && !h->init_fn()) {
                    return false;
                }
                h->initialized = true;
                progress = true;
            }
        }
        // Check that at least the non-dependent ones succeeded
        for (std::size_t i = 0; i < count; i++) {
            if (!subsystems[i]->initialized && subsystems[i]->dependency_count == 0) {
                return false;
            }
        }
        return true;
    }

    void shutdown_all() noexcept {
        for (std::size_t i = count; i > 0; i--) {
            auto* h = subsystems[i - 1];
            if (h->initialized && h->shutdown_fn) {
                h->shutdown_fn();
                h->initialized = false;
            }
        }
    }

    bool is_initialized(const char* name) const noexcept {
        for (std::size_t i = 0; i < count; i++) {
            if (subsystems[i]->name && std::strcmp(subsystems[i]->name, name) == 0) {
                return subsystems[i]->initialized;
            }
        }
        return false;
    }
};

} // namespace real
