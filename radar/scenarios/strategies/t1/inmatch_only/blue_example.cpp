#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::inmatch_only {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();
  const bool in_match = w.match_active;
  int foreign_vm_read = 0;
  int reader_active_hits = 0;

  std::printf("[blue:inmatch_only] sensor 1: sample during match window\n");
  if (!in_match) {
    r.reasons.emplace_back("match inactive — lobby-only sample may miss attach");
  }

  std::printf("[blue:inmatch_only] sensor 2: foreign VM_READ during match\n");
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      ++foreign_vm_read;
    }
  }
  const bool handle_hit = in_match && foreign_vm_read > 0;
  if (handle_hit) {
    r.reasons.emplace_back("foreign VM_READ handle observed during match");
  }

  std::printf("[blue:inmatch_only] sensor 3: reader_active process flag\n");
  for (const auto& p : w.list_processes(false)) {
    if (p.reader_active && !p.is_game && !p.is_ac) ++reader_active_hits;
  }
  const bool active_hit = in_match && reader_active_hits > 0;
  if (active_hit) {
    r.reasons.emplace_back("reader_active process observed during match");
  }
  if (foreign_vm_read > 0 && !in_match) {
    r.reasons.emplace_back("handle present outside match window (timing residual)");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.24);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected && in_match;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "in_match=" + std::string(in_match ? "1" : "0") +
             " handles=" + std::to_string(foreign_vm_read) +
             " reader_active=" + std::to_string(reader_active_hits);
  std::printf("[blue:inmatch_only] %s detected=%d mitigated=%d\n", r.detail.c_str(),
              r.detected, r.mitigated);
  return r;
}

}  // namespace examples::inmatch_only
