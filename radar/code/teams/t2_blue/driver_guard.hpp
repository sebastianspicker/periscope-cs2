// driver_guard.hpp — implements DriverLoadEvent, DriverScanResult, DriverGuard (T2 blue).
// Simulation unit; scars live on sim::World.

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace t2_blue {

// DriverLoadEvent: lab type for this educational unit.
struct DriverLoadEvent {
  std::string path;
  std::string sha256_hex;
  std::string signer;
  bool boot_start = false;
};

// DriverScanResult: lab type for this educational unit.
struct DriverScanResult {
  bool hit = false;
  int unknown_memrw = 0;
  int early_load = 0;
  int services = 0;
  std::vector<std::string> names;
  std::string detail;
};

// DriverGuard: lab type for this educational unit.
class DriverGuard {
 public:
  explicit DriverGuard(ac::ITelemetrySink& sink);

  void on_image_load(const DriverLoadEvent& ev);
  void set_allowlist(std::vector<std::string> sha256_hex);

  /// Scan World drivers/services for unknown mem-R/W / early load / SCM.
  DriverScanResult scan_world(const sim::World& w);

 private:
  ac::ITelemetrySink& sink_;
  std::vector<std::string> allowlist_;
};

}  // namespace t2_blue
