#pragma once

// Red simulation for cpuid_hypercall_channel. It plants the CPUID-magic-key
// hypercall covert-channel scar: red probes a magic CPUID leaf, then issues
// hypercall reads through the platform hypervisor without any game handle.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::cpuid_hypercall_channel {
struct RedResult { bool achieved; int steps; std::string magic; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::cpuid_hypercall_channel
