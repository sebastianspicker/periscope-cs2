#include "blue_example.hpp"

#include <cstdio>
#include <string>

bool vac_handle_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue, "sensor 1: enumerate system handles targeting the game");
  int foreign_vm_read = 0;
  int non_steam_owners = 0;
  for (const auto& handle : w.handles_to(w.game_pid(), true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(handle.owner_pid);
    if (owner == nullptr || owner->is_game || owner->is_ac) continue;
    ++foreign_vm_read;
    if (owner->name.find("steam") == std::string::npos &&
        owner->name.find("Steam") == std::string::npos) {
      ++non_steam_owners;
      n.say(sim::Side::Blue,
            "Detected foreign non-Steam VM_READ owner: " + owner->name);
    }
  }

  n.say(sim::Side::Blue, "sensor 2: co-occurrence with remote read activity");
  const bool cooccur = foreign_vm_read > 0 && w.remote_read_ops > 0;

  n.say(sim::Side::Blue, "sensor 3: compose multi-reason VAC handle residual");
  int signals = 0;
  if (foreign_vm_read > 0) ++signals;
  if (non_steam_owners > 0) ++signals;
  if (cooccur) ++signals;
  if (w.remote_read_bytes > 0 && foreign_vm_read > 0) ++signals;

  const bool detected = signals >= 2;
  if (!detected) {
    n.say(sim::Side::Blue, "No multi-reason game handle residual found.");
  } else {
    std::printf("[blue:vac_handle_enum] foreign=%d non_steam=%d cooccur=%d signals=%d\n",
                foreign_vm_read, non_steam_owners, cooccur ? 1 : 0, signals);
  }
  return detected;
}
