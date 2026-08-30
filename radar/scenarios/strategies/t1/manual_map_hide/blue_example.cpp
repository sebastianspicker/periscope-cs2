// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace examples::manual_map_hide {

BlueResult detect(sim::World& w) {
  BlueResult r;
  const auto game = w.game_pid();
  const auto* g = w.proc(game);
  if (g == nullptr) {
    r.detail = "game process missing";
    return r;
  }

  // Weak PEB walk: only modules still linked_in_peb.
  bool peb_sees_hidden = false;
  int unlinked = 0;
  int erased_headers = 0;
  for (const auto& m : g->modules) {
    if (!m.linked_in_peb) {
      ++unlinked;
      // Would a PEB walk still see it? No — only linked entries.
    } else if (m.name == "lab-map.dll" || m.text_hash == "foreign") {
      peb_sees_hidden = true;
    }
    if (m.headers_erased) {
      ++erased_headers;
    }
  }

  const bool region_hit = g->manual_mapped_region;
  const bool thread_hit = g->has_foreign_thread;
  const bool deep_hit =
      region_hit || thread_hit || unlinked > 0 || erased_headers > 0;
  // PEB-blind: deep scars exist but weak PEB walk never listed the payload.
  const bool peb_blind = !peb_sees_hidden && (unlinked > 0 || region_hit);

  if (unlinked > 0) {
    r.reasons.emplace_back("unlinked module(s) outside PEB lists: " +
                           std::to_string(unlinked));
  }
  if (erased_headers > 0) {
    r.reasons.emplace_back("module(s) with erased PE headers: " +
                           std::to_string(erased_headers));
  }
  if (region_hit) {
    r.reasons.emplace_back("manual_mapped_region scar on game process");
  }
  if (thread_hit) {
    r.reasons.emplace_back("foreign thread origin scar on game process");
  }
  if (peb_blind) {
    r.reasons.emplace_back(
        "weak PEB walk is blind to unlinked / private-mapped payload");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.24);
  // Multi-reason: one residual alone is insufficient (not single-OR deep_hit).
  r.detected = deep_hit && r.signals >= 2;
  r.mitigated = peb_blind && r.detected;
  r.detail = "deep_hit=" + std::to_string(deep_hit ? 1 : 0) +
             " peb_blind=" + std::to_string(peb_blind ? 1 : 0) +
             " unlinked=" + std::to_string(unlinked) +
             " risk=" + std::to_string(r.risk);

  std::printf(
      "[blue:manual_map_hide] detected=%d mitigated=%d unlinked=%d "
      "region=%d thread=%d\n",
      static_cast<int>(r.detected), static_cast<int>(r.mitigated), unlinked,
      static_cast<int>(region_hit), static_cast<int>(thread_hit));
  return r;
}

}  // namespace examples::manual_map_hide
