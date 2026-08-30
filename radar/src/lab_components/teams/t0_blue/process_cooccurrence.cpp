// process_cooccurrence.cpp — blue sensor: suspicious process co-occurrence with game pid.
// Complements handle-graph when names/parents look like radar tooling.

#include "t0_blue/process_cooccurrence.hpp"


#include <algorithm>
#include <cctype>
#include <sstream>

namespace t0_blue {

// ProcessCooccurrence::looks_suspicious_name: Heuristic name match for radar-like tools.
bool ProcessCooccurrence::looks_suspicious_name(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lower.find("radar") != std::string::npos ||
         lower.find("esp") != std::string::npos ||
         lower.find("cheat") != std::string::npos ||
         lower.find("hack") != std::string::npos ||
         lower.find("soft-aim") != std::string::npos ||
         lower.find("lab-radar") != std::string::npos;
}

// ProcessCooccurrence::ProcessCooccurrence: Blue sensor for suspicious process co-residence.
ProcessCooccurrence::ProcessCooccurrence(ac::ITelemetrySink& sink)
    : sink_(sink) {}

// ProcessCooccurrence::on_game_session: Observe process set for a game session and score co-occurrence.
void ProcessCooccurrence::on_game_session(std::uint32_t game_pid,
                                          const std::vector<ProcessRecord>& live) {
  last_ = {};
  for (const auto& p : live) {
    if (p.pid == game_pid) {
      continue;
    }
    if (!looks_suspicious_name(p.name)) {
      continue;
    }
    CooccurrenceHit h{p.pid, p.name, "name_heuristic", 1.0};
    last_.hits.push_back(h);
    last_.hit = true;
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::ProcessCoRun,
        .related_tier = ac::Tier::T0_UsermodeRpm,
        .subject_pid = p.pid,
        .object_pid = game_pid,
        .detail = p.name,
        .risk_delta = 1.0,
    });
  }
  last_.detail = "session_hits=" + std::to_string(last_.hits.size());
}

// ProcessCooccurrence::scan_world: Walk World processes for staging residuals via StagingWatch.
CooccurrenceResult ProcessCooccurrence::scan_world(const sim::World& w,
                                                   std::uint32_t game_pid,
                                                   bool weak_enum) {
  last_ = {};
  for (const auto& p : w.list_processes(weak_enum)) {
    if (p.pid == game_pid || p.is_game || p.is_ac) {
      continue;
    }
    // Name heuristic
    if (looks_suspicious_name(p.name)) {
      CooccurrenceHit h{p.pid, p.name, "name_heuristic", 1.5};
      last_.hits.push_back(h);
      last_.hit = true;
      sink_.emit(ac::TelemetryEvent{
          .kind = ac::EventKind::ProcessCoRun,
          .related_tier = ac::Tier::T0_UsermodeRpm,
          .subject_pid = p.pid,
          .object_pid = game_pid,
          .detail = p.name,
          .risk_delta = 1.5,
      });
    }
    // Reader co-run: process with VM_READ on game even if name is clean.
    bool has_vm = false;
    for (const auto& h : w.handles_to(game_pid, true)) {
      if (h.owner_pid == p.pid && sim::has(h.access, sim::AccessMask::VmRead)) {
        has_vm = true;
        break;
      }
    }
    if (has_vm && !looks_suspicious_name(p.name)) {
      CooccurrenceHit h{p.pid, p.name, "unnamed_reader_co_run", 2.0};
      last_.hits.push_back(h);
      last_.hit = true;
      sink_.emit(ac::TelemetryEvent{
          .kind = ac::EventKind::ProcessCoRun,
          .related_tier = ac::Tier::T0_UsermodeRpm,
          .subject_pid = p.pid,
          .object_pid = game_pid,
          .detail = p.name + "#vm_read",
          .risk_delta = 2.0,
      });
    }
  }
  std::ostringstream oss;
  oss << "co_run hits=" << last_.hits.size() << " weak_enum=" << (weak_enum ? 1 : 0);
  last_.detail = oss.str();
  return last_;
}

}  // namespace t0_blue
