// evasion_weak.cpp — weak T0 evasion kit that does NOT defeat handle-graph sensors.
// apply_common_stack leaves VmRead handles visible for blue lessons.

#include "t0_red/evasion_weak.hpp"

namespace t0_red {

// WeakEvasionKit::polymorphic_rename: Weak evasion: rename binary id (handle graph still visible).
WeakEvasionReport WeakEvasionKit::polymorphic_rename(sim::World& w,
                                                     std::uint32_t pid,
                                                     const std::string& new_name) {
  WeakEvasionReport r;
  r.attempt = "polymorphic rename / rebuild";
  r.blue_still_sees = "handle table still lists owner pid with VM_READ";
  r.defeats_handle_graph = false;
  if (auto* p = w.proc(pid)) {
    p->name = new_name;
    w.binary_build_id = "buyer_" + new_name;
    r.applied = true;
    r.detail = "renamed_to=" + new_name;
    w.note("weak_evasion rename " + r.detail);
  } else {
    r.detail = "no_process";
  }
  return r;
}

// WeakEvasionKit::hide_from_weak_enum: Weak evasion: hide from naive name enum only.
WeakEvasionReport WeakEvasionKit::hide_from_weak_enum(sim::World& w,
                                                      std::uint32_t pid) {
  WeakEvasionReport r;
  r.attempt = "hide from weak process snapshot";
  r.blue_still_sees =
      "handle enumeration is independent of process name lists";
  r.defeats_handle_graph = false;
  if (auto* p = w.proc(pid)) {
    p->hidden_from_weak_enum = true;
    r.applied = true;
    r.detail = "hidden_from_weak_enum=1";
    w.note("weak_evasion " + r.detail);
  }
  return r;
}

// WeakEvasionKit::throttle_reads: Weak evasion: slow RPM rate (does not remove handles).
WeakEvasionReport WeakEvasionKit::throttle_reads(sim::World& w, std::uint32_t pid,
                                                 int hz) {
  WeakEvasionReport r;
  r.attempt = "read at " + std::to_string(hz) + "Hz sparse fields only";
  r.blue_still_sees =
      "handle still open; volume heuristics may miss, graph does not";
  r.defeats_handle_graph = false;
  r.applied = true;
  r.detail = "read_throttle hz=" + std::to_string(hz) + " pid=" +
             std::to_string(pid);
  w.note(r.detail);
  return r;
}

// WeakEvasionKit::hide_handle_during_enum: Mark handle hidden_from_enum only (weak).
WeakEvasionReport WeakEvasionKit::hide_handle_during_enum(
    sim::World& w, std::uint32_t owner_pid, std::uint32_t game_pid) {
  WeakEvasionReport r;
  r.attempt = "hide handle during AC enum sample";
  r.blue_still_sees =
      "continuous / multi-sample handle graph still sees hidden edges";
  r.defeats_handle_graph = false;
  int n = 0;
  for (auto& h : w.handles) {
    if (h.owner_pid == owner_pid && h.target_pid == game_pid &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      h.hidden_during_enum = true;
      h.brief_reopen = true;
      ++n;
    }
  }
  r.applied = n > 0;
  r.detail = "hidden_handles=" + std::to_string(n);
  w.note("weak_evasion hide_handle " + r.detail);
  return r;
}

// WeakEvasionKit::claim_reputation: Weak signed-reputation claim; does not clear handles.
WeakEvasionReport WeakEvasionKit::claim_reputation(sim::World& w,
                                                   std::uint32_t pid) {
  WeakEvasionReport r;
  r.attempt = "claim looks_reputable / allowlist bait name";
  r.blue_still_sees =
      "strong FP policy treats name allowlist alone as insufficient";
  r.defeats_handle_graph = false;
  if (auto* p = w.proc(pid)) {
    p->looks_reputable = true;
    r.applied = true;
    r.detail = "looks_reputable=1 name=" + p->name;
    w.note("weak_evasion " + r.detail);
  }
  return r;
}

// WeakEvasionKit::apply_common_stack: Apply the full weak evasion stack; never defeats handle graph.
std::vector<WeakEvasionReport> WeakEvasionKit::apply_common_stack(
    sim::World& w, std::uint32_t pid, std::uint32_t game_pid) {
  std::vector<WeakEvasionReport> out;
  out.push_back(polymorphic_rename(w, pid, "nvidia-overlay"));
  out.push_back(hide_from_weak_enum(w, pid));
  out.push_back(throttle_reads(w, pid, 20));
  out.push_back(hide_handle_during_enum(w, pid, game_pid));
  out.push_back(claim_reputation(w, pid));
  return out;
}

}  // namespace t0_red
