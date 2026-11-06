#pragma once

// Full T2 educational radar: kernel/BYOVD channel, no game VM_READ handle.

#include "ac/types.hpp"
#include "sim/world.hpp"
#include "t2_red/byovd_surface.hpp"
#include "t2_red/callback_strip_sim.hpp"
#include "t2_red/ioctl_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t2_red {

enum class KernelPath { CustomDriver, Byovd, Physmem, AcpiSmbios, Hypercall };

// Aggregate outcome fields for `KernelRadarReport` (lab narrative / tests).
struct KernelRadarReport {
  bool brought_up = false;
  bool entities_ok = false;
  bool no_game_handle = false;
  bool byovd = false;
  bool custom_driver = false;
  bool callback_stripped = false;
  bool physmem_mapped = false;
  bool dkom_stolen = false;
  bool acpi_read = false;
  bool hypercall_read = false;
  bool pool_hidden = false;
  bool wfp_installed = false;
  bool etw_ti_blind = false;
  bool instr_callback = false;
  bool hal_heap_exploit = false;
  bool null_ptr_deref = false;
  bool dpc_execution = false;
  int dpc_queue_count = 0;
  bool module_shadowing = false;
  int entity_count = 0;
  int ioctl_ops = 0;
  std::uint64_t bytes_read = 0;
  std::string driver_sha;
  std::string device;
  std::string detail;
};

// Multi-step educational red client `KernelRadar` — plants lab scars on World only.
class KernelRadar {
 public:
  explicit KernelRadar(sim::World& world, std::string ui_name = "ud-radar.exe");

  bool bring_up(KernelPath path);
  // Read lab entity table through the attached backend into snapshots/blips.
  bool pull_entities();

  /// Optional aggressive callback strip after bring-up.
  void strip_callbacks();

  /// Simulation-only advanced T2 scar paths.
  void enable_physmem_direct_map();
  void enable_dkom_token_steal();
  void enable_acpi_pm_read(int count = 1);
  void enable_hypercall_read(std::string vendor = "hyperv");
  void enable_pool_tag_hide();
  void enable_wfp_ndis_filter();
  void enable_etw_ti_blind();
  void enable_instrumentation_callback();
  void enable_hal_heap_exploit();
  void enable_null_ptr_deref_exploit();
  void enable_dpc_execution(int queue_count);
  void enable_module_shadowing(std::string driver_name);

  /// Full loop: bring_up → optional strip → pull entities.
  KernelRadarReport run_full_loop(KernelPath path = KernelPath::Byovd,
                                  bool strip_cbs = false);

  /// Full advanced simulation loop with every T2 evasion scar enabled.
  KernelRadarReport run_full_stealth_loop(
      KernelPath path = KernelPath::Byovd);

  std::uint32_t ui_pid() const { return ui_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  bool has_game_handle() const;
  const std::string& device() const { return device_; }
  const std::string& driver_sha() const { return driver_sha_; }
  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  const KernelRadarReport& last_report() const { return last_; }
  IoctlReadBackend& backend() { return backend_; }

 private:
  sim::World& world_;
  std::string ui_name_;
  std::uint32_t ui_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  std::string device_ = "\\\\.\\AcLabMemRw";
  std::string driver_sha_;
  std::vector<ac::EntitySnapshot> entities_;
  IoctlReadBackend backend_;
  ByovdSurface byovd_;
  KernelRadarReport last_{};
};

}  // namespace t2_red
