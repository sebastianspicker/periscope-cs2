#include "blue_example.hpp"

#include "cs2/diagnostic_orchestrator.hpp"

bool diagnostic_telemetry_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue, "Collecting the simulated Message 159 diagnostic telemetry payload.");
  const auto snapshot = cs2::DiagnosticOrchestrator(w).run_full_diagnostics();

  int signals = 0;
  if (snapshot.module_snapshot.has_suspicious_modules) {
    ++signals;
    n.say(sim::Side::Blue, "reason: suspicious modules in snapshot");
  }
  if (snapshot.pe_timestamps.any_mismatch) {
    ++signals;
    n.say(sim::Side::Blue, "reason: PE timestamp mismatch");
  }
  if (snapshot.thread_capture.from_suspicious_module) {
    ++signals;
    n.say(sim::Side::Blue, "reason: thread from suspicious module");
  }
  if (snapshot.thread_capture.from_rwx_memory) {
    ++signals;
    n.say(sim::Side::Blue, "reason: thread from RWX memory");
  }
  if (w.diagnostic_response_count > 0) {
    ++signals;
    n.say(sim::Side::Blue, "reason: diagnostic response telemetry present");
  }

  const bool detected = signals >= 2;
  n.say(sim::Side::Blue, "Recorded module list, PE timestamps, thread capture, and diagnostic response state.");
  n.say(sim::Side::Blue, detected ? "Telemetry contains correlated multi-reason anomalies."
                                  : "Telemetry lacks multi-reason threshold.");
  return detected;
}
