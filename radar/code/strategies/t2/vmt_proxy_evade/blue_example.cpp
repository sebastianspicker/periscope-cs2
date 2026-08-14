#include "blue_example.hpp"

#include "cs2/diagnostic_sensors.hpp"

bool vmt_proxy_evade_blue_detect(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Blue,
        "Collecting multi-reason Message 160 VMT CRC, module provenance, and use-count telemetry.");
  const auto report = cs2::sensors::VmtIntegritySensor(w).collect();

  int signals = 0;
  if (report.vmt_hooking_detected) {
    ++signals;
    n.say(sim::Side::Blue, "reason: vmt_hooking_detected");
  }
  if (report.interface_tamper_detected) {
    ++signals;
    n.say(sim::Side::Blue, "reason: interface_tamper_detected");
  }
  if (report.anomalous_vmts > 0 || !report.suspicious_vmts.empty()) {
    ++signals;
    n.say(sim::Side::Blue, "reason: anomalous/suspicious VMT inventory");
  }
  for (const auto& [pid, proc] : w.processes) {
    (void)pid;
    for (const auto& m : proc.modules) {
      if (m.name.find("proxy") != std::string::npos ||
          m.name.find("foreign_vmt") != std::string::npos) {
        ++signals;
        n.say(sim::Side::Blue, "reason: proxy/foreign_vmt module residual");
        break;
      }
    }
  }
  if (w.diagnostic_state.vmt_crc32 != 0 &&
      w.diagnostic_state.expected_vmt_crc32 != 0 &&
      w.diagnostic_state.vmt_crc32 != w.diagnostic_state.expected_vmt_crc32) {
    ++signals;
    n.say(sim::Side::Blue, "reason: VMT CRC mismatch");
  }

  const bool detected = signals >= 2;
  n.say(sim::Side::Blue, detected
      ? "A singleton proxy VMT has multi-reason foreign provenance residual."
      : "No multi-reason singleton VMT residual was found.");
  return detected;
}
