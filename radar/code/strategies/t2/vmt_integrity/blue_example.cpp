#include "blue_example.hpp"

#include "cs2/diagnostic_sensors.hpp"

bool vmt_integrity_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue, "Collecting multi-reason VMT integrity telemetry.");
  const auto report = cs2::sensors::VmtIntegritySensor(w).collect();

  int signals = 0;
  if (report.interface_tamper_detected) {
    ++signals;
    n.say(sim::Side::Blue, "reason: interface VMT tamper detected");
  }
  if (report.vmt_hooking_detected) {
    ++signals;
    n.say(sim::Side::Blue, "reason: VMT hooking detected");
  }
  if (report.anomalous_vmts > 0) {
    ++signals;
    n.say(sim::Side::Blue, "reason: anomalous VMT entries present");
  }
  if (!report.suspicious_vmts.empty()) {
    ++signals;
    n.say(sim::Side::Blue, "reason: suspicious VMT inventory non-empty");
  }
  // World-level corroboration: foreign module holding unexpected VMT target.
  for (const auto& [pid, proc] : w.processes) {
    (void)pid;
    for (const auto& m : proc.modules) {
      if (m.name.find("foreign_vmt") != std::string::npos ||
          m.name == "foreign_vmt_stub") {
        ++signals;
        n.say(sim::Side::Blue, "reason: foreign_vmt module residual");
        break;
      }
    }
  }
  if (w.diagnostic_state.vmt_crc32 != 0 &&
      w.diagnostic_state.expected_vmt_crc32 != 0 &&
      w.diagnostic_state.vmt_crc32 != w.diagnostic_state.expected_vmt_crc32) {
    ++signals;
    n.say(sim::Side::Blue, "reason: diagnostic VMT CRC mismatch");
  }

  const bool detected = signals >= 2;
  n.say(sim::Side::Blue, detected ? "VMT multi-reason residual confirmed."
                                  : "No multi-reason VMT residual found.");
  return detected;
}
