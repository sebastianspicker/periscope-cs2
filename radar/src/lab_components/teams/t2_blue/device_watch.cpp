// device_watch.cpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_blue/device_watch.hpp"

#include <algorithm>
#include <sstream>

namespace t2_blue {

// DeviceWatch::DeviceWatch: Blue watch for suspicious Device nodes.
DeviceWatch::DeviceWatch(ac::ITelemetrySink& sink) : sink_(sink) {
  suspicious_ = {"AcLabMemRw", "MemRw", "VulnCap", "PhysMem", "\\\\.\\"};
}

// DeviceWatch::set_suspicious_names: Set suspicious names on this lab unit.
void DeviceWatch::set_suspicious_names(std::vector<std::string> names) {
  suspicious_ = std::move(names);
}

// DeviceWatch::on_device_open: Ingest a device-open event and score risk.
void DeviceWatch::on_device_open(const DeviceOpenEvent& ev) {
  const bool hit = std::any_of(
      suspicious_.begin(), suspicious_.end(), [&](const std::string& n) {
        return ev.device_name.find(n) != std::string::npos;
      });
  if (!hit) {
    return;
  }
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::DeviceOpen,
      .related_tier = ac::Tier::T2_KernelByovd,
      .subject_pid = ev.pid,
      .object_pid = 0,
      .detail = ev.device_name,
      .risk_delta = 3.5,
  });
}

// DeviceWatch::scan_world: Walk World processes for staging residuals via StagingWatch.
DeviceScanResult DeviceWatch::scan_world(const sim::World& w) {
  DeviceScanResult r;
  for (const auto& d : w.devices) {
    if (d.mem_rw_ioctl) {
      ++r.mem_rw_devices;
      r.names.push_back(d.name);
      r.hit = true;
      on_device_open({0, d.name});
    }
  }
  if (w.physmem_device_open) {
    ++r.physmem;
    r.hit = true;
    r.names.push_back("physmem");
    sink_.emit({ac::EventKind::DeviceOpen, ac::Tier::T2_KernelByovd, 0, 0,
                "physmem_device_open", 4.0});
  }
  std::ostringstream oss;
  oss << "devices mem_rw=" << r.mem_rw_devices << " physmem=" << r.physmem;
  r.detail = oss.str();
  return r;
}

}  // namespace t2_blue
