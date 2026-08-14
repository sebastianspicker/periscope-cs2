#pragma once

// Educational model of aggressive T2: strip / shadow notify callbacks on World.

#include "sim/world.hpp"

#include <cstddef>
#include <string>

namespace t2_red {

// Lab type `CallbackState` used by this educational unit.
struct CallbackState {
  std::size_t process_notify = 4;
  std::size_t image_notify = 3;
  bool ac_callback_present = true;
};

// Aggregate outcome fields for `CallbackStripReport` (lab narrative / tests).
struct CallbackStripReport {
  CallbackState before{};
  CallbackState after{};
  bool stripped = false;
  bool shadow = false;
  std::string detail;
};

// Lab type `CallbackStripSim` used by this educational unit.
class CallbackStripSim {
 public:
  static CallbackState strip_aggressive(CallbackState in);

  /// Capture current World notify model into CallbackState.
  static CallbackState capture(const sim::World& w);

  /// Apply aggressive strip to World (process/image notify + AC flag).
  static CallbackStripReport strip_world(sim::World& w);

  /// Shadow mode: hide during sample, restore truth later.
  static CallbackStripReport enable_shadow(sim::World& w, bool on);
};

}  // namespace t2_red
