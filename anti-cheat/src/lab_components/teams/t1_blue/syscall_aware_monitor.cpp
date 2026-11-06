// syscall_aware_monitor.cpp — T1 blue hook/syscall-aware monitor on sim World scars.
// Multi-reason detect for soft-edge delivery.

#include "t1_blue/syscall_aware_monitor.hpp"

#include <sstream>

namespace t1_blue {

SyscallAwareHandleMonitor::SyscallAwareHandleMonitor(
    t0_blue::HandleGraphMonitor& handles)
    : handles_(handles) {}

// SyscallAwareHandleMonitor::evaluate: Syscall-aware monitor: correlate handles with syscall/soft signals.
void SyscallAwareHandleMonitor::evaluate(
    std::uint32_t game_pid,
    const std::vector<t0_blue::HandleEdge>& edges,
    bool /*usermode_hooks_saw_rpm*/) {
  // Critical T1 lesson: even if hooks saw nothing, handles still speak.
  handles_.ingest_edges(game_pid, edges);
  last_hits_ = handles_.suspicious().size();
}

// T1Agent::T1Agent: T1 blue agent composing hook trap + staging + handles.
T1Agent::T1Agent(sim::World& world, ac::ITelemetrySink& sink)
    : world_(world),
      sink_(sink),
      handles_(sink),
      mon_(handles_),
      staging_(sink) {}

// T1Agent::full_scan: Blue: run handle + co-occurrence (and related) sensors once.
T1Detection T1Agent::full_scan() {
  T1Detection d;
  const auto game = world_.game_pid();

  // 1) Naive hooks vs truth
  auto hr = hooks_.analyze(world_, game);
  auto tr = truth_.analyze(world_, game, true);
  d.hook_visible = hr.visible_winapi_opens;
  d.foreign_vm_read = tr.foreign_vm_read;
  d.syscall_handles = tr.syscall_path;
  d.hooks_blind = hr.blind_to_syscall_red ||
                  (tr.syscall_path > 0 && hr.visible_winapi_opens == 0);
  d.handle_truth_hit = tr.foreign_vm_read > 0;

  if (d.handle_truth_hit) {
    d.reasons.push_back(tr.detail);
    risk_.ingest({ac::EventKind::HandleToGame, ac::Tier::T1_SyscallSoft, 0, game,
                  tr.detail, 3.0});
  }
  if (d.hooks_blind) {
    d.reasons.push_back("hooks_blind " + hr.detail);
  }

  // Feed handle graph multi-sample for depth.
  auto hs = handles_.multi_sample(world_, game);
  if (hs.truth_foreign_vm_read > 0) {
    mon_.evaluate(game, hs.suspicious, !d.hooks_blind);
  }

  // 2) Staging
  auto st = staging_.scan_world(world_);
  d.staging_hit = st.hit;
  if (st.hit) {
    d.reasons.push_back("staging " + st.detail);
    risk_.ingest({ac::EventKind::Generic, ac::Tier::T1_SyscallSoft, 0, game,
                  st.detail, st.risk > 0 ? st.risk : 1.5});
  }

  // 3) ETW blind / stack spoof
  if (!world_.etw_enabled) {
    d.etw_blind_hit = true;
    d.reasons.push_back("etw_disabled");
    risk_.ingest({ac::EventKind::Generic, ac::Tier::T1_SyscallSoft, 0, game,
                  "etw_blind", 1.0});
  }
  if (world_.stack_spoof_on_read) {
    d.stack_spoof_hit = true;
    d.reasons.push_back("stack_spoof_on_read");
    risk_.ingest({ac::EventKind::Generic, ac::Tier::T1_SyscallSoft, 0, game,
                  "stack_spoof", 1.0});
  }

  // 4) Hollow / lineage
  for (const auto& [pid, p] : world_.processes) {
    if (p.is_game || p.is_ac) {
      continue;
    }
    if (p.hollowed) {
      d.hollow_hit = true;
      d.reasons.push_back("hollowed " + p.name);
    }
    if (p.parent_pid != 0) {
      const auto* parent = world_.proc(p.parent_pid);
      if (parent &&
          (parent->name.find("stub") != std::string::npos ||
           parent->name.find("stage") != std::string::npos ||
           parent->name.find("loader") != std::string::npos)) {
        // Only if child has VM_READ or is reader
        for (const auto& h : world_.handles_to(game, true)) {
          if (h.owner_pid == pid &&
              sim::has(h.access, sim::AccessMask::VmRead)) {
            d.parent_lineage_hit = true;
            d.reasons.push_back("lineage " + p.name + "<-" + parent->name);
            break;
          }
        }
      }
    }
  }

  // 5) C2 / offset net
  for (const auto& f : world_.net) {
    if (f.looks_like_offset_c2) {
      d.reasons.push_back("offset_c2 " + f.dest);
      risk_.ingest({ac::EventKind::Generic, ac::Tier::T1_SyscallSoft, f.pid, game,
                    f.dest, 1.0});
    }
  }

  d.risk = risk_.state().score;
  std::ostringstream oss;
  oss << "handle_truth=" << d.handle_truth_hit << " hooks_blind=" << d.hooks_blind
      << " syscall_h=" << d.syscall_handles << " staging=" << d.staging_hit
      << " etw=" << d.etw_blind_hit << " spoof=" << d.stack_spoof_hit
      << " risk=" << d.risk << " reasons=" << d.reasons.size();
  d.summary = oss.str();
  return d;
}

}  // namespace t1_blue
