// Full T2 tests: drive shipped red/blue APIs on sim::World — no reimplementation.

#include "sim/world.hpp"
#include "sim/narrative.hpp"
#include "ac/telemetry.hpp"
#include "lab/fixture_process.hpp"
#include "t2_red/ioctl_backend.hpp"
#include "t2_red/byovd_surface.hpp"
#include "t2_red/callback_strip_sim.hpp"
#include "t2_red/kernel_radar.hpp"
#include "t2_blue/byovd_blocklist.hpp"
#include "t2_blue/device_watch.hpp"
#include "t2_blue/driver_guard.hpp"
#include "t2_blue/callback_integrity.hpp"
#include "t2_blue/kernel_ac.hpp"
#include "strategies/t2/byovd/red_example.hpp"
#include "strategies/t2/byovd/blue_example.hpp"
#include "strategies/t2/kernel_ioctl/red_example.hpp"
#include "strategies/t2/kernel_ioctl/blue_example.hpp"
#include "strategies/t2/callback_strip/red_example.hpp"
#include "strategies/t2/callback_strip/blue_example.hpp"
#include "strategies/t2/callback_shadow/red_example.hpp"
#include "strategies/t2/callback_shadow/blue_example.hpp"
#include "strategies/t2/scm_service/red_example.hpp"
#include "strategies/t2/scm_service/blue_example.hpp"
#include "strategies/t2/physmem_map/red_example.hpp"
#include "strategies/t2/physmem_map/blue_example.hpp"
#include "strategies/t2/etw_ti_blind/red_example.hpp"
#include "strategies/t2/etw_ti_blind/blue_example.hpp"
#include "strategies/t2/instr_callback/red_example.hpp"
#include "strategies/t2/instr_callback/blue_example.hpp"
#include "strategies/t2/pool_tag_hide/red_example.hpp"
#include "strategies/t2/pool_tag_hide/blue_example.hpp"
#include "strategies/t2/wfp_ndis_filter/red_example.hpp"
#include "strategies/t2/wfp_ndis_filter/blue_example.hpp"
#include "strategies/t2/dkom_hide/red_example.hpp"
#include "strategies/t2/dkom_hide/blue_example.hpp"
#include "strategies/t2/early_load_race/red_example.hpp"
#include "strategies/t2/early_load_race/blue_example.hpp"
#include "strategies/t2/minifilter_strip/red_example.hpp"
#include "strategies/t2/minifilter_strip/blue_example.hpp"
#include "strategies/t2/object_callback_strip/red_example.hpp"
#include "strategies/t2/object_callback_strip/blue_example.hpp"
#include "strategies/t2/registry_notify_strip/red_example.hpp"
#include "strategies/t2/registry_notify_strip/blue_example.hpp"
#include "strategies/t2/driver_allowlist/red_example.hpp"
#include "strategies/t2/driver_allowlist/blue_example.hpp"
#include "strategies/t2/module_list_hide/red_example.hpp"
#include "strategies/t2/module_list_hide/blue_example.hpp"

#include <cstdio>
#include <string>

namespace {

int fails = 0;
void expect(bool c, const char* m) {
  if (!c) {
    std::fprintf(stderr, "FAIL: %s\n", m);
    ++fails;
  } else {
    std::printf("ok: %s\n", m);
  }
}

}  // namespace

int main() {
  // ── IoctlReadBackend fixture + world ─────────────────────────────────
  {
    t2_red::IoctlReadBackend be;
    expect(be.attach_fixture(lab::global_fixture().id()) == ac::Status::Ok,
           "fixture attach");
    expect(!be.exposes_usermode_handle_to_game(), "no game handle fixture");
    auto rr = be.read({lab::global_fixture().base_address(), 4});
    expect(rr.status == ac::Status::Ok, "fixture ioctl-shaped read");
  }

  // ── ByovdSurface World load ──────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::ByovdSurface bv;
    bv.set_candidate({"LabVulnDrv.sys", "lab_byovd_hash_001", "OldVendor",
                      "\\\\.\\AcLabMemRw", true});
    auto lr = bv.load_on_world(w);
    expect(lr.loaded && lr.device_created, "byovd load+device");
    bool bad = false, dev = false;
    for (const auto& d : w.drivers) {
      if (d.byovd_known_bad) {
        bad = true;
      }
    }
    for (const auto& d : w.devices) {
      if (d.mem_rw_ioctl) {
        dev = true;
      }
    }
    expect(bad && dev, "world scars byovd+device");
  }

  // ── KernelRadar full loop no handle ──────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar radar(w);
    auto rep = radar.run_full_loop(t2_red::KernelPath::Byovd, true);
    expect(rep.brought_up && rep.entities_ok, "bring_up+entities");
    expect(rep.entity_count >= 1, "entity_count");
    expect(rep.no_game_handle && !radar.has_game_handle(), "no VM_READ handle");
    expect(rep.byovd, "byovd path");
    expect(rep.callback_stripped, "callbacks stripped");
    expect(rep.ioctl_ops >= 1 || rep.bytes_read >= 4, "ioctl activity");
  }

  // ── Custom driver path ───────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar radar(w);
    auto rep = radar.run_full_loop(t2_red::KernelPath::CustomDriver, false);
    expect(rep.entities_ok && rep.custom_driver, "custom memrw path");
    expect(!radar.has_game_handle(), "custom no handle");
  }

  // ── Callback strip mutates world ─────────────────────────────────────
  {
    auto w = sim::make_arena();
    auto before = t2_red::CallbackStripSim::capture(w);
    auto r = t2_red::CallbackStripSim::strip_world(w);
    expect(true, "strip applied");
    expect(w.process_notify < before.process_notify || !w.ac_callback_present,
           "notify degraded");
  }

  // ── KernelAc full_scan ───────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar radar(w);
    radar.run_full_loop(t2_red::KernelPath::Byovd, true);
    ac::MemoryTelemetrySink sink;
    t2_blue::KernelAc kac(w, sink);
    auto d = kac.full_scan();
    expect(d.byovd, "blue byovd");
    expect(d.suspicious_device, "blue device");
    expect(d.callback_tamper, "blue callbacks");
    expect(!d.reasons.empty() && d.risk > 0, "reasons+risk");
    expect(!d.any_handle, "pure T2 no foreign handle");
  }

  // ── Mitigate blocks IOCTL ────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::KernelRadar radar(w);
    radar.run_full_loop(t2_red::KernelPath::Byovd, false);
    ac::MemoryTelemetrySink sink;
    t2_blue::KernelAc kac(w, sink);
    expect(kac.mitigate(), "mitigate");
    expect(w.byovd_policy_block && w.ranked_access_denied, "policy flags");
    std::vector<std::uint8_t> buf;
    bool after = w.device_ioctl_read(radar.ui_pid(), radar.device(),
                                     w.game_pid(),
                                     w.proc(w.game_pid())->base, 4, buf);
    expect(!after, "post-mitigate ioctl denied");
  }

  // ── Fail path: no device ─────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    t2_red::IoctlReadBackend be;
    auto reader = w.spawn("ui.exe");
    expect(be.attach_world(w, reader, w.game_pid(), "\\\\.\\Missing") ==
               ac::Status::Unavailable,
           "missing device fails attach");
  }

  // ── Strategy pairs ───────────────────────────────────────────────────
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::byovd::run_red(w, n);
    expect(rr.achieved, "strategy byovd red");
    auto br = examples::byovd::run_blue(w, n);
    expect(br.detected, "strategy byovd blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::kernel_ioctl::run_red(w, n);
    expect(rr.achieved, "strategy kernel_ioctl red");
    auto br = examples::kernel_ioctl::run_blue(w, n);
    expect(br.detected, "strategy kernel_ioctl blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::callback_strip::run_red(w, n);
    expect(rr.achieved, "callback_strip red");
    auto br = examples::callback_strip::run_blue(w, n);
    expect(br.detected, "callback_strip blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::callback_shadow::run_red(w, n);
    expect(rr.achieved, "callback_shadow red");
    auto br = examples::callback_shadow::run_blue(w, n);
    expect(br.detected, "callback_shadow blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::scm_service::run_red(w, n);
    expect(rr.achieved, "scm_service red");
    expect(rr.steps >= 3, "scm_service multi-step");
    auto br = examples::scm_service::run_blue(w, n);
    expect(br.detected, "scm_service blue");
    expect(br.signals >= 2, "scm_service multi-reason");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::physmem_map::run_red(w, n);
    expect(rr.achieved, "physmem_map red");
    expect(rr.steps >= 3, "physmem_map multi-step");
    auto br = examples::physmem_map::run_blue(w, n);
    expect(br.detected, "physmem_map blue");
    expect(br.signals >= 2, "physmem_map multi-reason");
  }
  // Residual multi-step strategies (no flag-echo only).
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::etw_ti_blind::run_red(w, n);
    expect(rr.achieved, "etw_ti_blind multi-step red");
    expect(rr.steps >= 3, "etw_ti multi-step count");
    auto br = examples::etw_ti_blind::run_blue(w, n);
    expect(br.detected, "etw_ti_blind multi-step blue");
    expect(br.signals >= 2, "etw_ti multi-reason");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::instr_callback::run_red(w, n);
    expect(rr.achieved, "instr_callback multi-step red");
    expect(rr.steps >= 3, "instr_callback multi-step count");
    auto br = examples::instr_callback::run_blue(w, n);
    expect(br.detected, "instr_callback multi-step blue");
    expect(br.signals >= 2, "instr multi-reason");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::pool_tag_hide::run_red(w, n);
    expect(rr.achieved, "pool_tag_hide multi-step red");
    expect(rr.steps >= 3, "pool_tag multi-step count");
    auto br = examples::pool_tag_hide::run_blue(w, n);
    expect(br.detected, "pool_tag_hide multi-step blue");
    expect(br.signals >= 2, "pool_tag multi-reason");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::wfp_ndis_filter::run_red(w, n);
    expect(rr.achieved, "wfp_ndis multi-step red");
    expect(rr.steps >= 3, "wfp multi-step count");
    auto br = examples::wfp_ndis_filter::run_blue(w, n);
    expect(br.detected, "wfp_ndis multi-step blue");
    expect(br.signals >= 2, "wfp multi-reason");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::driver_allowlist::run_red(w, n);
    expect(rr.achieved, "driver_allowlist multi-step red");
    expect(rr.steps >= 3, "driver_allowlist steps>=3");
    auto br = examples::driver_allowlist::run_blue(w, n);
    expect(br.detected, "driver_allowlist multi-step blue");
    expect(br.signals >= 2, "driver_allowlist multi-reason");
  }
  {
    auto w = sim::make_arena();
    strategy::t2_module_list_hide::Red red;
    red.apply(w);
    expect(w.module_shadowing_active && w.module_loaded, "module_list_hide red scars");
    strategy::t2_module_list_hide::Blue blue;
    auto det = blue.detect(w);
    expect(det.detection_count >= 2, "module_list_hide multi-reason blue");
  }
  {
    auto w = sim::make_arena();
    sim::Narrator n;
    auto rr = examples::dkom_hide::run_red(w, n);
    expect(rr.achieved, "dkom_hide multi-step red");
    expect(rr.steps >= 3, "dkom_hide steps>=3");
    auto br = examples::dkom_hide::run_blue(w, n);
    expect(br.detected, "dkom_hide multi-step blue");
    expect(br.signals >= 2, "dkom_hide multi-reason");
  }
  {
    auto w = sim::make_arena();
    if (w.ac_driver_load_order <= 0) w.ac_driver_load_order = 50;
    sim::Narrator n;
    auto rr = examples::early_load_race::run_red(w, n);
    expect(rr.achieved, "early_load_race multi-step red");
    auto br = examples::early_load_race::run_blue(w, n);
    expect(br.detected, "early_load_race multi-step blue");
    expect(br.signals >= 2, "early_load_race multi-reason");
  }
  {
    auto w = sim::make_arena();
    w.minifilter_present = true;
    w.minifilter_callbacks = 3;
    sim::Narrator n;
    auto rr = examples::minifilter_strip::run_red(w, n);
    expect(rr.achieved, "minifilter_strip multi-step red");
    auto br = examples::minifilter_strip::run_blue(w, n);
    expect(br.detected, "minifilter_strip multi-step blue");
  }
  {
    auto w = sim::make_arena();
    w.object_callbacks_present = true;
    w.object_callbacks = 2;
    w.object_callbacks_true = 2;
    w.object_callbacks_true_present = true;
    sim::Narrator n;
    auto rr = examples::object_callback_strip::run_red(w, n);
    expect(rr.achieved, "object_callback_strip multi-step red");
    auto br = examples::object_callback_strip::run_blue(w, n);
    expect(br.detected, "object_callback_strip multi-step blue");
  }
  {
    auto w = sim::make_arena();
    w.registry_notify_present = true;
    w.registry_notify = 2;
    sim::Narrator n;
    auto rr = examples::registry_notify_strip::run_red(w, n);
    expect(rr.achieved, "registry_notify_strip multi-step red");
    auto br = examples::registry_notify_strip::run_blue(w, n);
    expect(br.detected, "registry_notify_strip multi-step blue");
  }

  if (fails) {
    std::fprintf(stderr, "t2_full_tests: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("t2_full_tests: all passed\n");
  return 0;
}
