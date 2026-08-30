#pragma once

#include "cs2/diagnostics.hpp"
#include "sim/world.hpp"

#include <cstdint>

namespace cs2::sensors {

/// Simulates CS2's VEH stack capture analysis.
/// After an exception, the VEH copies up to 2944 bytes of stack data.
/// Blue analyzes the stack frames to determine if cheat code is on the call stack.
class StackAnalysisSensor {
 public:
  explicit StackAnalysisSensor(sim::World& world);

  /// Walk the simulated stack frames from the most recent exception record.
  /// Check whether any captured frame falls within the cheat module range.
  void analyze_stack_for_cheat_frames(std::uint32_t cheat_pid);

  /// Check if the game has performed self-patching.
  /// Red may hide patches behind this legitimate AV mechanism.
  bool is_self_patching_active() const;

 private:
  sim::World& world_;
};

}  // namespace cs2::sensors
