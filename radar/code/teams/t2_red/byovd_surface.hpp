#pragma once

// BYOVD load surface: known-bad signed driver + mem-R/W device on sim::World.
// Simulated SCM and IOCTL mapping.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <string>

namespace t2_red {

// Lab type `VulnerableDriverRef` used by this educational unit.
struct VulnerableDriverRef {
  std::string image_name;
  std::string sha256_hex;  // catalog key for blue blocklist
  std::string signer = "OldVendor";
  std::string device_name = "\\\\.\\AcLabMemRw";
  bool signed_driver = true;
};

// Aggregate outcome fields for `ByovdLoadReport` (lab narrative / tests).
struct ByovdLoadReport {
  bool loaded = false;
  bool device_created = false;
  std::string detail;
};

// Lab type `ByovdSurface` used by this educational unit.
class ByovdSurface {
 public:
  void set_candidate(VulnerableDriverRef ref);
  /// In-memory mark only (unit tests).
  ac::Status attempt_load_lab();

  /// Multi-step: load_driver + create_device on World (primary educational path).
  ByovdLoadReport load_on_world(sim::World& w);

  bool loaded() const { return loaded_; }
  const VulnerableDriverRef& candidate() const { return candidate_; }
  const ByovdLoadReport& last() const { return last_; }

 private:
  VulnerableDriverRef candidate_{};
  bool loaded_ = false;
  ByovdLoadReport last_{};
};

}  // namespace t2_red
