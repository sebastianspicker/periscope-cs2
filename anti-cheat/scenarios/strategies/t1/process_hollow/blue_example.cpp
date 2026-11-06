#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::process_hollow {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();
  int hollowed_hits = 0;
  int combo_hits = 0;

  std::printf("[blue:process_hollow] sensor 1: hollow scar / original_image\n");
  for (const auto& p : w.list_processes(false)) {
    if (p.hollowed) {
      ++hollowed_hits;
      r.reasons.emplace_back("process image lineage indicates hollowing: " +
                             p.name);
    }
    if (!p.original_image.empty() && p.hollowed) {
      r.reasons.emplace_back("original_image mismatch residual: " +
                             p.original_image);
    }
  }
  if (w.hollowed_pid != 0) {
    r.reasons.emplace_back("world hollowed_pid residual set");
  }

  std::printf("[blue:process_hollow] sensor 2: reputation + map + handle combo\n");
  for (const auto& p : w.list_processes(false)) {
    if (p.is_game || p.is_ac) continue;
    bool foreign_vm_read = false;
    for (const auto& h : w.handles_to(game, true)) {
      if (h.owner_pid == p.pid && sim::has(h.access, sim::AccessMask::VmRead)) {
        foreign_vm_read = true;
        break;
      }
    }
    if (p.looks_reputable && p.manual_mapped_region && foreign_vm_read) {
      ++combo_hits;
      r.reasons.emplace_back(
          "reputable process with manual map holds foreign VM_READ");
    }
  }

  std::printf("[blue:process_hollow] sensor 3: foreign handle graph\n");
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      r.reasons.emplace_back("foreign VM_READ edge from non-AC process");
      break;
    }
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.22);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected && r.signals >= 2;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "hollowed=" + std::to_string(hollowed_hits) +
             " combo=" + std::to_string(combo_hits) +
             " signals=" + std::to_string(r.signals);
  std::printf("[blue:process_hollow] %s detected=%d mitigated=%d\n",
              r.detail.c_str(), r.detected, r.mitigated);
  return r;
}

}  // namespace examples::process_hollow
