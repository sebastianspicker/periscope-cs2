// telemetry.cpp — ITelemetrySink + MemoryTelemetrySink for lab event capture.
// Tests assert emitted kinds/risk and EventKind labels.

#include "ac/telemetry.hpp"

namespace ac {

// to_string: stable non-empty label for every EventKind enumerator.
std::string_view to_string(EventKind kind) {
  switch (kind) {
    case EventKind::HandleToGame:
      return "handle_to_game";
    case EventKind::ProcessCoRun:
      return "process_co_run";
    case EventKind::DriverLoad:
      return "driver_load";
    case EventKind::ByovdBlocked:
      return "byovd_blocked";
    case EventKind::DeviceOpen:
      return "device_open";
    case EventKind::TrustPolicyFail:
      return "trust_policy_fail";
    case EventKind::HvProbeAnomaly:
      return "hv_probe_anomaly";
    case EventKind::BridgeSuspected:
      return "bridge_suspected";
    case EventKind::InfoAdvantageHit:
      return "info_advantage_hit";
    case EventKind::Generic:
      return "generic";
  }
  return "unknown_event";
}

// MemoryTelemetrySink::emit: append event in emission order (tests rely on order).
void MemoryTelemetrySink::emit(const TelemetryEvent& ev) {
  events_.push_back(ev);
}

}  // namespace ac
