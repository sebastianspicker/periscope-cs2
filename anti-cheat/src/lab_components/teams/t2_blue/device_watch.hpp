// device_watch.hpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t2_blue {

// DeviceOpenEvent: lab type for this educational unit.
struct DeviceOpenEvent {
  std::uint32_t pid = 0;
  std::string device_name;
};

// DeviceScanResult: lab type for this educational unit.
struct DeviceScanResult {
  bool hit = false;
  int mem_rw_devices = 0;
  int physmem = 0;
  std::vector<std::string> names;
  std::string detail;
};

// DeviceWatch: lab type for this educational unit.
class DeviceWatch {
 public:
  explicit DeviceWatch(ac::ITelemetrySink& sink);
  void set_suspicious_names(std::vector<std::string> names);
  void on_device_open(const DeviceOpenEvent& ev);

  /// Scan World devices for mem_rw_ioctl / physmem / name heuristics.
  DeviceScanResult scan_world(const sim::World& w);

 private:
  ac::ITelemetrySink& sink_;
  std::vector<std::string> suspicious_;
};

}  // namespace t2_blue
