// bridge_intel.cpp — T3 blue platform/HV sensor using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_blue/bridge_intel.hpp"

#include <sstream>

namespace t3_blue {

// Educational bridge intel: scan World drivers/devices for HV-bridge residuals.
// Owns DriverGuard/DeviceWatch when constructed from a telemetry sink.

BridgeIntel::BridgeIntel(ac::ITelemetrySink& sink)
    : sink_(sink),
      owned_drivers_(sink),
      owned_devices_(sink),
      drivers_(&owned_drivers_),
      devices_(&owned_devices_) {
  seed_lab_indicators();
}

BridgeIntel::BridgeIntel(ac::ITelemetrySink& sink, t2_blue::DriverGuard& drivers,
                         t2_blue::DeviceWatch& devices)
    : sink_(sink),
      owned_drivers_(sink),
      owned_devices_(sink),
      drivers_(&drivers),
      devices_(&devices) {
  seed_lab_indicators();
}

// BridgeIntel::seed_lab_indicators: Seed lab-known bridge indicators for demos.
void BridgeIntel::seed_lab_indicators() {
  if (devices_) {
    devices_->set_suspicious_names(
        {"AcLabHvComm", "HvComm", "VMCALL", "hvcomm"});
  }
}

// BridgeIntel::scan_world: Walk World processes for staging residuals via StagingWatch.
BridgeScanResult BridgeIntel::scan_world(const sim::World& w) {
  BridgeScanResult r;
  for (const auto& d : w.drivers) {
    if (d.is_bridge || d.name.find("hvcomm") != std::string::npos ||
        d.name.find("HvComm") != std::string::npos) {
      r.hit = true;
      r.drivers.push_back(d.name);
      r.risk += 3.0;
      sink_.emit({ac::EventKind::BridgeSuspected, ac::Tier::T3_Hypervisor, 0, 0,
                  d.name, 3.0});
    }
  }
  for (const auto& dev : w.devices) {
    if (dev.name.find("HvComm") != std::string::npos ||
        dev.name.find("VMCALL") != std::string::npos ||
        dev.owner_driver.find("hvcomm") != std::string::npos) {
      r.hit = true;
      r.devices.push_back(dev.name);
      r.risk += 2.5;
      sink_.emit({ac::EventKind::BridgeSuspected, ac::Tier::T3_Hypervisor, 0, 0,
                  dev.name, 2.5});
    }
  }
  std::ostringstream oss;
  oss << "bridge drivers=" << r.drivers.size()
      << " devices=" << r.devices.size() << " risk=" << r.risk;
  r.detail = oss.str();
  return r;
}

}  // namespace t3_blue
