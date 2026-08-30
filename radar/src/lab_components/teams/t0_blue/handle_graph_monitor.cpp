// handle_graph_monitor.cpp — blue sensor: foreign VmRead handles into the game process.
// Core T0 residual that weak evasion does not erase.

#include "t0_blue/handle_graph_monitor.hpp"


#include <algorithm>
#include <sstream>

namespace t0_blue {
namespace {

// has_flag: free function for this educational unit.
bool has_flag(ProcessAccess a, ProcessAccess f) {
  return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(f)) != 0;
}

// to_access: free function for this educational unit.
ProcessAccess to_access(sim::AccessMask m) {
  ProcessAccess a = ProcessAccess::None;
  if (sim::has(m, sim::AccessMask::Query)) {
    a = static_cast<ProcessAccess>(static_cast<std::uint32_t>(a) |
                                   static_cast<std::uint32_t>(ProcessAccess::QueryLimited));
  }
  if (sim::has(m, sim::AccessMask::VmRead)) {
    a = static_cast<ProcessAccess>(static_cast<std::uint32_t>(a) |
                                   static_cast<std::uint32_t>(ProcessAccess::VmRead));
  }
  return a;
}

}  // namespace

HandleGraphMonitor::HandleGraphMonitor(ac::ITelemetrySink& sink) : sink_(sink) {}

// HandleGraphMonitor::ingest_edges: Ingest handle-graph edges (owner→game VmRead) for this tick.
void HandleGraphMonitor::ingest_edges(std::uint32_t game_pid,
                                      const std::vector<HandleEdge>& edges) {
  suspicious_.clear();
  last_ = {};
  for (const auto& e : edges) {
    if (e.target_pid != game_pid) {
      continue;
    }
    if (!has_flag(e.access, ProcessAccess::VmRead)) {
      continue;
    }
    if (e.source_pid == game_pid) {
      continue;
    }
    suspicious_.push_back(e);
    ++last_.truth_foreign_vm_read;
    if (!e.hidden_during_enum) {
      ++last_.visible_foreign_vm_read;
    } else {
      ++last_.hidden_edges;
    }
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::HandleToGame,
        .related_tier = ac::Tier::T0_UsermodeRpm,
        .subject_pid = e.source_pid,
        .object_pid = e.target_pid,
        .detail = e.source_name,
        .risk_delta = 3.0,
    });
  }
  last_.suspicious = suspicious_;
  last_.race_suspected =
      last_.truth_foreign_vm_read > last_.visible_foreign_vm_read;
  std::ostringstream oss;
  oss << "ingest vis=" << last_.visible_foreign_vm_read
      << " truth=" << last_.truth_foreign_vm_read
      << " hidden=" << last_.hidden_edges;
  last_.detail = oss.str();
}

// HandleGraphMonitor::scan_world: Walk World processes for staging residuals via StagingWatch.
HandleSampleResult HandleGraphMonitor::scan_world(const sim::World& w,
                                                  std::uint32_t game_pid,
                                                  bool include_hidden) {
  suspicious_.clear();
  last_ = {};
  if (game_pid == 0) {
    last_.detail = "no_game";
    return last_;
  }

  // Visible sample
  for (const auto& h : w.handles_to(game_pid, /*include_hidden=*/false)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = w.proc(h.owner_pid);
    if (!p || p->is_game || p->is_ac) {
      continue;
    }
    ++last_.visible_foreign_vm_read;
  }

  // Truth sample
  for (const auto& h : w.handles_to(game_pid, /*include_hidden=*/true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = w.proc(h.owner_pid);
    if (!p || p->is_game || p->is_ac) {
      continue;
    }
    ++last_.truth_foreign_vm_read;
    if (h.hidden_during_enum) {
      ++last_.hidden_edges;
    }
    if (h.brief_reopen) {
      ++last_.brief_reopen;
    }
    if (include_hidden || !h.hidden_during_enum) {
      HandleEdge e;
      e.source_pid = h.owner_pid;
      e.target_pid = h.target_pid;
      e.access = to_access(h.access);
      e.source_name = p->name;
      e.via_syscall = h.via_syscall_path;
      e.hidden_during_enum = h.hidden_during_enum;
      e.brief_reopen = h.brief_reopen;
      suspicious_.push_back(e);
      sink_.emit(ac::TelemetryEvent{
          .kind = ac::EventKind::HandleToGame,
          .related_tier = ac::Tier::T0_UsermodeRpm,
          .subject_pid = h.owner_pid,
          .object_pid = game_pid,
          .detail = p->name + (h.hidden_during_enum ? "#hidden" : ""),
          .risk_delta = h.hidden_during_enum ? 3.5 : 3.0,
      });
    }
  }

  last_.suspicious = suspicious_;
  last_.race_suspected =
      last_.truth_foreign_vm_read > last_.visible_foreign_vm_read ||
      last_.hidden_edges > 0;
  std::ostringstream oss;
  oss << "scan_world vis=" << last_.visible_foreign_vm_read
      << " truth=" << last_.truth_foreign_vm_read
      << " hidden=" << last_.hidden_edges
      << " brief=" << last_.brief_reopen
      << " race=" << (last_.race_suspected ? 1 : 0)
      << " susp=" << suspicious_.size();
  last_.detail = oss.str();
  return last_;
}

// HandleGraphMonitor::multi_sample: Multi-sample handle edges across ticks for confidence.
HandleSampleResult HandleGraphMonitor::multi_sample(const sim::World& w,
                                                    std::uint32_t game_pid) {
  // First: naive single sample (visible only) — may miss hide-on-enum.
  auto naive = scan_world(w, game_pid, /*include_hidden=*/false);
  const int naive_vis = naive.visible_foreign_vm_read;
  // Second: continuous truth.
  auto full = scan_world(w, game_pid, /*include_hidden=*/true);
  full.race_suspected = full.truth_foreign_vm_read > naive_vis ||
                        full.hidden_edges > 0;
  full.detail += " multi naive_vis=" + std::to_string(naive_vis);
  last_ = full;
  return last_;
}

// ── Real handle scanning ────────────────────────────────────

HandleSampleResult HandleGraphMonitor::scan_real_handles(uint32_t cs2_pid) {
  HandleSampleResult result;
  (void)cs2_pid;
  result.detail = "host adapter unavailable in lab component";
  last_ = result;
  return result;
}

bool HandleGraphMonitor::is_pid_legitimate(uint32_t owner_pid) const {
  // Known system PIDs that legitimately hold process handles
  if (owner_pid == 0 || owner_pid == 4) return true;  // System, Idle
  return false;
}

}  // namespace t0_blue
