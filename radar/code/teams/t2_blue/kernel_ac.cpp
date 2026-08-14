// kernel_ac.cpp — T2 blue kernel/BYOVD sensor on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_blue/kernel_ac.hpp"

#include <sstream>

namespace t2_blue {

// KernelAc::KernelAc: T2 blue kernel AC: drivers, devices, callbacks.
KernelAc::KernelAc(sim::World& world, ac::ITelemetrySink& sink)
    : world_(world),
      sink_(sink),
      blocklist_(sink),
      drivers_(sink),
      devices_(sink),
      callbacks_(sink) {
  game_pid_ = world_.game_pid();
  blocklist_.add("lab_byovd_hash_001");
}

// KernelAc::blocklist_add: Add a known-bad driver name to the kernel AC blocklist.
void KernelAc::blocklist_add(std::string sha) { blocklist_.add(std::move(sha)); }

// KernelAc::scan: Full T2 blue scan of driver/device/callback residuals.
KernelDetection KernelAc::scan(const t2_red::CallbackState& cbs,
                               const t2_red::CallbackState& baseline) {
  // Apply external callback view for legacy duel path.
  CallbackSnapshot base{baseline.process_notify, baseline.image_notify,
                        baseline.ac_callback_present};
  CallbackSnapshot now{cbs.process_notify, cbs.image_notify,
                       cbs.ac_callback_present};
  callbacks_.set_baseline(base);
  callbacks_.audit(now);

  auto d = full_scan();
  if (cbs.process_notify < baseline.process_notify ||
      cbs.image_notify < baseline.image_notify ||
      (baseline.ac_callback_present && !cbs.ac_callback_present)) {
    d.callback_tamper = true;
    d.reasons.push_back("callback chain degraded (explicit state)");
  }
  return d;
}

// KernelAc::full_scan: Blue: run handle + co-occurrence (and related) sensors once.
KernelDetection KernelAc::full_scan() {
  KernelDetection d;
  if (!game_pid_) {
    game_pid_ = world_.game_pid();
  }

  // 1) Handles (expect clean for pure T2).
  for (const auto& h : world_.handles_to(game_pid_, true)) {
    if (sim::has(h.access, sim::AccessMask::VmRead)) {
      auto* p = world_.proc(h.owner_pid);
      if (p && !p->is_ac && !p->is_game) {
        d.any_handle = true;
        d.reasons.push_back("unexpected VM_READ handle (not pure T2) from " +
                            p->name);
      }
    }
  }

  // 2) BYOVD blocklist
  auto bv = blocklist_.scan_world(world_);
  d.byovd = bv.hit;
  if (bv.hit) {
    d.reasons.push_back(bv.detail);
    for (const auto& h : bv.hits) {
      d.reasons.push_back("BYOVD " + h.image);
      risk_.ingest({ac::EventKind::ByovdBlocked, ac::Tier::T2_KernelByovd, 0, 0,
                    h.image, h.risk});
    }
  }

  // 3) Driver guard (unknown memrw / early / SCM)
  auto dg = drivers_.scan_world(world_);
  d.unknown_memrw_driver = dg.unknown_memrw > 0;
  if (dg.hit) {
    d.reasons.push_back(dg.detail);
    risk_.ingest({ac::EventKind::DriverLoad, ac::Tier::T2_KernelByovd, 0, 0,
                  dg.detail, 2.5});
  }

  // 4) Devices
  auto dv = devices_.scan_world(world_);
  d.suspicious_device = dv.hit;
  if (dv.hit) {
    d.reasons.push_back(dv.detail);
    risk_.ingest({ac::EventKind::DeviceOpen, ac::Tier::T2_KernelByovd, 0, 0,
                  dv.detail, 3.5});
  }

  // 5) Callback integrity
  auto ca = callbacks_.audit_world(world_);
  d.callback_tamper = ca.degraded || ca.shadow_active;
  if (d.callback_tamper) {
    d.reasons.push_back(ca.detail);
    risk_.ingest({ac::EventKind::Generic, ac::Tier::T2_KernelByovd, 0, 0,
                  ca.detail, ca.risk > 0 ? ca.risk : 4.0});
  }

  // 6) Extra residual scars
  if (world_.pool_tag_anomaly) {
    d.pool_tag = true;
    d.reasons.push_back("pool_tag_anomaly");
  }
  if (world_.etw_ti_blind) {
    d.etw_ti = true;
    d.reasons.push_back("etw_ti_blind");
  }
  if (world_.instrumentation_callback) {
    d.reasons.push_back("instrumentation_callback");
  }
  if (world_.wfp_ndis_filter) {
    d.reasons.push_back("wfp_ndis_filter");
  }

  d.risk = risk_.state().score;
  d.mitigated = world_.byovd_policy_block || world_.ranked_access_denied;
  std::ostringstream oss;
  oss << "byovd=" << d.byovd << " memrw=" << d.unknown_memrw_driver
      << " device=" << d.suspicious_device << " cb=" << d.callback_tamper
      << " handle=" << d.any_handle << " risk=" << d.risk
      << " reasons=" << d.reasons.size();
  d.summary = oss.str();
  return d;
}

// KernelAc::mitigate: Apply fail-closed policy (deny ranked / block device / fog) after detect.
bool KernelAc::mitigate() {
  bool ok = blocklist_.mitigate_world(world_);
  if (ok) {
    risk_.ingest({ac::EventKind::ByovdBlocked, ac::Tier::T2_KernelByovd, 0, 0,
                  "mitigate_applied", 1.0});
  }
  return ok;
}

}  // namespace t2_blue
