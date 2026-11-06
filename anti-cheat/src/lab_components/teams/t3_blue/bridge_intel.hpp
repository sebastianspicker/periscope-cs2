// bridge_intel.hpp — T3 blue platform/HV sensor using World.trust posture flags.
// Simulated personal HV / attestation fields

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t2_blue/device_watch.hpp"
#include "t2_blue/driver_guard.hpp"

#include <string>
#include <vector>

namespace t3_blue {

// BridgeScanResult: lab type for this educational unit.
struct BridgeScanResult {
  bool hit = false;
  std::vector<std::string> drivers;
  std::vector<std::string> devices;
  double risk = 0;
  std::string detail;
};

// BridgeIntel: lab type for this educational unit.
class BridgeIntel {
 public:
  /// Own DriverGuard/DeviceWatch (primary).
  explicit BridgeIntel(ac::ITelemetrySink& sink);
  /// Optional external refs (seed only; still scans World).
  BridgeIntel(ac::ITelemetrySink& sink, t2_blue::DriverGuard& drivers,
              t2_blue::DeviceWatch& devices);

  void seed_lab_indicators();
  // Scan World for HV bridge / driver / device intel residuals.
  BridgeScanResult scan_world(const sim::World& w);

  t2_blue::DriverGuard& drivers() { return *drivers_; }
  t2_blue::DeviceWatch& devices() { return *devices_; }

 private:
  ac::ITelemetrySink& sink_;
  t2_blue::DriverGuard owned_drivers_;
  t2_blue::DeviceWatch owned_devices_;
  t2_blue::DriverGuard* drivers_ = nullptr;
  t2_blue::DeviceWatch* devices_ = nullptr;
};

}  // namespace t3_blue
