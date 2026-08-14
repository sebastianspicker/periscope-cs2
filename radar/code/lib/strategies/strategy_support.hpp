#pragma once

// Pure shared helpers for strategy examples. Every pair still implements its
// own multi-step red / multi-reason blue path; these helpers inventory World
// scars and score risk without a mega-switch dispatcher.
//
// Back-compat: examples::support::{has_device,has_driver,add_signal,risk_for}.

#include "sim/world.hpp"
#include "strategies/multi_reason.hpp"
#include "strategies/scar_sensors.hpp"
#include "strategies/strategy_types.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace examples::support {

inline bool has_device(const sim::World& w, const std::string& name) {
  return strategies::sensors::has_device_named(w, name, /*require_mem_rw=*/true);
}

inline bool has_driver(const sim::World& w, const std::string& name) {
  return strategies::sensors::has_driver_named(w, name);
}

inline bool has_untrusted_memrw_driver(const sim::World& w) {
  return strategies::sensors::has_untrusted_memrw_driver(w);
}

inline void add_signal(int& signals, std::vector<std::string>& reasons,
                       bool present, const char* reason) {
  strategies::add_signal(signals, reasons, present, reason);
}

inline double risk_for(int signals) {
  return strategies::risk_for_signals(signals);
}

/// Convenience: full scar inventory over World.
inline strategies::sensors::ScarInventory inventory(const sim::World& w) {
  return strategies::sensors::inventory(w);
}

/// Convenience: multi-reason detector pre-seeded with common scars.
inline strategies::MultiReasonDetector common_blue(const char* id,
                                                   const sim::World& w) {
  strategies::MultiReasonDetector d(id);
  d.set_require_specific_scar(false);
  const auto inv = strategies::sensors::inventory(w);
  d.signal(!inv.foreign_handles.empty(),
           "foreign VM_READ handle edges present", 0.30);
  d.signal(!inv.untrusted_drivers.empty(),
           "untrusted mem-rw/BYOVD/bridge driver present", 0.35);
  d.signal(inv.has_untrusted_memrw_device, "memory r/w device exposed", 0.28);
  d.signal(inv.physmem_open, "physmem mapping path open", 0.32);
  d.signal(inv.personal_hv, "personal hypervisor active", 0.40);
  d.signal(inv.dma_device, "DMA device present", 0.36);
  d.signal(inv.etw_blind, "ETW pipeline blind", 0.22);
  d.signal(inv.callback_degraded, "callback surface degraded", 0.28);
  d.signal(inv.input_mixed, "input provenance residual", 0.24);
  d.signal(inv.capture_residual, "capture residual", 0.20);
  d.signal(inv.remote_read_ops > 0, "remote read telemetry present", 0.18);
  return d;
}

}  // namespace examples::support

// Prefer this namespace for new code; examples::support remains stable.
namespace strategies::support {
using examples::support::add_signal;
using examples::support::common_blue;
using examples::support::has_device;
using examples::support::has_driver;
using examples::support::has_untrusted_memrw_driver;
using examples::support::inventory;
using examples::support::risk_for;
}  // namespace strategies::support
