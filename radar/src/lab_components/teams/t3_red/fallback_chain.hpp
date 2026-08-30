// fallback_chain.hpp — implements FallbackChain (T3 red).
// Multi-step T3→T2→T1/T0 attach chain; blue must keep all detectors live.

#pragma once

#include "ac/memory_backend.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace t3_red {

/// Outcome of a multi-step fallback attach (T3 → T2 → T1/T0).
struct FallbackAttachReport {
  bool ok = false;
  ac::Tier active_tier = ac::Tier::T0_UsermodeRpm;
  int attempts = 0;
  int failed_steps = 0;
  std::string active_name;
  std::vector<std::string> attempt_log;
  std::string detail;
};

/// Real packs: try T3 → T2 → T1/T0. Blue must keep all detectors live.
class FallbackChain {
 public:
  void add(std::unique_ptr<ac::IMemoryBackend> backend);

  /// Try each registered backend until one attach succeeds.
  ac::Status attach_first_available(std::uint32_t target_id);

  /// Multi-step attach with full attempt log (preferred for lab narratives).
  FallbackAttachReport attach_with_report(std::uint32_t target_id);

  ac::IMemoryBackend* active() const { return active_; }
  ac::Tier active_tier() const;
  int attempt_count() const { return attempts_; }
  int failed_steps() const { return failed_steps_; }
  const FallbackAttachReport& last_report() const { return last_; }

 private:
  std::vector<std::unique_ptr<ac::IMemoryBackend>> backends_;
  ac::IMemoryBackend* active_ = nullptr;
  int attempts_ = 0;
  int failed_steps_ = 0;
  FallbackAttachReport last_{};
};

}  // namespace t3_red
