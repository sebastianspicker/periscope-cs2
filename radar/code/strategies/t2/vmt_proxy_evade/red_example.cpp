#include "red_example.hpp"

#include <cstdint>
#include <string>

bool vmt_proxy_evade_red_apply(sim::World& w, sim::Narrator& n) {
  auto* game = w.proc(w.game_pid());
  if (game == nullptr || game->modules.empty()) return false;

  auto& vmts = w.diagnostic_state.vmts;
  vmts.clear();
  const auto module_base = game->modules.front().base;
  const auto& module_name = game->modules.front().name;

  // The shared address gives the simulated production VMT a non-singleton use count.
  constexpr std::uint64_t kRealDispatchVmtOffset = 0x1800;
  for (int index = 0; index != 112; ++index) {
    vmts.push_back({module_base + kRealDispatchVmtOffset,
                    "interface_" + std::to_string(index), module_name, false, true});
  }
  for (int entity = 0; entity != 2; ++entity) {
    vmts.push_back({module_base + kRealDispatchVmtOffset,
                    "entity_" + std::to_string(entity), module_name, true, true});
  }

  // Only the simulated red caller sees this proxy; no original dispatch entry is changed.
  vmts.push_back({module_base + 0x900000, "red_proxy_dispatch", "red_proxy_stub", true, false});
  n.say(sim::Side::Red,
        "Routing simulated red calls through a proxy while leaving original VMT entries intact.");
  return true;
}
