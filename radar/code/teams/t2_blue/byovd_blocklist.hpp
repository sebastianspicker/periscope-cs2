// byovd_blocklist.hpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace t2_blue {

// ByovdHit: lab type for this educational unit.
struct ByovdHit {
  std::string image;
  std::string sha256;
  double risk = 8.0;
};

// ByovdScanResult: lab type for this educational unit.
struct ByovdScanResult {
  bool hit = false;
  std::vector<ByovdHit> hits;
  std::string detail;
};

// ByovdBlocklist: lab type for this educational unit.
class ByovdBlocklist {
 public:
  explicit ByovdBlocklist(ac::ITelemetrySink& sink);

  void add(std::string sha256_hex);
  bool is_blocked(const std::string& sha256_hex) const;
  /// Returns true if load should be denied / session flagged.
  bool check_and_emit(const std::string& sha256_hex, const std::string& image);

  /// Scan World drivers for known-bad / blocklisted hashes.
  ByovdScanResult scan_world(const sim::World& w);

  /// Enforce lab mitigate: byovd_policy_block + strip mem_rw on known-bad.
  bool mitigate_world(sim::World& w);

 private:
  ac::ITelemetrySink& sink_;
  std::unordered_set<std::string> blocked_;
};

}  // namespace t2_blue
