/// radar_t1.cpp — T1: Direct syscall NtReadVirtualMemory live radar.
#include "demos/radar_shared.hpp"
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif
#if defined(_WIN32)
// Console tool; force the console subsystem so the CRT uses main() instead
// of WinMain, regardless of any transitively-linked GUI libs.
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

#define TIER_NAME OBF("T1 — Direct Syscall")
#define TIER_TECH OBF("NtReadVirtualMemory via syscall instruction")
#define TIER_SCAR OBF("VM_READ handle + syscall instruction")
#define TIER_DET  OBF("Handle enumeration + ETW TI syscall monitoring")
#define TIER_MIT  OBF("ETW TI, kernel callbacks")
#define TIER_CODE OBF("lib/real/win/syscall_helper.cpp :: syscall_4()")

using Clock = std::chrono::steady_clock;

int main() {
    std::printf("=== %s ===\n", TIER_NAME);
    TierDoc doc{TIER_NAME, TIER_TECH, TIER_SCAR, TIER_DET, TIER_MIT, TIER_CODE};

    // T1 uses the same OpenProcess handle but reads via direct syscall.
    // The handle is still visible (same limitation as T0), but the RPM
    // goes through NtReadVirtualMemory directly, bypassing any ntdll hooks.
#if LR_HAS_REAL_PLATFORM
    auto attach = real::cs2::attach_to_cs2();
    bool live = attach.attached;
    std::printf("[%s]\n", live ? "REAL CS2 FOUND (T1 syscall path)" : "SIMULATION (start cs2.exe for live)");

    real::cs2::Cs2MemoryReader reader;
    real::cs2::Cs2Offsets offsets{};
    if (live) {
        std::printf("  PID=%u  Base=0x%llx\n", attach.pid, (unsigned long long)attach.base_address);
        auto o = real::cs2::resolve_offsets(attach.pid, attach.base_address, attach.image_size);
        if (o) { offsets = *o; reader.attach(ac::Tier::T1_SyscallSoft, attach.pid); }
        std::printf("  Backend: %s\n", reader.active_backend_name());
    }
#else
    constexpr bool live = false;
    std::printf("[SIMULATION (real backends disabled)]\n");
#endif

    real::gpu::RenderPipeline* rp = nullptr;
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    {
        auto created = real::gpu::create_render_pipeline(
            default_radar_overlay_style(), TIER_NAME);
        if (created) rp = *created;
    }
#endif

    const int max_frames = radar_demo_max_frames(live);
    int window_frames = 0;
    int total_frames = 0;
    int last_ents = 0, last_blips = 0;
    auto report_started = Clock::now();
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    HWND cs2_hwnd = nullptr;
    cs2_hwnd = FindWindowA(OBF("Valve001"), nullptr);
#endif

    while (!rp || rp->process_messages()) {
        do {  // MSVC: allow early break without crossing inits
        auto frame_started = Clock::now();
        std::vector<PlayerEntity> ents;

        // V5-B2: Skip reads if game window not focused (live only)
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
        if (live && cs2_hwnd && GetForegroundWindow() != cs2_hwnd) {
            break;
        }
#endif

#if LR_HAS_REAL_PLATFORM
        if (live && offsets.is_complete()) {
            auto entities = real::cs2::read_entity_list(reader, offsets, attach.pid);
            auto local = real::cs2::read_local_player(reader, offsets);

            // V5-B1: Skip if local player is dead or spectating
            if (local && !local->is_alive) {
                break;
            }

            if (entities.read_successful) {
                for (auto& e : entities.entities) {
                    bool is_local = (local && e.controller_handle == local->controller_handle);
                    ents.push_back({e.controller_handle, e.origin, e.eye_angles,
                                    static_cast<uint8_t>(e.team), e.health, e.is_alive, is_local});
                }
            }
        }
#endif
        if (ents.empty()) ents = make_sim_entities(2);

        ac::Vec3 local_origin{}; float local_yaw = 0;
        for (auto& e : ents) { if (e.is_local) { local_origin = e.origin; local_yaw = e.angles.y; } }
        auto blips = project_to_radar(ents, local_origin, local_yaw);
        auto us = std::chrono::duration<double, std::micro>(Clock::now() - frame_started).count();
        render_radar(rp, ents, blips, doc, us);
        last_ents = static_cast<int>(ents.size());
        last_blips = static_cast<int>(blips.size());

        ++window_frames;
        ++total_frames;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - report_started).count();
        if (elapsed >= 1000) {
            std::printf("\r[T1] FPS: %.1f | entities: %d | %s   ", window_frames / (elapsed / 1000.0),
                        last_ents, live ? "LIVE (direct syscall)" : "SIM");
            fflush(stdout); window_frames = 0; report_started = Clock::now();
        }
} while (0);
        if (!rp) break;
        if (max_frames > 0 && total_frames >= max_frames) break;
        static uint64_t _rng = static_cast<uint64_t>(Clock::now().time_since_epoch().count());
        _rng ^= _rng << 13; _rng ^= _rng >> 7; _rng ^= _rng << 17;
        int32_t delay_ms = 14 + static_cast<int32_t>(_rng % 7);
#if LR_HAS_REAL_PLATFORM
        real::win::fuzzed_sleep(delay_ms);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
#endif
    }

    if (last_ents == 0) {
        auto ents = make_sim_entities(2);
        ac::Vec3 o{}; float y = 0;
        for (auto& e : ents) if (e.is_local) { o = e.origin; y = e.angles.y; }
        auto blips = project_to_radar(ents, o, y);
        last_ents = (int)ents.size(); last_blips = (int)blips.size();
        render_console(doc, ents, blips, 0.0);
    }
    std::printf("\n");
    print_tier_surface(doc, last_ents, last_blips, live);
#if LR_HAS_REAL_PLATFORM
    if (live) { reader.detach(); real::cs2::detach_from_cs2(attach.handle); }
#endif
    if (rp) { rp->shutdown(); delete rp; }
    std::printf("[T1] Complete.\n");
    return 0;
}
