/// radar_t0.cpp — T0: OpenProcess + ReadProcessMemory live radar.
/// Works against real CS2. Falls back to simulation if CS2 not found.
#include "demos/radar_shared.hpp"
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"
#include "real/cs2/hijack_reader.hpp"
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif
#if defined(_WIN32)
// Console tool; force the console subsystem so the CRT uses main() instead
// of WinMain, regardless of any transitively-linked GUI libs.
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

#define TIER_NAME OBF("T0 - Usermode RPM")
#define TIER_TECH OBF("OpenProcess(PROCESS_VM_READ) + ReadProcessMemory")
#define TIER_SCAR OBF("VM_READ handle visible in system handle table")
#define TIER_DET  OBF("NtQuerySystemInformation handle with VM_READ to cs2.exe")
#define TIER_MIT  OBF("Protected Process Light (PPL), handle-auditing ETW")
#define TIER_CODE OBF("lib/real/cs2/memory.cpp :: Cs2MemoryReader(T0_UsermodeRpm)")

using Clock = std::chrono::steady_clock;

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    std::printf("=== %s ===\n", TIER_NAME);
    std::fflush(stdout);
    TierDoc doc{TIER_NAME, TIER_TECH, TIER_SCAR, TIER_DET, TIER_MIT, TIER_CODE};

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    std::printf("[0] Resolving API table...\n");
    auto& api = real::win::g_Api();
    if (!api.resolved) {
        std::fprintf(stderr, "FAIL: API table resolution failed\n");
        return 1;
    }
    std::printf("  OK\n");
#endif

    // ── Attach to real CS2 ───────────────────────────────────────
#if LR_HAS_REAL_PLATFORM
    auto attach = real::cs2::attach_to_cs2();
    bool live = attach.attached;
    std::printf("[%s]\n", live ? "REAL CS2 FOUND" : "SIMULATION (start cs2.exe for live)");
    if (!live) {
        std::printf("  attach error: %s\n", attach.error_msg.c_str());
    }

    real::cs2::Cs2MemoryReader reader;
    real::cs2::Cs2Offsets offsets{};
    if (live) {
        std::printf("  PID=%u  Base=0x%llx  ImageSize=%zu\n",
                    attach.pid,
                    (unsigned long long)attach.base_address,
                    attach.image_size);

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
        if (real::cs2::hijack::g_Hijack().setup(attach.pid)) {
            std::printf("  Hijack OK (donor=%u)\n",
                        real::cs2::hijack::g_Hijack().donor_pid());
        } else {
            std::printf("  Hijack failed — educational OpenProcess RPM\n");
        }
#endif

        auto o = real::cs2::resolve_offsets_for_process(
            attach.pid, attach.handle, attach.base_address, attach.image_size);
        if (o) {
            offsets = *o;
            std::printf("  Offsets complete=%d\n  %s\n",
                        offsets.is_complete() ? 1 : 0,
                        offsets.describe().c_str());
        } else {
            std::printf("  Offset resolve failed: %s\n", o.error_msg.c_str());
        }

        auto st = reader.attach(ac::Tier::T0_UsermodeRpm, attach.pid);
        std::printf("  Backend: %s (attach=%s)\n",
                    reader.active_backend_name(),
                    st == ac::Status::Ok ? "ok" : "fail");
    }
#else
    constexpr bool live = false;
    std::printf("[SIMULATION (real backends disabled)]\n");
#endif

    // ── Create floating always-on-top D3D11 overlay ──────────────
    real::gpu::RenderPipeline* rp = nullptr;
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    {
        real::gpu::OverlayStyle style = default_radar_overlay_style();
        auto created = real::gpu::create_render_pipeline(style, TIER_NAME);
        if (created) {
            rp = *created;
            std::printf("  Overlay: %dx%d alpha=%u topmost clickthrough\n",
                        style.width, style.height,
                        static_cast<unsigned>(style.window_alpha));
        } else {
            std::printf("  Overlay create failed: %s\n", created.error_msg.c_str());
        }
    }
#endif
    // ── Main loop (finite when SIM so lab/CI demos exit cleanly) ──
    const int max_frames = radar_demo_max_frames(live);
    int window_frames = 0;
    int total_frames = 0;
    int last_ents = 0, last_blips = 0;
    auto report_started = Clock::now();
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    HWND cs2_hwnd = nullptr;
    cs2_hwnd = FindWindowA(OBF("Valve001"), nullptr);
#endif

    if (!rp) {
        std::printf("  No GPU overlay — console/sim entity path\n");
    }

    while (!rp || rp->process_messages()) {
        auto frame_started = Clock::now();

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
        if (live && cs2_hwnd && GetForegroundWindow() != cs2_hwnd) {
            real::win::fuzzed_sleep(16, 10);
            if (max_frames > 0 && ++total_frames >= max_frames) break;
            continue;
        }
#endif

        std::vector<PlayerEntity> ents;
#if LR_HAS_REAL_PLATFORM
        if (live && offsets.is_complete() && reader.is_attached()) {
            auto entities = real::cs2::read_entity_list(reader, offsets, attach.pid);
            auto local = real::cs2::read_local_player(reader, offsets);

            if (!(local && !local->is_alive)) {
                if (entities.read_successful) {
                    for (auto& e : entities.entities) {
                        bool is_local = (local && e.controller_handle == local->controller_handle);
                        ents.push_back({e.controller_handle, e.origin, e.eye_angles,
                                        static_cast<uint8_t>(e.team), e.health, e.is_alive, is_local});
                    }
                    std::printf("\r[T0 RPM] %d entities read", entities.entity_count);
                    std::fflush(stdout);
                }
            }
        }
#endif
        if (ents.empty()) ents = make_sim_entities(2);

        ac::Vec3 local_origin{}; float local_yaw = 0;
        for (auto& e : ents) {
            if (e.is_local) { local_origin = e.origin; local_yaw = e.angles.y; }
        }
        auto blips = project_to_radar(ents, local_origin, local_yaw);
        auto us = std::chrono::duration<double, std::micro>(Clock::now() - frame_started).count();
        render_radar(rp, ents, blips, doc, us);
        last_ents = static_cast<int>(ents.size());
        last_blips = static_cast<int>(blips.size());

        ++window_frames;
        ++total_frames;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - report_started).count();
        if (elapsed >= 1000) {
            std::printf("\r[T0] FPS: %.1f | entities: %d | %s   ",
                        window_frames / (elapsed / 1000.0),
                        last_ents, live ? "LIVE" : "SIM");
            fflush(stdout);
            window_frames = 0;
            report_started = Clock::now();
        }

        if (max_frames > 0 && total_frames >= max_frames) break;
#if LR_HAS_REAL_PLATFORM
        real::win::fuzzed_sleep(14, 15);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(14));
#endif
    }

    if (last_ents == 0) {
        auto ents = make_sim_entities(2);
        ac::Vec3 local_origin{}; float local_yaw = 0;
        for (auto& e : ents) {
            if (e.is_local) { local_origin = e.origin; local_yaw = e.angles.y; }
        }
        auto blips = project_to_radar(ents, local_origin, local_yaw);
        last_ents = static_cast<int>(ents.size());
        last_blips = static_cast<int>(blips.size());
        render_console(doc, ents, blips, 0.0);
    }
    std::printf("\n");
    print_tier_surface(doc, last_ents, last_blips, live);

#if LR_HAS_REAL_PLATFORM
    if (live) {
        reader.detach();
        real::cs2::detach_from_cs2(attach.handle);
    }
#endif
    if (rp) { rp->shutdown(); delete rp; }
    std::printf("[T0] Complete.\n");
    return 0;
}
