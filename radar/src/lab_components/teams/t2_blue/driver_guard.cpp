// driver_guard.cpp — implements driver_guard (T2 blue).
// Key methods: set_allowlist, on_image_load, find, scan_world, find.

#include "t2_blue/driver_guard.hpp"

#include <algorithm>
#include <sstream>

namespace t2_blue {

DriverGuard::DriverGuard(ac::ITelemetrySink& sink) : sink_(sink) {}

// DriverGuard::set_allowlist: Set allowlist on this lab unit.
void DriverGuard::set_allowlist(std::vector<std::string> sha256_hex) {
  allowlist_ = std::move(sha256_hex);
}

// DriverGuard::on_image_load: Ingest a driver image load and score.
void DriverGuard::on_image_load(const DriverLoadEvent& ev) {
  const bool allowed =
      std::find(allowlist_.begin(), allowlist_.end(), ev.sha256_hex) !=
      allowlist_.end();
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::DriverLoad,
      .related_tier = ac::Tier::T2_KernelByovd,
      .subject_pid = 0,
      .object_pid = 0,
      .detail = ev.path + (allowed ? "|allow" : "|unknown"),
      .risk_delta = allowed ? 0.0 : 2.5,
  });
}

// DriverGuard::scan_world: Walk World processes for staging residuals via StagingWatch.
DriverScanResult DriverGuard::scan_world(const sim::World& w) {
  DriverScanResult r;
  for (const auto& d : w.drivers) {
    if (d.is_ac) {
      continue;
    }
    const bool allowed =
        std::find(allowlist_.begin(), allowlist_.end(), d.sha256) !=
        allowlist_.end();
    if (d.provides_mem_rw && !allowed && !d.byovd_known_bad) {
      ++r.unknown_memrw;
      r.names.push_back(d.name);
      r.hit = true;
      on_image_load({d.name, d.sha256, d.signer, d.boot_start});
    }
    if (d.load_order < w.ac_driver_load_order && d.provides_mem_rw) {
      ++r.early_load;
      r.hit = true;
    }
  }
  for (const auto& s : w.services) {
    if (s.kernel_driver) {
      ++r.services;
      r.hit = true;
    }
  }
  std::ostringstream oss;
  oss << "drivers unknown_memrw=" << r.unknown_memrw
      << " early=" << r.early_load << " scm=" << r.services;
  r.detail = oss.str();
  return r;
}

}  // namespace t2_blue
