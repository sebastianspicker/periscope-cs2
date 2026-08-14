// telemetry.hpp — ITelemetrySink + MemoryTelemetrySink for lab event capture.
// Tests assert emitted kinds/risk.

#pragma once

#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ac {

// EventKind: lab telemetry categories consumed by RiskAggregator / blue agents.
enum class EventKind : std::uint8_t {
  HandleToGame,
  ProcessCoRun,
  DriverLoad,
  ByovdBlocked,
  DeviceOpen,
  TrustPolicyFail,
  HvProbeAnomaly,
  BridgeSuspected,
  InfoAdvantageHit,
  Generic,
};

// Human-readable label for EventKind (logging / proto demos / tests).
std::string_view to_string(EventKind kind);

// TelemetryEvent: lab type for this educational unit.
struct TelemetryEvent {
  EventKind kind = EventKind::Generic;
  Tier related_tier = Tier::T0_UsermodeRpm;
  std::uint32_t subject_pid = 0;
  std::uint32_t object_pid = 0;
  std::string detail;
  double risk_delta = 0.0;
};

// ITelemetrySink: lab type for this educational unit.
class ITelemetrySink {
 public:
  virtual ~ITelemetrySink() = default;
  virtual void emit(const TelemetryEvent& ev) = 0;
};

/// In-memory sink for tests and local agents.
class MemoryTelemetrySink final : public ITelemetrySink {
 public:
  void emit(const TelemetryEvent& ev) override;
  const std::vector<TelemetryEvent>& events() const { return events_; }
  void clear() { events_.clear(); }

 private:
  std::vector<TelemetryEvent> events_;
};

}  // namespace ac
