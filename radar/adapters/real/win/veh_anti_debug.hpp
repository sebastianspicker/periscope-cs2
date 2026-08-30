#pragma once

namespace real::win {

void register_veh() noexcept;
void unregister_veh() noexcept;

/// Periodic VEH cycling to evade CS2 diagnostic module 12 probing.
/// Call from frame loop every ~200-400 frames.
void veh_cycle() noexcept;

/// True if a VEH handler from this module is currently registered.
bool veh_is_registered() noexcept;

}  // namespace real::win
