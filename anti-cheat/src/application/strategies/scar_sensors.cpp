#include "strategies/scar_sensors.hpp"

#include <algorithm>
#include <sstream>

namespace strategies::sensors {
namespace {

const sim::Process* game_proc(const sim::World& w) {
  const auto pid = w.game_pid();
  if (pid == 0) return nullptr;
  return w.proc(pid);
}

bool is_foreign(const sim::Process* p) {
  return p != nullptr && !p->is_game && !p->is_ac;
}

}  // namespace

int count_foreign_vm_read(const sim::World& w, bool include_hidden) {
  return static_cast<int>(list_foreign_vm_read(w, include_hidden).size());
}

std::vector<ForeignHandleHit> list_foreign_vm_read(const sim::World& w,
                                                   bool include_hidden) {
  std::vector<ForeignHandleHit> out;
  const auto game = w.game_pid();
  if (game == 0) return out;

  for (const auto& h : w.handles_to(game, include_hidden)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (!is_foreign(owner)) continue;
    ForeignHandleHit hit;
    hit.owner_pid = h.owner_pid;
    hit.owner_name = owner->name;
    hit.brief_reopen = h.brief_reopen;
    hit.hidden_during_enum = h.hidden_during_enum;
    hit.via_proxy = h.via_proxy;
    hit.via_syscall = h.via_syscall_path;
    out.push_back(std::move(hit));
  }
  return out;
}

bool has_foreign_vm_read(const sim::World& w, bool include_hidden) {
  return count_foreign_vm_read(w, include_hidden) > 0;
}

std::vector<DriverHit> list_untrusted_drivers(const sim::World& w) {
  std::vector<DriverHit> out;
  for (const auto& d : w.drivers) {
    if (d.is_ac) continue;
    if (!d.provides_mem_rw && !d.byovd_known_bad && !d.is_bridge) continue;
    DriverHit hit;
    hit.name = d.name;
    hit.sha256 = d.sha256;
    hit.provides_mem_rw = d.provides_mem_rw;
    hit.byovd_known_bad = d.byovd_known_bad;
    hit.is_bridge = d.is_bridge;
    hit.load_order = d.load_order;
    out.push_back(std::move(hit));
  }
  return out;
}

bool has_untrusted_memrw_driver(const sim::World& w) {
  return std::any_of(w.drivers.begin(), w.drivers.end(), [](const auto& d) {
    return !d.is_ac && d.provides_mem_rw;
  });
}

bool has_known_bad_byovd(const sim::World& w) {
  return std::any_of(w.drivers.begin(), w.drivers.end(),
                     [](const auto& d) { return d.byovd_known_bad; });
}

bool has_memrw_device(const sim::World& w) {
  return std::any_of(w.devices.begin(), w.devices.end(),
                     [](const auto& d) { return d.mem_rw_ioctl; });
}

bool has_device_named(const sim::World& w, const std::string& name,
                     bool require_mem_rw) {
  return std::any_of(w.devices.begin(), w.devices.end(), [&](const auto& d) {
    if (d.name != name) return false;
    return !require_mem_rw || d.mem_rw_ioctl;
  });
}

bool has_driver_named(const sim::World& w, const std::string& name) {
  return std::any_of(w.drivers.begin(), w.drivers.end(),
                     [&](const auto& d) { return d.name == name; });
}

int count_module_integrity_scars(const sim::World& w) {
  int n = 0;
  const auto* g = game_proc(w);
  if (!g) return 0;
  for (const auto& m : g->modules) {
    if (m.iat_hooked) ++n;
    if (m.eat_hooked) ++n;
    if (m.present_hooked) ++n;
    if (!m.linked_in_peb) ++n;
    if (m.headers_erased) ++n;
    if (m.text_hash != "clean" && !m.text_hash.empty()) ++n;
  }
  return n;
}

int count_execution_scars(const sim::World& w) {
  int n = 0;
  for (const auto& [pid, p] : w.processes) {
    (void)pid;
    if (p.hollowed) ++n;
    if (p.thread_hijacked) ++n;
    if (p.has_foreign_thread) ++n;
    if (p.manual_mapped_region) ++n;
  }
  return n;
}

int count_trust_posture_scars(const sim::World& w) {
  int n = 0;
  const auto& t = w.trust;
  if (!t.secure_boot) ++n;
  if (!t.vbs) ++n;
  if (!t.hvci || !t.hvci_enabled) ++n;
  if (!t.iommu_on) ++n;
  if (t.personal_hv_active) ++n;
  if (t.dma_device_present) ++n;
  if (!t.attestation_valid || !t.attestation_pcr_ok) ++n;
  if (t.ept_hide_ac_pages) ++n;
  if (t.unexpected_efi_entry) ++n;
  if (t.dual_boot_profile) ++n;
  if (!t.elam_enabled || !t.secure_launch) ++n;
  if (!t.dse_enforced || t.test_signing) ++n;
  if (t.secure_kernel_view_dirty) ++n;
  if (t.timing_spoofed) ++n;
  if (w.ci_options_disabled) ++n;
  if (w.feature_control_spoofed) ++n;
  if (!w.vtl1_enclave_present) ++n;
  if (w.smm_residual) ++n;
  if (w.infinity_hook_residual) ++n;
  return n;
}

int count_callback_degradation(const sim::World& w) {
  int n = 0;
  if (!w.ac_callback_present) ++n;
  if (w.callback_shadow_active) ++n;
  if (!w.object_callbacks_present) ++n;
  if (!w.minifilter_present) ++n;
  if (!w.registry_notify_present) ++n;
  if (!w.etw_enabled) ++n;
  if (w.etw_ti_blind) ++n;
  if (!w.veh_chain_clean) ++n;
  if (w.instrumentation_callback) ++n;
  return n;
}

int count_input_scars(const sim::World& w) {
  int n = 0;
  if (w.raw_sendinput_mixed) ++n;
  if (w.wh_mouse_hook) ++n;
  if (w.block_input_active) ++n;
  if (w.clipcursor_confined) ++n;
  bool has_injected = false;
  bool has_raw = false;
  bool has_serial = false;
  for (const auto& e : w.inputs) {
    if (e.source == "injected") has_injected = true;
    if (e.source == "raw_hid") has_raw = true;
    if (e.source == "serial_arduino" || e.source == "kmbox") has_serial = true;
  }
  if (has_injected) ++n;
  if (has_injected && has_raw) ++n;
  if (has_serial) ++n;
  if (w.silent_aim_active) ++n;
  if (w.triggerbot_active) ++n;
  if (w.rcs_active) ++n;
  return n;
}

int count_capture_scars(const sim::World& w) {
  int n = 0;
  if (w.desktop_duplication || w.desktop_duplication_active) ++n;
  if (w.gdi_bitblt_capture) ++n;
  if (w.printwindow_capture) ++n;
  if (w.external_display_clone) ++n;
  if (w.trust.capture_card_present) ++n;
  if (w.capture_vs_present_mismatch) ++n;
  if (w.wda_excluded_from_capture) ++n;
  for (const auto& o : w.overlays) {
    if (o.stream_proof) ++n;
    if (o.hijacks_swapchain || o.window_hijacked) ++n;
  }
  if (w.windowless_swapchain_hijack || w.swapchain_hijacked_no_window) ++n;
  return n;
}

ScarInventory inventory(const sim::World& w) {
  ScarInventory inv;
  inv.foreign_handles = list_foreign_vm_read(w, true);
  inv.untrusted_drivers = list_untrusted_drivers(w);
  for (const auto& o : w.overlays) {
    OverlayHit hit;
    hit.owner_pid = o.owner_pid;
    hit.title = o.title;
    hit.topmost = o.topmost;
    hit.stream_proof = o.stream_proof;
    hit.hijacks_swapchain = o.hijacks_swapchain;
    hit.window_hijacked = o.window_hijacked;
    inv.overlays.push_back(std::move(hit));
  }
  inv.remote_read_ops = static_cast<int>(w.remote_read_ops);
  inv.remote_read_bytes = static_cast<int>(w.remote_read_bytes);
  inv.has_untrusted_memrw_device = has_memrw_device(w);
  inv.physmem_open = w.physmem_device_open;
  inv.personal_hv = w.trust.personal_hv_active;
  inv.dma_device = w.trust.dma_device_present;
  inv.iommu_off = !w.trust.iommu_on;
  inv.etw_blind = !w.etw_enabled || w.etw_ti_blind;
  inv.callback_degraded = count_callback_degradation(w) > 0;
  inv.input_mixed = w.raw_sendinput_mixed || count_input_scars(w) > 0;
  inv.capture_residual = count_capture_scars(w) > 0;
  inv.vpn_proxy = w.vpn_proxy_active;
  inv.silent_aim = w.silent_aim_active;
  inv.ranked_denied = w.ranked_access_denied;

  inv.total_scars = 0;
  inv.total_scars += static_cast<int>(inv.foreign_handles.size());
  inv.total_scars += static_cast<int>(inv.untrusted_drivers.size());
  if (inv.has_untrusted_memrw_device) ++inv.total_scars;
  if (inv.physmem_open) ++inv.total_scars;
  if (inv.personal_hv) ++inv.total_scars;
  if (inv.dma_device) ++inv.total_scars;
  if (inv.iommu_off) ++inv.total_scars;
  if (inv.etw_blind) ++inv.total_scars;
  if (inv.callback_degraded) ++inv.total_scars;
  if (inv.input_mixed) ++inv.total_scars;
  if (inv.capture_residual) ++inv.total_scars;
  if (inv.vpn_proxy) ++inv.total_scars;
  if (inv.silent_aim) ++inv.total_scars;
  inv.total_scars += count_module_integrity_scars(w);
  inv.total_scars += count_execution_scars(w);
  inv.total_scars += count_trust_posture_scars(w);
  if (inv.remote_read_ops > 0) ++inv.total_scars;

  std::ostringstream oss;
  oss << "scars=" << inv.total_scars
      << " foreign_vm_read=" << inv.foreign_handles.size()
      << " untrusted_drv=" << inv.untrusted_drivers.size()
      << " remote_ops=" << inv.remote_read_ops
      << " trust_hv=" << (inv.personal_hv ? 1 : 0)
      << " dma=" << (inv.dma_device ? 1 : 0);
  inv.summary = oss.str();
  return inv;
}

BlueOutcome inventory_to_blue(const ScarInventory& inv, double base_weight) {
  BlueOutcome out;
  auto add = [&](bool present, const std::string& reason, double w) {
    if (!present) return;
    ++out.signals;
    out.risk += w;
    out.reasons.push_back(reason);
  };

  add(!inv.foreign_handles.empty(),
      "foreign VM_READ handles=" + std::to_string(inv.foreign_handles.size()),
      base_weight + 0.12);
  add(!inv.untrusted_drivers.empty(),
      "untrusted mem-rw/BYOVD/bridge drivers=" +
          std::to_string(inv.untrusted_drivers.size()),
      base_weight + 0.18);
  add(inv.has_untrusted_memrw_device, "memory r/w device exposed",
      base_weight + 0.10);
  add(inv.physmem_open, "physical memory mapping path open", base_weight + 0.14);
  add(inv.personal_hv, "personal hypervisor active", base_weight + 0.20);
  add(inv.dma_device, "DMA-capable device present", base_weight + 0.16);
  add(inv.iommu_off, "IOMMU disabled", base_weight + 0.12);
  add(inv.etw_blind, "ETW / TI pipeline blind", base_weight + 0.10);
  add(inv.callback_degraded, "kernel callback surface degraded",
      base_weight + 0.14);
  add(inv.input_mixed, "input provenance residual", base_weight + 0.10);
  add(inv.capture_residual, "capture / presentation residual",
      base_weight + 0.08);
  add(inv.vpn_proxy, "VPN / proxy residual", base_weight + 0.06);
  add(inv.silent_aim, "silent aim residual", base_weight + 0.14);
  add(inv.remote_read_ops > 0,
      "remote read telemetry ops=" + std::to_string(inv.remote_read_ops),
      base_weight + 0.08);
  add(!inv.overlays.empty(),
      "overlay windows=" + std::to_string(inv.overlays.size()), base_weight + 0.06);

  out.risk = std::min(1.0, out.risk);
  out.detected = out.signals > 0;
  out.mitigated = out.signals >= 3 || out.risk >= 0.55;
  out.detail = inv.summary + " blue_signals=" + std::to_string(out.signals) +
               " risk=" + std::to_string(out.risk);
  return out;
}

}  // namespace strategies::sensors
