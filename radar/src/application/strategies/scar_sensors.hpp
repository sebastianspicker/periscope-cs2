#pragma once

// Tier-aware scar inventory: pure scanners over sim::World used by blue
// multi-reason detectors. Every function inspects live World state — no stubs.

#include "sim/world.hpp"
#include "strategies/strategy_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace strategies::sensors {

/// Foreign VM_READ handle edge against the game process.
struct ForeignHandleHit {
  std::uint32_t owner_pid = 0;
  std::string owner_name;
  bool brief_reopen = false;
  bool hidden_during_enum = false;
  bool via_proxy = false;
  bool via_syscall = false;
};

/// Untrusted memory-capable / known-bad driver residual.
struct DriverHit {
  std::string name;
  std::string sha256;
  bool provides_mem_rw = false;
  bool byovd_known_bad = false;
  bool is_bridge = false;
  int load_order = 0;
};

/// Overlay / presentation residual.
struct OverlayHit {
  std::uint32_t owner_pid = 0;
  std::string title;
  bool topmost = false;
  bool stream_proof = false;
  bool hijacks_swapchain = false;
  bool window_hijacked = false;
};

/// Aggregate scar inventory across delivery / trust / feature surfaces.
struct ScarInventory {
  std::vector<ForeignHandleHit> foreign_handles;
  std::vector<DriverHit> untrusted_drivers;
  std::vector<OverlayHit> overlays;
  int remote_read_ops = 0;
  int remote_read_bytes = 0;
  bool has_untrusted_memrw_device = false;
  bool physmem_open = false;
  bool personal_hv = false;
  bool dma_device = false;
  bool iommu_off = false;
  bool etw_blind = false;
  bool callback_degraded = false;
  bool input_mixed = false;
  bool capture_residual = false;
  bool vpn_proxy = false;
  bool silent_aim = false;
  bool ranked_denied = false;
  int total_scars = 0;
  std::string summary;
};

/// Count foreign (non-game, non-AC) processes holding VM_READ on the game.
int count_foreign_vm_read(const sim::World& w, bool include_hidden = true);

/// List foreign VM_READ edges with owner metadata.
std::vector<ForeignHandleHit> list_foreign_vm_read(const sim::World& w,
                                                   bool include_hidden = true);

/// True if any foreign process holds VM_READ on the game.
bool has_foreign_vm_read(const sim::World& w, bool include_hidden = true);

/// Untrusted drivers that provide mem-rw, BYOVD, or HV bridge.
std::vector<DriverHit> list_untrusted_drivers(const sim::World& w);

/// Any non-AC driver with provides_mem_rw.
bool has_untrusted_memrw_driver(const sim::World& w);

/// Any driver with byovd_known_bad.
bool has_known_bad_byovd(const sim::World& w);

/// Any device exposing mem_rw_ioctl.
bool has_memrw_device(const sim::World& w);

/// Named device present with optional mem-rw requirement.
bool has_device_named(const sim::World& w, const std::string& name,
                     bool require_mem_rw = false);

/// Named driver present.
bool has_driver_named(const sim::World& w, const std::string& name);

/// Game process module integrity scars (IAT/EAT/present/hash).
int count_module_integrity_scars(const sim::World& w);

/// Process-level execution scars (hollow, hijack, foreign thread, manual map).
int count_execution_scars(const sim::World& w);

/// Trust posture failures relevant to ranked play.
int count_trust_posture_scars(const sim::World& w);

/// Kernel notify / object / minifilter / registry callback degradation.
int count_callback_degradation(const sim::World& w);

/// Input provenance residuals (inject / mixed / hooks / block).
int count_input_scars(const sim::World& w);

/// Capture / clone / desktop-dup / printwindow residuals.
int count_capture_scars(const sim::World& w);

/// Full inventory pass — populates ScarInventory and total_scars.
ScarInventory inventory(const sim::World& w);

/// Convert inventory into BlueOutcome signals (one reason per scar family).
BlueOutcome inventory_to_blue(const ScarInventory& inv, double base_weight = 0.18);

}  // namespace strategies::sensors
