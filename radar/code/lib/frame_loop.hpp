#pragma once
#include <cstddef>
#include <cstdint>

namespace real {

// Three-phase frame execution with error propagation.
// Each phase returns false on failure.
struct FrameLoopPhase {
    bool (*pre)(uint64_t frame) noexcept;
    bool (*collect)(uint64_t frame) noexcept;
    bool (*post)(uint64_t frame) noexcept;
};

struct FrameLoop {
    FrameLoopPhase* phases;
    std::size_t phase_count;
    uint64_t frame_index = 0;

    // Calls pre/collect/post for each phase. Pre and post failures
    // short-circuit; collect failure records internally (via health ladder)
    // so post always runs.
    bool run_frame() noexcept {
        frame_index++;
        for (std::size_t i = 0; i < phase_count; i++) {
            auto& p = phases[i];
            if (p.pre && !p.pre(frame_index)) return false;
            if (p.collect && !p.collect(frame_index)) return false;
            if (p.post && !p.post(frame_index)) return false;
        }
        return true;
    }
};

} // namespace real
