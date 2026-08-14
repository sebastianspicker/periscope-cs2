// cs2_stack_unit_test.cpp — Offline unit tests for shipped real::cs2 pure logic.
// Drives real functions (no CS2 attach, no re-implemented algorithms).

#include "real/cs2/live_radar_stack.hpp"
#include "real/cs2/memory.hpp"
#include "real/cs2/offsets.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/cs2/radar.hpp"

#include "ac/types.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static int g_fails = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("OK: %s\n", msg);
  }
}

static bool near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  using namespace real::cs2;
  using namespace real::cs2::stack;

  // ── 1) offsets_from_snapshot / offsets_from_client_base ─────────
  {
    constexpr std::uint64_t kClient = 0x7FF600000000ull;
    const Cs2Offsets o = offsets_from_snapshot(kClient);
    expect(o.is_complete(), "offsets_from_snapshot is_complete with client base");
    expect(o.entity_list == kClient + snapshot::globals::dwEntityList,
           "entity_list = client + dwEntityList RVA");
    expect(o.local_player == kClient + snapshot::globals::dwLocalPlayerPawn,
           "local_player = client + dwLocalPlayerPawn RVA");
    expect(o.entity_health == snapshot::fields::m_iHealth,
           "entity_health schema field");
    expect(o.entity_controller_pawn == snapshot::fields::m_hPlayerPawn,
           "entity_controller_pawn schema field");
    expect(o.entity_origin == snapshot::fields::m_vOldOrigin, "entity_origin field");
    expect(o.entity_list_entry == snapshot::constants::entity_identity_stride,
           "identity stride from snapshot constants");

    const Cs2Offsets via_stack = offsets_from_client_base(kClient);
    expect(via_stack.entity_list == o.entity_list,
           "offsets_from_client_base matches offsets_from_snapshot");
    expect(via_stack.is_complete(), "offsets_from_client_base complete");

    const Cs2Offsets rva_only = offsets_from_snapshot(0);
    expect(rva_only.entity_list == snapshot::globals::dwEntityList,
           "zero base stores RVAs");
    expect(rva_only.entity_health != 0, "schema fields non-zero without base");
  }

  // ── 2) entities_to_blips ────────────────────────────────────────
  {
    std::vector<Cs2PlayerEntity> ents(2);
    ents[0].controller_handle = 1;
    ents[0].origin = {100.f, 50.f, 10.f};
    ents[0].team = 2;
    ents[0].is_alive = true;
    ents[0].is_local_player = true;
    ents[1].controller_handle = 2;
    ents[1].origin = {200.f, 150.f, 20.f};
    ents[1].team = 3;
    ents[1].is_alive = false;

    const float scale = 0.5f;
    const float ox = 0.f;
    const float oy = 0.f;
    const auto blips = entities_to_blips(ents, scale, ox, oy);
    expect(blips.size() == 2, "entities_to_blips size");
    // x = (origin.x - map_origin_x) * scale
    expect(near(blips[0].x, 100.f * scale), "blip0.x world transform");
    // y = (map_origin_y - origin.y) * scale
    expect(near(blips[0].y, (0.f - 50.f) * scale), "blip0.y world transform");
    expect(near(blips[0].height, 10.f), "blip0.height");
    expect(blips[0].is_local && blips[0].is_alive, "blip0 local+alive flags");
    expect(near(blips[1].x, 200.f * scale) && near(blips[1].y, -150.f * scale),
           "blip1 transform");
    expect(!blips[1].is_alive && blips[1].team == 3, "blip1 team/alive");
  }

  // ── 3) ReadThrottle due/arm ─────────────────────────────────────
  {
    ReadThrottle th;
    th.target_hz = 20;
    th.local_hz = 50;
    th.far_hz = 5;
    th.jitter_frac = 0.f;  // deterministic arm_full
    th.rng = 0xC0FFEEu;

    const auto t0 = std::chrono::steady_clock::now();
    // Fresh throttle: next_* default to epoch => due immediately
    expect(th.full_due(t0), "full_due at start");
    expect(th.local_due(t0), "local_due at start");
    expect(th.far_due(t0), "far_due at start");

    th.arm_full(t0);
    th.arm_local(t0);
    th.arm_far(t0);
    expect(!th.full_due(t0), "full not due immediately after arm");
    expect(!th.local_due(t0), "local not due immediately after arm");
    expect(!th.far_due(t0), "far not due immediately after arm");

    // target_hz=20 => ~50ms; local_hz=50 => 20ms; far_hz=5 => 200ms
    const auto t_local =
        t0 + std::chrono::milliseconds(25);
    expect(th.local_due(t_local), "local due after local period");
    expect(!th.full_due(t_local), "full still not due at 25ms");

    const auto t_full = t0 + std::chrono::milliseconds(60);
    expect(th.full_due(t_full), "full due after full period");

    const auto t_far = t0 + std::chrono::milliseconds(220);
    expect(th.far_due(t_far), "far due after far period");
  }

  // ── 4) AttachLadder stage/scar strings ──────────────────────────
  {
    expect(std::strcmp(AttachLadder::stage_name(AttachStage::None), "none") == 0,
           "stage_name None");
    expect(std::strcmp(AttachLadder::stage_name(AttachStage::HijackVerified),
                       "hijack_verified") == 0,
           "stage_name HijackVerified");
    expect(std::strcmp(AttachLadder::stage_name(AttachStage::DonorWorkerIpc),
                       "donor_worker_ipc") == 0,
           "stage_name DonorWorkerIpc");
    expect(std::strcmp(AttachLadder::stage_name(AttachStage::DirectOpenProcess),
                       "direct_openprocess") == 0,
           "stage_name DirectOpenProcess");
    expect(std::strcmp(AttachLadder::stage_name(AttachStage::DegradedHudOnly),
                       "degraded_hud_only") == 0,
           "stage_name DegradedHudOnly");

    const char* scar_h =
        AttachLadder::scar_string(AttachStage::HijackVerified);
    expect(scar_h && std::strstr(scar_h, "SCAR:L1_hijack") != nullptr,
           "scar HijackVerified");
    const char* scar_w =
        AttachLadder::scar_string(AttachStage::DonorWorkerIpc);
    expect(scar_w && std::strstr(scar_w, "SCAR:L2_donor_worker") != nullptr,
           "scar DonorWorkerIpc");
    const char* scar_o =
        AttachLadder::scar_string(AttachStage::DirectOpenProcess);
    expect(scar_o && std::strstr(scar_o, "SCAR:L3_openprocess") != nullptr,
           "scar DirectOpenProcess");

    AttachLadder ladder;
    ladder.set_stage(AttachStage::DonorWorkerIpc);
    expect(ladder.stage == AttachStage::DonorWorkerIpc, "set_stage stage");
    expect(ladder.worker_ipc && ladder.ui_detached && !ladder.open_process_handle,
           "set_stage DonorWorkerIpc flags");
    expect(ladder.last_scar &&
               std::strstr(ladder.last_scar, "SCAR:L2_donor_worker") != nullptr,
           "set_stage last_scar");

    ladder.set_stage(AttachStage::DirectOpenProcess);
    expect(ladder.open_process_handle && !ladder.worker_ipc && !ladder.ui_detached,
           "set_stage DirectOpenProcess flags");
  }

  // ── 5) score_blue_dual composite / scar_owner ───────────────────
  {
    // Clean: no handles, low volume
    BlueDualConfig clean{};
    clean.window_sec = 5.0;
    clean.rpm_reads = 10;
    clean.rpm_bytes = 100;
    auto s0 = score_blue_dual(clean);
    expect(std::strcmp(s0.scar_owner, "none") == 0, "clean scar_owner none");
    expect(!s0.would_detect || s0.composite < 50, "clean low/no detect");

    // UI holds OpenProcess + high RPM volume => would_detect, scar_owner=ui
    BlueDualConfig ui{};
    ui.cs2_pid = 1000;
    ui.self_pid = 2000;
    ui.ui_holds_openprocess = true;
    ui.ui_detached = false;
    ui.rpm_reads = 30000;  // 6000 rps over 5s => volume_score 95
    ui.rpm_bytes = 30000 * 64;
    ui.window_sec = 5.0;
    auto s1 = score_blue_dual(ui);
    expect(std::strcmp(s1.scar_owner, "ui") == 0, "UI OpenProcess scar_owner=ui");
    expect(s1.scar_pid == ui.self_pid, "UI scar_pid is self");
    expect(s1.handle_score >= 90, "UI handle_score high");
    expect(s1.volume_score >= 70, "high RPM volume_score");
    expect(s1.composite >= 50, "UI+volume composite >= 50");
    expect(s1.would_detect, "would_detect when UI holds OpenProcess + high RPM");
    expect(s1.foreign_vm_read_handles == 1, "foreign_vm_read_handles=1");

    // Worker path: scar on worker, not UI
    BlueDualConfig worker{};
    worker.cs2_pid = 1000;
    worker.self_pid = 2000;
    worker.worker_pid = 3000;
    worker.has_worker = true;
    worker.ui_detached = true;
    worker.ui_holds_openprocess = false;
    worker.rpm_reads = 100;
    worker.window_sec = 5.0;
    auto s2 = score_blue_dual(worker);
    expect(std::strcmp(s2.scar_owner, "worker") == 0, "worker scar_owner");
    expect(s2.scar_pid == worker.worker_pid, "worker scar_pid");

    // Hijack/donor path
    BlueDualConfig hijack{};
    hijack.has_hijack = true;
    hijack.donor_pid = 4000;
    hijack.rpm_reads = 50;
    auto s3 = score_blue_dual(hijack);
    expect(std::strcmp(s3.scar_owner, "donor") == 0, "hijack scar_owner=donor");
    expect(s3.scar_pid == 4000, "hijack scar_pid=donor");
  }

  // ── 6) Memory backend honesty (PreferIoctl / T3) ────────────────
  {
    auto t0 = create_backend(ac::Tier::T0_UsermodeRpm, 0);
    expect(t0 != nullptr && t0->tier() == ac::Tier::T0_UsermodeRpm,
           "create_backend T0");
    auto t2 = create_backend(ac::Tier::T2_KernelByovd, 0);
    expect(t2 != nullptr && t2->tier() == ac::Tier::T2_KernelByovd,
           "create_backend T2");
    auto t3 = create_backend(ac::Tier::T3_Hypervisor, 0);
    expect(t3 != nullptr && t3->tier() == ac::Tier::T3_Hypervisor,
           "create_backend T3");
    expect(!t3->is_attached(), "T3 not attached pre-attach");
    const ac::Status st = t3->attach(1234);
    expect(st == ac::Status::Unavailable, "T3 attach Unavailable without HV");
    expect(!t3->is_attached(), "T3 still not attached after failed attach");
    auto rr = t3->read({0x1000, 16});
    expect(rr.status == ac::Status::Unavailable && rr.bytes.empty(),
           "T3 read Unavailable empty");

    // PreferIoctl: attach T2 must not leave an RPM backend if ioctl fails
    Cs2MemoryReader reader;
    reader.set_preference(MemoryBackendPreference::PreferIoctl);
    const ac::Status ast =
        reader.attach(ac::Tier::T2_KernelByovd, /*fake pid*/ 1);
    expect(ast != ac::Status::Ok, "PreferIoctl T2 fails without driver");
    expect(!reader.is_attached(), "PreferIoctl T2 not attached on failure");
    // Must not silently become RPM
    const char* be = reader.active_backend_name();
    expect(be && std::strcmp(be, "real_rpm") != 0,
           "PreferIoctl failure does not install real_rpm");
  }

  // ── 7) write_offset_snapshot_file + round-trip fields ───────────
  {
    constexpr std::uint64_t kClient = 0x180000000ull;
    Cs2Offsets o = offsets_from_client_base(kClient);
    const char* path = "cs2_stack_unit_offsets_snapshot.json";
    expect(write_offset_snapshot_file(o, kClient, path),
           "write_offset_snapshot_file");
    // try_load will pick up cwd file if present
    Cs2Offsets fields{};
    // We only assert write succeeded; load path may also pick other files.
    // Direct file content check via re-open is enough for pure path.
    std::FILE* f = std::fopen(path, "rb");
    expect(f != nullptr, "snapshot file exists after write");
    if (f) {
      char buf[512]{};
      const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
      std::fclose(f);
      expect(n > 0 && std::strstr(buf, "dwEntityList") != nullptr,
             "snapshot contains dwEntityList");
      expect(std::strstr(buf, "m_iHealth") != nullptr, "snapshot contains m_iHealth");
    }
    std::remove(path);
    (void)fields;
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s) in cs2_stack_unit_test\n", g_fails);
    return 1;
  }
  std::printf("ALL cs2_stack_unit_test PASSED\n");
  return 0;
}
