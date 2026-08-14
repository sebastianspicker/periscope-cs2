// kernel_radar.cpp — T2 red kernel/BYOVD path on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_red/kernel_radar.hpp"

#include "t0_red/entity_pipeline.hpp"

#include <cstring>
#include <sstream>
#include <utility>

namespace t2_red {

// KernelRadar::KernelRadar: T2 red kernel/BYOVD radar orchestrator on World.
KernelRadar::KernelRadar(sim::World& world, std::string ui_name)
    : world_(world), ui_name_(std::move(ui_name)) {
  ui_pid_ = world_.spawn(ui_name_);
  game_pid_ = world_.game_pid();
}

// KernelRadar::bring_up: Red: load path + obtain read capability on lab World.
bool KernelRadar::bring_up(KernelPath path) {
  if (!game_pid_) {
    game_pid_ = world_.game_pid();
  }
  if (auto* g = world_.proc(game_pid_)) {
    if (g->memory.size() < 0x30) {
      world_.plant_lab_entities(game_pid_);
    }
  }

  if (path == KernelPath::Byovd) {
    byovd_.set_candidate(VulnerableDriverRef{
        "LabVulnDrv.sys", "lab_byovd_hash_001", "OldVendor", device_, true});
    auto lr = byovd_.load_on_world(world_);
    driver_sha_ = byovd_.candidate().sha256_hex;
    last_.byovd = lr.loaded;
    last_.brought_up = lr.loaded && lr.device_created;
  } else {
    driver_sha_ = "private_memrw_hash";
    world_.load_driver(sim::Driver{"memrw.sys", driver_sha_, "unknown", false,
                                   false, false, false, true, 20});
    world_.create_device(sim::Device{device_, "memrw.sys", true});
    world_.add_service(sim::ServiceEvent{"memrw_svc", "memrw.sys", true});
    last_.custom_driver = true;
    last_.brought_up = true;
  }

  if (last_.brought_up) {
    auto st = backend_.attach_world(world_, ui_pid_, game_pid_, device_);
    last_.brought_up = (st == ac::Status::Ok);
  }
  last_.device = device_;
  last_.driver_sha = driver_sha_;
  last_.no_game_handle = !has_game_handle();
  last_.detail = last_.brought_up ? "bring_up_ok" : "bring_up_failed";
  return last_.brought_up;
}

// KernelRadar::strip_callbacks: Red T2: clear AC callback presence flags (sim residual).
void KernelRadar::strip_callbacks() {
  auto r = CallbackStripSim::strip_world(world_);
  last_.callback_stripped = r.stripped;
  last_.detail += " " + r.detail;
}

// KernelRadar::enable_physmem_direct_map: Mark the simulated direct physical-memory path.
void KernelRadar::enable_physmem_direct_map() {
  world_.physmem_direct_mapped = true;
  world_.physmem_game_pfn = 0x1A2B0000ull;
  last_.physmem_mapped = true;
  world_.note("t2 KernelRadar physmem_direct_map pfn=0x1A2B0000");
}

// KernelRadar::enable_dkom_token_steal: Mark a simulated SYSTEM token source and ACL bypass.
void KernelRadar::enable_dkom_token_steal() {
  std::uint32_t source_pid = 0;
  for (const auto& [pid, process] : world_.processes) {
    if (process.name == "SYSTEM") {
      source_pid = pid;
      break;
    }
  }
  if (!source_pid) {
    source_pid = world_.spawn("SYSTEM");
  }
  world_.dkom_token_stolen = true;
  world_.dkom_token_source_pid = source_pid;
  world_.token_bypasses_handle_acls = true;
  last_.dkom_stolen = true;
  world_.note("t2 KernelRadar dkom_token_steal source_pid=" +
              std::to_string(source_pid));
}

// KernelRadar::enable_acpi_pm_read: Mark the simulated ACPI PM timer read path.
void KernelRadar::enable_acpi_pm_read(int count) {
  world_.acpi_pm_read_active = true;
  world_.acpi_smi_trigger_count = count;
  last_.acpi_read = true;
  world_.note("t2 KernelRadar acpi_pm_read count=" + std::to_string(count));
}

// KernelRadar::enable_hypercall_read: Mark the simulated platform-hypercall read path.
void KernelRadar::enable_hypercall_read(std::string vendor) {
  world_.hypercall_read_active = true;
  world_.hypercall_vendor = std::move(vendor);
  last_.hypercall_read = true;
  world_.note("t2 KernelRadar hypercall_read vendor=" + world_.hypercall_vendor);
}

// KernelRadar::enable_pool_tag_hide: Mark the simulated pool-tag anomaly.
void KernelRadar::enable_pool_tag_hide() {
  world_.pool_tag_anomaly = true;
  last_.pool_hidden = true;
  world_.note("t2 KernelRadar pool_tag_hide");
}

// KernelRadar::enable_wfp_ndis_filter: Mark the filter and its SCM-shaped residual.
void KernelRadar::enable_wfp_ndis_filter() {
  world_.wfp_ndis_filter = true;
  world_.add_service(
      sim::ServiceEvent{"wfp_filter_svc", "wfp_filter_drv.sys", true});
  last_.wfp_installed = true;
  world_.note("t2 KernelRadar wfp_ndis_filter");
}

// KernelRadar::enable_etw_ti_blind: Mark the simulated ETW Threat Intelligence blind state.
void KernelRadar::enable_etw_ti_blind() {
  world_.etw_ti_blind = true;
  world_.etw_enabled = false;
  last_.etw_ti_blind = true;
  world_.note("t2 KernelRadar etw_ti_blind");
}

// KernelRadar::enable_instrumentation_callback: Mark instrumentation callback state.
void KernelRadar::enable_instrumentation_callback() {
  world_.instrumentation_callback = true;
  last_.instr_callback = true;
  world_.note("t2 KernelRadar instrumentation_callback");
}

// KernelRadar::enable_hal_heap_exploit: Mark simulated HAL heap payload execution.
void KernelRadar::enable_hal_heap_exploit() {
  world_.hal_heap_exploit_active = true;
  world_.hal_heap_payload_executed = true;
  last_.hal_heap_exploit = true;
  world_.note("t2 KernelRadar hal_heap_exploit");
}

// KernelRadar::enable_null_ptr_deref_exploit: Mark simulated kernel NULL dereference.
void KernelRadar::enable_null_ptr_deref_exploit() {
  world_.null_ptr_deref_exploit_active = true;
  last_.null_ptr_deref = true;
  world_.note("t2 KernelRadar null_ptr_deref_exploit");
}

// KernelRadar::enable_dpc_execution: Mark simulated DPC queue execution.
void KernelRadar::enable_dpc_execution(int queue_count) {
  world_.dpc_execution_active = true;
  world_.dpc_queue_count = queue_count;
  last_.dpc_execution = true;
  last_.dpc_queue_count = queue_count;
  world_.note("t2 KernelRadar dpc_execution queue_count=" +
              std::to_string(queue_count));
}

// KernelRadar::enable_module_shadowing: Mark simulated driver list unlinking.
void KernelRadar::enable_module_shadowing(std::string driver_name) {
  world_.module_shadowing_active = true;
  world_.shadowed_driver_name = std::move(driver_name);
  last_.module_shadowing = true;
  world_.note("t2 KernelRadar module_shadowing driver=" +
              world_.shadowed_driver_name);
}

// KernelRadar::has_game_handle: Return/query has game handle for this lab unit.
bool KernelRadar::has_game_handle() const {
  for (const auto& h : world_.handles_to(game_pid_, true)) {
    if (h.owner_pid == ui_pid_ && sim::has(h.access, sim::AccessMask::VmRead)) {
      return true;
    }
  }
  return false;
}

// KernelRadar::pull_entities: Refresh entity pipeline and cache living set for radar UI.
bool KernelRadar::pull_entities() {
  entities_.clear();
  if (!backend_.is_attached()) {
    // Fallback direct device_ioctl if backend not attached.
    auto* g = world_.proc(game_pid_);
    if (!g) {
      return false;
    }
    std::vector<std::uint8_t> count_b;
    if (!world_.device_ioctl_read(ui_pid_, device_, game_pid_, g->base, 4,
                                  count_b)) {
      last_.entities_ok = false;
      return false;
    }
  }

  auto* g = world_.proc(game_pid_);
  if (!g) {
    return false;
  }
  t0_red::EntityPipeline pipe(backend_);
  if (backend_.is_attached()) {
    if (pipe.refresh(g->base) != ac::Status::Ok) {
      last_.entities_ok = false;
      last_.detail = "refresh_failed";
      return false;
    }
    entities_ = pipe.entities();
  } else {
    // Direct path
    std::vector<std::uint8_t> count_b;
    if (!world_.device_ioctl_read(ui_pid_, device_, game_pid_, g->base, 4,
                                  count_b) ||
        count_b.size() < 4) {
      return false;
    }
    std::uint32_t count = 0;
    std::memcpy(&count, count_b.data(), 4);
    struct Ent {
      float x, y, z;
      std::uint8_t team, alive, pad[2];
    };
    for (std::uint32_t i = 0; i < count && i < 32; ++i) {
      std::vector<std::uint8_t> eb;
      if (!world_.device_ioctl_read(ui_pid_, device_, game_pid_,
                                    g->base + 0x10 + i * sizeof(Ent),
                                    sizeof(Ent), eb)) {
        return false;
      }
      Ent e{};
      std::memcpy(&e, eb.data(), sizeof(e));
      entities_.push_back(
          ac::EntitySnapshot{i, {e.x, e.y, e.z}, e.team, e.alive != 0});
    }
  }

  last_.entities_ok = !entities_.empty();
  last_.entity_count = static_cast<int>(entities_.size());
  last_.ioctl_ops = backend_.ioctl_ops();
  last_.bytes_read = backend_.bytes_read_total();
  last_.no_game_handle = !has_game_handle();
  return last_.entities_ok;
}

// KernelRadar::run_full_loop: Red attach/read then blue sensors/mitigate on one World tick.
KernelRadarReport KernelRadar::run_full_loop(KernelPath path, bool strip_cbs) {
  last_ = {};
  if (!bring_up(path)) {
    return last_;
  }
  if (strip_cbs) {
    strip_callbacks();
  }
  if (!pull_entities()) {
    return last_;
  }
  std::ostringstream oss;
  oss << "full_loop entities=" << last_.entity_count
      << " no_handle=" << (last_.no_game_handle ? 1 : 0)
      << " byovd=" << (last_.byovd ? 1 : 0)
      << " custom=" << (last_.custom_driver ? 1 : 0)
      << " ioctl_ops=" << last_.ioctl_ops << " bytes=" << last_.bytes_read
      << " stripped=" << (last_.callback_stripped ? 1 : 0);
  last_.detail = oss.str();
  world_.note("t2 KernelRadar " + last_.detail);
  return last_;
}

// KernelRadar::run_full_stealth_loop: Compose all simulated advanced T2 scar paths.
KernelRadarReport KernelRadar::run_full_stealth_loop(KernelPath path) {
  last_ = {};
  if (!bring_up(path)) {
    return last_;
  }
  strip_callbacks();
  enable_dkom_token_steal();
  enable_physmem_direct_map();
  enable_acpi_pm_read(1);
  enable_hypercall_read();
  enable_pool_tag_hide();
  enable_wfp_ndis_filter();
  enable_etw_ti_blind();
  enable_instrumentation_callback();
  enable_hal_heap_exploit();
  enable_null_ptr_deref_exploit();
  enable_dpc_execution(1);
  enable_module_shadowing("LabVulnDrv.sys");
  pull_entities();

  std::ostringstream oss;
  oss << "full_stealth entities=" << last_.entity_count
      << " no_handle=" << (last_.no_game_handle ? 1 : 0)
      << " physmem=" << (last_.physmem_mapped ? 1 : 0)
      << " dkom=" << (last_.dkom_stolen ? 1 : 0)
      << " acpi=" << (last_.acpi_read ? 1 : 0)
      << " hypercall=" << (last_.hypercall_read ? 1 : 0)
      << " pool=" << (last_.pool_hidden ? 1 : 0)
       << " wfp=" << (last_.wfp_installed ? 1 : 0)
       << " etw_ti=" << (last_.etw_ti_blind ? 1 : 0)
       << " instr=" << (last_.instr_callback ? 1 : 0)
       << " hal_heap=" << (last_.hal_heap_exploit ? 1 : 0)
       << " null_deref=" << (last_.null_ptr_deref ? 1 : 0)
       << " dpc=" << (last_.dpc_execution ? 1 : 0)
       << " dpc_count=" << last_.dpc_queue_count
       << " module_shadow=" << (last_.module_shadowing ? 1 : 0);
  last_.detail = oss.str();
  world_.note("t2 KernelRadar " + last_.detail);
  return last_;
}

}  // namespace t2_red
