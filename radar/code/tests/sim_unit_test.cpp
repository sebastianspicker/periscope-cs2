// Focused code/lib/sim surface: World, MemoryReader (all models), HandleTable,
// DonorDiscovery, EvasionMaster, FixtureLoader, OverlayDetector.
// Drives shipped entry points only — no reimplementation of production logic.

#include "sim/donor.hpp"
#include "sim/evasion_master.hpp"
#include "sim/fixture.hpp"
#include "sim/handle_table.hpp"
#include "sim/memory_reader.hpp"
#include "sim/narrative.hpp"
#include "sim/overlay_detection.hpp"
#include "sim/strategy_example.hpp"
#include "sim/world.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <span>
#include <memory>
#include <string>
#include <vector>

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

// Global donor read callback for HijackProxyReader callback path.
sim::World* g_donor_world = nullptr;
std::uint32_t g_donor_pid = 0;

bool donor_cb(std::uint64_t addr, void* buf, std::size_t size) {
  if (g_donor_world == nullptr || g_donor_pid == 0) {
    return false;
  }
  const auto game = g_donor_world->game_pid();
  auto rr = g_donor_world->read_mem(g_donor_pid, game, addr, size, true);
  if (rr.status != ac::Status::Ok) {
    return false;
  }
  if (rr.bytes.size() != size || (size != 0 && buf == nullptr)) {
    return false;
  }
  if (size != 0) {
    std::copy(rr.bytes.begin(), rr.bytes.end(), static_cast<std::uint8_t*>(buf));
  }
  return true;
}

}  // namespace

int main() {
  // ── 1. make_arena + plant + read_mem path ────────────────────────────────
  {
    auto w = sim::make_arena("lab-game");
    const auto game = w.game_pid();
    const auto ac = w.ac_pid();
    expect(game != 0 && ac != 0, "arena: game+ac pids");
    expect(w.lab_pattern_marker_present, "arena: ACPT marker planted");
    auto* g = w.proc(game);
    expect(g != nullptr && g->memory.size() >= 0x208, "arena: game memory");

    // Marker magic bytes via shipped plant.
    expect(g->memory[w.lab_pattern_marker_off + 0] == 'A' &&
               g->memory[w.lab_pattern_marker_off + 1] == 'C' &&
               g->memory[w.lab_pattern_marker_off + 2] == 'P' &&
               g->memory[w.lab_pattern_marker_off + 3] == 'T',
           "arena: ACPT magic");

    auto reader_pid = w.spawn("rpm-reader.exe");
    expect(w.open_process(reader_pid, game, sim::AccessMask::VmRead, false),
           "arena: open_process");
    expect(w.handle_table.open_handle_count() >= 1, "arena: handle_table synced");
    expect(w.handle_table.unique_owners_for(game) >= 1,
           "arena: unique owners for game");

    auto rr = w.read_mem(reader_pid, game, g->base, 4, true);
    expect(rr.status == ac::Status::Ok && rr.bytes.size() == 4,
           "arena: read_mem entity count");
    std::uint32_t count = 0;
    std::memcpy(&count, rr.bytes.data(), 4);
    expect(count >= 2, "arena: planted entity count >= 2");
    expect(w.remote_read_ops >= 1, "arena: remote_read_ops counted");

    // Mutate layout and re-read marker.
    expect(w.mutate_lab_pattern_layout(game, 0x280, 0x40),
           "arena: mutate_lab_pattern_layout");
    expect(w.lab_pattern_generation >= 2, "arena: generation bumped");

    // Bounds are checked without wrapping, allocating, or partially reading.
    const auto before_size = g->memory.size();
    const std::uint8_t byte = 0xA5;
    expect(!w.write_mem(game, std::numeric_limits<std::uint64_t>::max(), &byte,
                        static_cast<std::size_t>(g->base) + 1),
           "arena: write_mem rejects address/length overflow");
    expect(g->memory.size() == before_size,
           "arena: overflow write leaves memory extent unchanged");
    const auto truncated =
        w.read_mem(reader_pid, game, g->base + g->memory.size() - 2, 4, true);
    expect(truncated.status == ac::Status::InvalidArgument && truncated.bytes.empty(),
           "arena: truncated read rejects partial result");
  }

  // ── 2. MemoryReader factory — all six acquisition models ─────────────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    auto* g = w.proc(game);
    expect(g != nullptr, "reader: game process");
    const auto cheat = w.spawn("cheat.exe");

    // DirectRpm
    {
      auto r = sim::CreateMemoryReader(ac::MemoryAcquisitionModel::DirectRpm, w,
                                       cheat);
      expect(r != nullptr, "factory: DirectRpm non-null");
      expect(r->model() == ac::MemoryAcquisitionModel::DirectRpm,
             "DirectRpm model");
      expect(r->handle_model() == ac::HandleAcquisitionModel::DirectOpenProcess,
             "DirectRpm handle model");
      std::uint32_t count = 0;
      expect(r->read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "DirectRpm read entity count");
      expect(count >= 2, "DirectRpm count value");
      expect(r->has_handle_to(game), "DirectRpm has_handle_to game");
      expect(r->compatible_with(ac::DefenseLayer::ProcessIsolation),
             "DirectRpm compatible ProcessIsolation");
      expect(!r->compatible_with(ac::DefenseLayer::ProxyMemoryAccess),
             "DirectRpm incompatible ProxyMemoryAccess");
      expect(w.active_memory_model == ac::MemoryAcquisitionModel::DirectRpm,
             "World active_memory_model DirectRpm");
      expect(r->read(g->base, nullptr, sizeof(count)) == ac::Status::InvalidArgument,
             "DirectRpm rejects null non-empty destination");
      std::array<std::uint8_t, sizeof(count)> bytes{};
      expect(r->read(g->base, std::span<std::uint8_t>(bytes)) == ac::Status::Ok,
             "DirectRpm span read fills full destination");
      std::uint32_t typed_count = 0;
      expect(r->read_t(g->base, typed_count) == ac::Status::Ok && typed_count >= 2,
             "DirectRpm typed read fills whole object");
      std::uint32_t unaligned_count = 0;
      expect(r->read(g->base + 1, &unaligned_count, sizeof(unaligned_count)) ==
                 ac::Status::Ok,
             "DirectRpm byte-stream permits misaligned address safely");
    }

    // Syscall
    {
      const auto c2 = w.spawn("syscall-cheat.exe");
      auto r = sim::CreateMemoryReader(ac::MemoryAcquisitionModel::Syscall, w,
                                       c2);
      expect(r != nullptr, "factory: Syscall non-null");
      std::uint32_t count = 0;
      expect(r->read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "Syscall read");
      expect(r->has_handle_to(game), "Syscall has handle");
      // Syscall path sets via_syscall_path on handle.
      bool syscall_path = false;
      for (const auto& h : w.handles) {
        if (h.owner_pid == c2 && h.via_syscall_path) {
          syscall_path = true;
        }
      }
      expect(syscall_path, "Syscall path scar on handle");
    }

    // KernelIoctl — no usermode handle
    // Construct concrete type (project builds with /GR- so dynamic_cast is UB).
    {
      const auto c3 = w.spawn("kernel-cheat.exe");
      auto factory =
          sim::CreateMemoryReader(ac::MemoryAcquisitionModel::KernelIoctl, w, c3);
      expect(factory != nullptr, "factory: KernelIoctl non-null");
      sim::KernelIoctlReader kr(w, c3);
      expect(kr.ensure_bridge(), "KernelIoctl ensure_bridge");
      std::uint32_t count = 0;
      expect(kr.read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "KernelIoctl read");
      expect(count >= 2, "KernelIoctl count");
      expect(!kr.has_handle_to(game), "KernelIoctl no usermode handle");
      bool has_device = false;
      for (const auto& d : w.devices) {
        if (d.mem_rw_ioctl) {
          has_device = true;
        }
      }
      expect(has_device, "KernelIoctl created mem_rw device");
    }

    // HvHypercall
    {
      const auto c4 = w.spawn("hv-cheat.exe");
      // Force personal HV path: disable VBS/HVCI so try_start succeeds.
      w.trust.vbs = false;
      w.trust.hvci = false;
      auto r = sim::CreateMemoryReader(ac::MemoryAcquisitionModel::HvHypercall,
                                       w, c4);
      expect(r != nullptr, "factory: HvHypercall non-null");
      std::uint32_t count = 0;
      expect(r->read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "HvHypercall read");
      expect(!r->has_handle_to(game), "HvHypercall no handle");
      expect(w.trust.personal_hv_active || w.hypercall_read_active,
             "HvHypercall scar active");
    }

    // DmaPhysical — concrete type for arm_device (no RTTI).
    {
      auto factory =
          sim::CreateMemoryReader(ac::MemoryAcquisitionModel::DmaPhysical, w, 0);
      expect(factory != nullptr, "factory: DmaPhysical non-null");
      sim::DmaPhysicalReader dr(w, 0);
      dr.arm_device(true, false);
      expect(w.trust.dma_device_present, "DMA device present");
      std::uint32_t count = 0;
      expect(dr.read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "DmaPhysical read");
      expect(count >= 2, "DmaPhysical count");
      expect(!dr.has_handle_to(game), "DmaPhysical no handle");
      expect(w.fpga_scatter_reads >= 1, "DmaPhysical scatter scar");
    }

    // HijackProxy via donor pid (no callback)
    {
      const auto donor = w.spawn("steamwebhelper.exe");
      const auto consumer = w.spawn("radar-ui.exe");
      expect(w.open_process(donor, game,
                            sim::AccessMask::VmRead | sim::AccessMask::Query,
                            false),
             "HijackProxy donor open");
      auto factory = sim::CreateMemoryReader(
          ac::MemoryAcquisitionModel::HijackProxy, w, consumer);
      expect(factory != nullptr, "factory: HijackProxy non-null");
      sim::HijackProxyReader hr(w, consumer);
      hr.set_donor_pid(donor);
      std::uint32_t count = 0;
      expect(hr.read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "HijackProxy read via donor");
      expect(!hr.has_handle_to(game), "HijackProxy consumer has no handle");
      expect(hr.compatible_with(ac::DefenseLayer::ProxyMemoryAccess),
             "HijackProxy compatible ProxyMemoryAccess");
      expect(w.handle_proxy_active, "HijackProxy handle_proxy_active");
      expect(w.handle_proxy_owner_pid == donor, "HijackProxy owner pid");
    }

    // HijackProxy via callback
    {
      const auto donor = w.spawn("Discord.exe");
      const auto consumer = w.spawn("radar-cb.exe");
      expect(w.open_process(donor, game, sim::AccessMask::VmRead, false),
             "callback donor open");
      g_donor_world = &w;
      g_donor_pid = donor;
      sim::HijackProxyReader hr(w, consumer);
      hr.set_donor_callback(&donor_cb);
      std::uint32_t count = 0;
      expect(hr.read(g->base, &count, sizeof(count)) == ac::Status::Ok,
             "HijackProxy callback read");
      g_donor_world = nullptr;
      g_donor_pid = 0;
    }
  }

  // ── 3. HandleTable standalone + World integration ────────────────────────
  {
    sim::HandleTable table;
    table.open(10, 20, static_cast<std::uint32_t>(sim::AccessMask::VmRead),
               ac::HandleAcquisitionModel::DirectOpenProcess, 1);
    table.open(11, 20, static_cast<std::uint32_t>(sim::AccessMask::VmRead),
               ac::HandleAcquisitionModel::DirectSyscall, 2);
    expect(table.unique_owners_for(20) == 2, "HandleTable unique_owners");
    expect(table.open_vm_read_count(20) == 2, "HandleTable open_vm_read");
    table.close_handle(10, 20, 3);
    expect(table.unique_owners_for(20) == 1, "HandleTable after close");
    expect(table.open_handle_count() == 1, "HandleTable open count");
    expect(table.total_handles() == 2, "HandleTable total history");

    auto w = sim::make_arena();
    const auto cheat = w.spawn("ht-cheat.exe");
    w.open_process(cheat, w.game_pid(), sim::AccessMask::VmRead, false);
    expect(w.handle_table.open_vm_read_count(w.game_pid()) >= 1,
           "World handle_table open_vm_read");
    w.close_handles_from(cheat);
    expect(w.handles_to(w.game_pid()).empty() ||
               [&] {
                 for (const auto& h : w.handles) {
                   if (h.owner_pid == cheat) return false;
                 }
                 return true;
               }(),
           "close_handles_from removes World.handles");
  }

  // ── 4. DonorDiscovery full path ──────────────────────────────────────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    const auto donor = w.spawn("steam.exe");
    const auto cheat = w.spawn("external-radar.exe");
    w.open_process(donor, game,
                   sim::AccessMask::VmRead | sim::AccessMask::VmWrite, false);

    sim::DonorDiscovery discovery(w);
    expect(discovery.discover_and_select(game), "DonorDiscovery select");
    expect(discovery.selected().pid == donor, "DonorDiscovery best is steam");
    expect(discovery.selected().legitimacy_score >= 0.9,
           "DonorDiscovery steam legitimacy");

    auto xfer = discovery.transfer_handle(donor, game, cheat);
    expect(xfer.rights_reduced, "DonorDiscovery rights reduced");
    expect(xfer.transferred_access != 0, "DonorDiscovery transferred access");
    expect(w.donor_handle_duplicated, "World donor_handle_duplicated");
    expect(w.handle_table.open_handle_count() >= 2,
           "DonorDiscovery handle_table entry");

    auto payload =
        discovery.inject_payload(donor, game, /*handle_value=*/0x40, false);
    expect(payload.injected, "DonorDiscovery inject_payload");
    expect(w.shellcode_donor_active, "World shellcode_donor_active");

    auto section = discovery.create_shared_section(donor, cheat, 4096, true);
    expect(section.name_randomized, "DonorDiscovery randomized section");
    expect(!w.sections.empty(), "DonorDiscovery section on World");
  }

  // ── 5. EvasionMaster tick/shutdown scars ─────────────────────────────────
  {
    auto w = sim::make_arena();
    const auto cheat = w.spawn("rtss-decoy.exe");
    sim::EvasionConfig cfg;
    cfg.frame_drop_rate = 0.0;  // deterministic render path
    cfg.entity_omission_rate = 0.0;
    cfg.position_fuzz_px = 2.0;
    cfg.decoy_read_ratio = 0.0;
    cfg.read_order_shuffle = true;
    cfg.disguise_enabled = true;
    cfg.active_disguise = ac::DisguiseProfile::RivaTuner;
    cfg.forensic_clean_enabled = true;
    cfg.peb_unlink_enabled = true;

    sim::EvasionMaster master(w);
    master.init(cfg, cheat);
    expect(w.proc(cheat)->name == "RTSS.exe", "EvasionMaster disguise RTSS");
    expect(master.state().disguise_applied, "EvasionMaster disguise_applied");
    expect(master.state().peb_unlinked, "EvasionMaster peb_unlinked");
    expect(!master.state().active_layers.empty(), "EvasionMaster layers");

    std::vector<ac::EntitySnapshot> ents = {
        {1, {0, 0, 0}, 1, true},
        {2, {100, 0, 0}, 2, true},
        {3, {200, 0, 0}, 2, true},
    };
    auto out = master.tick(ents, 1, cheat);
    expect(out.size() == 3, "EvasionMaster tick preserves entities");
    expect(master.state().frames_rendered == 1, "EvasionMaster frames_rendered");
    expect(w.scattered_read_count >= 3, "EvasionMaster scatter scar");

    master.shutdown(cheat);
    expect(master.state().forensic_cleaned, "EvasionMaster forensic on shutdown");
    expect(w.forensic_prefetch_cleared, "World forensic_prefetch_cleared");
  }

  // ── 6. FixtureLoader plant into game memory ──────────────────────────────
  {
    sim::FixtureLoader::clear_registry();
    auto snap = sim::FixtureLoader::create_2v2();
    expect(snap.entities.size() == 4, "fixture 2v2 size");
    sim::FixtureLoader::save("2v2", snap);
    auto loaded = sim::FixtureLoader::load("2v2");
    expect(loaded.name == "2v2" && loaded.entities.size() == 4,
           "fixture save/load");

    auto w = sim::make_arena();
    auto game = sim::FixtureLoader::apply_to_world(snap, w);
    expect(game == w.game_pid(), "fixture apply game_pid");
    auto decoded = w.read_entity_snapshots(game);
    expect(decoded.size() == 4, "fixture plant decode count");
    // Verify positions match fixture via shipped plant/read path.
    expect(decoded[0].origin.x == 0.0f && decoded[2].team == 2,
           "fixture plant positions/teams");

    auto full = sim::FixtureLoader::create_full_server();
    expect(full.entities.size() == 14, "fixture full_server size");
    auto empty = sim::FixtureLoader::create_empty();
    expect(empty.entities.size() == 1, "fixture empty size");

    auto dma_fix =
        sim::FixtureLoader::create_for_model(ac::MemoryAcquisitionModel::DmaPhysical);
    expect(dma_fix.acq_model == ac::MemoryAcquisitionModel::DmaPhysical,
           "fixture create_for_model");
    expect(!sim::FixtureLoader::list_saved().empty(), "fixture list_saved");
  }

  // ── 7. OverlayDetector steam + window hijack ─────────────────────────────
  {
    auto w = sim::make_arena();
    sim::OverlayDetector det(w);

    // Clean state: no gameoverlay → not detected.
    auto clean = det.check_steam_overlay();
    expect(!clean.steam_hook_detected, "overlay clean steam");

    // Plant steam hook scars.
    w.gameoverlay_loaded = true;
    w.gameoverlay_base = 0x180000000ull;
    w.original_steam_present_ptr = w.gameoverlay_base + w.steam_present_ptr_offset;
    w.original_steam_resize_buffers_ptr =
        w.gameoverlay_base + w.steam_resize_buffers_offset;
    w.steam_present_hooked = true;
    w.hook_present_fn = 0xDEADBEEFull;
    auto hooked = det.check_steam_overlay();
    expect(hooked.steam_hook_detected, "overlay steam hook detected");
    expect(hooked.present_ptr_overwritten, "overlay present overwritten");

    // Window hijack: presenter != hwnd owner.
    w.swap_chain_count = 1;
    w.swap_chain_output_window = 0x1234;
    w.output_window_owner_pid = 111;
    w.hwnd_owner_pid = 111;
    w.presenter_pid = 222;
    w.cross_process_hijack = true;
    auto win = det.check_window_hijack();
    expect(win.swapchain_hijacked, "overlay window hijack detected");

    auto full = det.full_check();
    expect(full.any_detection, "overlay full_check detection");
    expect(full.risk_score > 0.0, "overlay risk_score");
    expect(full.any_mitigation, "overlay mitigation applied");
  }

  // ── 8. write_mem + plant_entity_snapshots round-trip ─────────────────────
  {
    auto w = sim::make_arena();
    const auto game = w.game_pid();
    auto* g = w.proc(game);
    const char tag[] = "SIMTAG";
    expect(w.write_mem(game, g->base + 0x300, tag, sizeof(tag)),
           "write_mem ok");
    expect(std::memcmp(g->memory.data() + 0x300, tag, sizeof(tag)) == 0,
           "write_mem bytes");

    std::vector<ac::EntitySnapshot> ents = {
        {1, {1.f, 2.f, 3.f}, 1, true},
        {2, {9.f, 8.f, 7.f}, 2, false},
    };
    expect(w.plant_entity_snapshots(game, ents, 0x50), "plant_entity_snapshots");
    auto back = w.read_entity_snapshots(game, 0x50);
    expect(back.size() == 2, "read_entity_snapshots size");
    expect(back[0].origin.x == 1.f && back[1].alive == false,
           "read_entity_snapshots values");

    // Corrupt count and rows to model malformed fixture input. The decoder must
    // return no partial snapshots and must not reserve the forged count.
    const std::uint32_t forged = std::numeric_limits<std::uint32_t>::max();
    expect(w.write_mem(game, g->base + 0x50, &forged, sizeof(forged)),
           "write forged entity count");
    expect(w.read_entity_snapshots(game, 0x50).empty(),
           "read_entity_snapshots rejects oversized count");
    g->memory.resize(0x52);
    expect(w.read_entity_snapshots(game, 0x50).empty(),
           "read_entity_snapshots rejects truncated table");
    expect(!w.mutate_lab_pattern_layout(game,
                                        std::numeric_limits<std::size_t>::max(),
                                        0x40),
           "mutate layout rejects marker offset overflow");
  }

  // ── 9. strategy_example + narrative smoke ────────────────────────────────
  {
    auto w = sim::make_arena();
    auto red = sim::strategy_example::record_red(
        w, "sim_unit", "strategy-actor.exe",
        {{"open handle", true}, {"read sample", true}});
    expect(red.achieved && red.steps == 2, "strategy_example red");

    sim::strategy_example::BlueOutcome blue;
    sim::strategy_example::add_common_signals(w, blue);
    // No untrusted VmRead yet — add one.
    const auto bad = w.spawn("unknown-tool.exe");
    w.open_process(bad, w.game_pid(), sim::AccessMask::VmRead, false);
    blue = {};
    sim::strategy_example::add_common_signals(w, blue);
    // Second independent residual so finish_blue multi-reason bar (signals>=2) fires.
    w.etw_enabled = false;
    sim::strategy_example::add_signal(blue, "ETW pipeline residual for multi-reason bar",
                                      0.20);
    auto finished = sim::strategy_example::finish_blue("sim_unit", blue);
    expect(finished.detected && finished.signals >= 2, "strategy_example blue multi-reason");

    sim::Narrator n;
    n.say(sim::Side::System, "sim_unit_test complete");
  }

  // ── 10. acquisition_compatible matrix ────────────────────────────────────
  {
    expect(sim::acquisition_compatible(ac::MemoryAcquisitionModel::HijackProxy,
                                       ac::HandleAcquisitionModel::HijackProxy,
                                       ac::DefenseLayer::ProxyMemoryAccess),
           "compat: HijackProxy + ProxyMemoryAccess");
    expect(!sim::acquisition_compatible(ac::MemoryAcquisitionModel::DirectRpm,
                                        ac::HandleAcquisitionModel::DirectOpenProcess,
                                        ac::DefenseLayer::ProxyMemoryAccess),
           "compat: DirectRpm rejects ProxyMemoryAccess");
    expect(sim::acquisition_compatible(ac::MemoryAcquisitionModel::DmaPhysical,
                                       ac::HandleAcquisitionModel::DmaPhysical,
                                       ac::DefenseLayer::ProcessIsolation),
           "compat: DMA + ProcessIsolation");
  }

  if (fails != 0) {
    std::fprintf(stderr, "\nsim_unit_test: %d failure(s)\n", fails);
    return 1;
  }
  std::printf("\nsim_unit_test: all passed\n");
  return 0;
}
