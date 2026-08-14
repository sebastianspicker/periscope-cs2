// staging_watch.cpp — implements staging_watch (T1 blue).
// Key methods: observe, scan_world, string.

#include "t1_blue/staging_watch.hpp"

#include <sstream>

namespace t1_blue {

StagingWatch::StagingWatch(ac::ITelemetrySink& sink) : sink_(sink) {}

// StagingWatch::observe: Observe one process for private RX / short-lived stub.
void StagingWatch::observe(const StagingObservation& obs) {
  if (!obs.has_private_rx && !obs.short_lived_stub) {
    return;
  }
  StagingScanHit h;
  h.pid = obs.pid;
  h.name = obs.image_name;
  h.reason = std::string(obs.has_private_rx ? "private_rx" : "") +
             (obs.short_lived_stub ? "|stub" : "");
  last_.hits.push_back(h);
  last_.hit = true;
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::Generic,
      .related_tier = ac::Tier::T1_SyscallSoft,
      .subject_pid = obs.pid,
      .object_pid = 0,
      .detail = obs.image_name + h.reason,
      .risk_delta = 1.5,
  });
}

// StagingWatch::scan_world: Walk World processes for staging residuals via StagingWatch.
StagingScanResult StagingWatch::scan_world(const sim::World& w) {
  last_ = {};
  for (const auto& [pid, p] : w.processes) {
    if (p.is_game || p.is_ac) {
      continue;
    }
    bool stub = p.name.find("stub") != std::string::npos ||
                p.name.find("stage") != std::string::npos ||
                p.name.find("loader") != std::string::npos;
    bool rx = p.manual_mapped_region;
    for (const auto& m : p.modules) {
      if (!m.linked_in_peb || m.headers_erased) {
        rx = true;
      }
    }
    if (stub || rx) {
      StagingScanHit h;
      h.pid = pid;
      h.name = p.name;
      h.reason = (stub ? "short_lived_stub" : "") +
                 std::string(stub && rx ? "|" : "") + (rx ? "private_rx" : "");
      h.risk = stub && rx ? 2.0 : 1.5;
      last_.hits.push_back(h);
      last_.hit = true;
      sink_.emit(ac::TelemetryEvent{
          .kind = ac::EventKind::Generic,
          .related_tier = ac::Tier::T1_SyscallSoft,
          .subject_pid = pid,
          .detail = p.name + "|" + h.reason,
          .risk_delta = h.risk,
      });
    }
  }
  if (w.mapper_process_present) {
    StagingScanHit h;
    h.pid = 0;
    h.name = "mapper";
    h.reason = "mapper_process_present";
    last_.hits.push_back(h);
    last_.hit = true;
  }
  std::ostringstream oss;
  oss << "staging hits=" << last_.hits.size();
  last_.detail = oss.str();
  return last_;
}

}  // namespace t1_blue
