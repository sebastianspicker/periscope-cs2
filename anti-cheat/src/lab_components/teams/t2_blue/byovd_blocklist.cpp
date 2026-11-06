// byovd_blocklist.cpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_blue/byovd_blocklist.hpp"

#include <sstream>

namespace t2_blue {

// ByovdBlocklist::ByovdBlocklist: Known-bad driver blocklist for lab BYOVD.
ByovdBlocklist::ByovdBlocklist(ac::ITelemetrySink& sink) : sink_(sink) {
  // Default lab catalog entry.
  blocked_.insert("lab_byovd_hash_001");
  blocked_.insert("sha-byovd-bad");
}

// ByovdBlocklist::add: Add a driver name/sha to the blocklist.
void ByovdBlocklist::add(std::string sha256_hex) {
  blocked_.insert(std::move(sha256_hex));
}

// ByovdBlocklist::is_blocked: Return/query is blocked for this lab unit.
bool ByovdBlocklist::is_blocked(const std::string& sha256_hex) const {
  return blocked_.find(sha256_hex) != blocked_.end();
}

// ByovdBlocklist::check_and_emit: Match World drivers against blocklist; emit telemetry hits.
bool ByovdBlocklist::check_and_emit(const std::string& sha256_hex,
                                    const std::string& image) {
  if (!is_blocked(sha256_hex)) {
    return false;
  }
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::ByovdBlocked,
      .related_tier = ac::Tier::T2_KernelByovd,
      .subject_pid = 0,
      .object_pid = 0,
      .detail = image,
      .risk_delta = 8.0,
  });
  return true;
}

// ByovdBlocklist::scan_world: Walk World processes for staging residuals via StagingWatch.
ByovdScanResult ByovdBlocklist::scan_world(const sim::World& w) {
  ByovdScanResult r;
  for (const auto& d : w.drivers) {
    if (d.is_ac) {
      continue;
    }
    if (d.byovd_known_bad || is_blocked(d.sha256)) {
      ByovdHit h{d.name, d.sha256, 8.0};
      r.hits.push_back(h);
      r.hit = true;
      check_and_emit(d.sha256, d.name);
    }
  }
  std::ostringstream oss;
  oss << "byovd hits=" << r.hits.size();
  r.detail = oss.str();
  return r;
}

// ByovdBlocklist::mitigate_world: Set BYOVD policy deny flags on World.
bool ByovdBlocklist::mitigate_world(sim::World& w) {
  auto sc = scan_world(w);
  if (!sc.hit) {
    return false;
  }
  w.byovd_policy_block = true;
  w.ranked_access_denied = true;
  for (auto& d : w.drivers) {
    if (d.byovd_known_bad || is_blocked(d.sha256)) {
      d.provides_mem_rw = false;
    }
  }
  for (auto& dev : w.devices) {
    for (const auto& drv : w.drivers) {
      if (drv.name == dev.owner_driver &&
          (drv.byovd_known_bad || is_blocked(drv.sha256))) {
        dev.mem_rw_ioctl = false;
      }
    }
  }
  w.note("t2 ByovdBlocklist mitigate policy_block+ranked_deny");
  return true;
}

}  // namespace t2_blue
