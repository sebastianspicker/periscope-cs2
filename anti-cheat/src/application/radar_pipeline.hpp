#pragma once

// Simulation application service. Platform/adapter orchestration is composed
// outside src/ by opt-in real applications.
#include "ac_sim/accept_gate.hpp"
#include "ac_sim/api_integrity.hpp"
#include "ac_sim/behavioral_filter.hpp"
#include "ac_sim/health_ladder.hpp"

#include <cstdint>

namespace radar {

class FramePipeline {
 public:
  FramePipeline() noexcept = default;
  bool initialize(std::uint32_t cs2_pid, bool use_real_cs2) noexcept;
  bool run_frame() noexcept;
  void shutdown() noexcept;

  const sim::HealthLadder& health() const noexcept { return health_; }
  const sim::AcceptGate& gate() const noexcept { return gate_; }
  const sim::BehavioralFilter& behavioral() const noexcept { return behavioral_; }
  const sim::ApiIntegrityChecker& api_integrity() const noexcept { return api_integrity_; }
  const sim::GateMetrics& last_gate_metrics() const noexcept { return gate_metrics_; }
  std::uint64_t frame_count() const noexcept { return frame_count_; }
  bool last_api_integrity_ok() const noexcept { return api_integrity_ok_; }
  bool api_integrity_registry_ready() const noexcept { return api_integrity_ready_; }
  int last_entity_count() const noexcept { return last_entity_count_; }
  int last_filtered_entity_count() const noexcept { return last_filtered_entity_count_; }

 private:
  sim::HealthLadder health_;
  sim::AcceptGate gate_;
  sim::BehavioralFilter behavioral_;
  sim::ApiIntegrityChecker api_integrity_;
  sim::GateMetrics gate_metrics_{};
  std::uint64_t frame_count_ = 0;
  bool initialized_ = false;
  bool api_integrity_ok_ = false;
  bool api_integrity_ready_ = false;
  int last_entity_count_ = 0;
  int last_filtered_entity_count_ = 0;
};

}  // namespace radar
