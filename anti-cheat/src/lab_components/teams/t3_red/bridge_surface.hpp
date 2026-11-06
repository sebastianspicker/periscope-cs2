#pragma once

// Guest-visible HV bridge (primary T3 hunt surface on blue).
// Thin .sys / shared page / VMCALL-class channel — sim.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace t3_red {

// Aggregate outcome fields for `BridgeReport` (lab narrative / tests).
struct BridgeReport {
  bool open = false;
  bool driver_loaded = false;
  bool device_created = false;
  std::string driver_name = "hvcomm.sys";
  std::string device_name = "\\\\.\\AcLabHvComm";
  std::string detail;
};

// Lab type `BridgeSurface` used by this educational unit.
class BridgeSurface {
 public:
  const std::string& device_name() const { return device_; }
  /// Unit path: mark open without World.
  ac::Status open_lab();
  void close();
  bool is_open() const { return open_; }

  /// Multi-step: load is_bridge driver + memrw/VMCALL device on World.
  BridgeReport open_on_world(sim::World& w,
                             const std::string& driver = "hvcomm.sys",
                             const std::string& device = "\\\\.\\AcLabHvComm");

  const BridgeReport& last() const { return last_; }

 private:
  std::string device_ = "\\\\.\\AcLabHvComm";
  bool open_ = false;
  BridgeReport last_{};
};

}  // namespace t3_red
