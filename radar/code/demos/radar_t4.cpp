/// radar_t4.cpp — T4: PCIe FPGA DMA radar (theoretical live mode).
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

#define TIER_NAME "T4 — PCIe FPGA DMA"
#define TIER_TECH "FPGA reads physical memory via PCIe read TLP — zero OS involvement"
#define TIER_SCAR "PCIe read TLP on the bus — visible to IOMMU only"
#define TIER_DET  "VT-d/AMD-Vi DMA remapping blocks DMA to unmapped pages"
#define TIER_MIT  "Enable VT-d/AMD-Vi with DMA remapping in firmware + IOMMU driver in OS"
#define TIER_CODE "lib/real/dma/{core,fpga,backend}.cpp :: RealDmaBackend::read()"

using Clock = std::chrono::steady_clock;

int main() {
    std::printf("=== %s ===\n", TIER_NAME);
    TierDoc doc{TIER_NAME, TIER_TECH, TIER_SCAR, TIER_DET, TIER_MIT, TIER_CODE};

    // T4 reads via physical memory: no OS involvement at any level.
    // THEORETICAL LIVE MODE: Requires FPGA hardware (PCILeech).
    //   PCILeech 2.0 B1: Xilinx Artix-7 on PCIe gen2 x4
    //   FPGA performs PCIe memory read TLPs to CS2's physical memory pages
    //   Result: NO handles, NO syscalls, NO IOCTLs, NO kernel involvement
    //
    // PCIe DMA descriptor:
    //   src_addr = physical address of cs2.exe entity list
    //   size     = 4096 (one page)
    //   flags    = READ | CHAIN
    //
    // FIRMWARE: firmware/example_pcie_dma/{fpga_dma.h,host_sim.cpp}
    //   PCILeech 2.0 B1 firmware register map
    //   BAR0 128KB non-prefetchable

    // Show PCIe and IOMMU state (requires -DLR_ENABLE_REAL_DMA=ON)
#if defined(LR_HAS_REAL_DMA)
    auto pcie_devs = real::dma::enum_pci_devices();
    std::printf("  PCIe devices: %zu found\n", pcie_devs ? pcie_devs->size() : 0);
    auto iommu = real::dma::iommu_enabled();
    std::printf("  IOMMU: %s\n", iommu ? "ACTIVE (DMA blocked)" : "OFF (DMA possible)");
#else
    std::printf("  DMA backend not built (enable -DLR_ENABLE_REAL_DMA=ON)\n");
    std::printf("  IOMMU: (unknown without DMA backend)\n");
#endif
    std::printf("  T4 live path requires PCILeech FPGA hardware\n");

#if LR_HAS_REAL_PLATFORM
    auto attach = real::cs2::attach_to_cs2();
    bool live = attach.attached;
    std::printf("[%s]\n", live ? "CS2 FOUND — would use DMA path" : "SIMULATION");

    real::cs2::Cs2MemoryReader reader;
    real::cs2::Cs2Offsets offsets{};
    if (live) {
        auto o = real::cs2::resolve_offsets(attach.pid, attach.base_address, attach.image_size);
        if (o) { offsets = *o; reader.attach(ac::Tier::T0_UsermodeRpm, attach.pid); }
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

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    HWND cs2_hwnd_t4 = nullptr;
    cs2_hwnd_t4 = FindWindowA(OBF("Valve001"), nullptr);
#endif

    const int max_frames = radar_demo_max_frames(live);
    int window_frames = 0;
    int total_frames = 0;
    int last_ents = 0, last_blips = 0;
    auto report_started = Clock::now();
    while (!rp || rp->process_messages()) {
        do {  // MSVC: allow early break without crossing inits
        auto frame_started = Clock::now();
        std::vector<PlayerEntity> ents;

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
        if (live && cs2_hwnd_t4 && GetForegroundWindow() != cs2_hwnd_t4) {
            break;
        }
#endif

#if LR_HAS_REAL_PLATFORM
        if (live && offsets.is_complete()) {
            auto entities = real::cs2::read_entity_list(reader, offsets, attach.pid);
            auto local = real::cs2::read_local_player(reader, offsets);
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
            std::printf("\r[T4] FPS: %.1f | entities: %d | %s   ",
                        window_frames / (elapsed / 1000.0), last_ents, live ? "LIVE" : "SIM");
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
    std::printf("[T4] Complete.\n");
    return 0;
}
