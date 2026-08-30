// staging_detector.cpp — T1 blue staging sensor: private RX / short-lived stub on processes.
// inspect(pid) for one actor; scan_world() walks StagingWatch hits.

#include "t1_blue/staging_detector.hpp"

namespace t1_blue {

// StagingDetector::StagingDetector: T1 blue staging sensor bound to telemetry sink.
StagingDetector::StagingDetector(ac::ITelemetrySink& sink)
    : sink_(sink), watch_(sink) {}

// StagingDetector::inspect: Score staging signals (private RX / short-lived stub) on one process.
StagingFinding StagingDetector::inspect(const sim::World& w, std::uint32_t pid,
                                        bool private_rx, bool short_lived_stub) {
  StagingFinding f;
  if (!private_rx && !short_lived_stub) {
    f.detail = "no_staging_flags";
    return f;
  }
  f.hit = true;
  const auto* p = w.proc(pid);
  f.detail = std::string("staging signals on ") + (p ? p->name : "?") +
             (private_rx ? " rx" : "") + (short_lived_stub ? " stub" : "");
  f.risk = 1.5;
  sink_.emit({
      .kind = ac::EventKind::Generic,
      .related_tier = ac::Tier::T1_SyscallSoft,
      .subject_pid = pid,
      .detail = f.detail,
      .risk_delta = 1.5,
  });
  return f;
}

// StagingDetector::scan_world: Walk World processes for staging residuals via StagingWatch.
StagingFinding StagingDetector::scan_world(const sim::World& w) {
  auto r = watch_.scan_world(w);
  StagingFinding f;
  f.hit = r.hit;
  f.detail = r.detail;
  for (const auto& h : r.hits) {
    f.risk += h.risk;
    f.detail += " | " + h.name + ":" + h.reason;
  }
  return f;
}

}  // namespace t1_blue
