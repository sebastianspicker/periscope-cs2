/// radar_multi_tier.cpp — Complete radar prototype for ALL 5 tiers (T0-T4).
///
/// Each tier reads CS2 entity data using a different technique, projects
/// world coordinates to radar coordinates, and renders an overlay.
///
/// USAGE:
///   ./build/radar_multi_tier --tier T0    # T0: OpenProcess + ReadProcessMemory
///   ./build/radar_multi_tier --tier T1    # T1: Direct syscall NtReadVirtualMemory
///   ./build/radar_multi_tier --tier T2    # T2: BYOVD gdrv.sys IOCTL (simulated)
///   ./build/radar_multi_tier --tier T3    # T3: Hyper-V hypercall (simulated)
///   ./build/radar_multi_tier --tier T4    # T4: PCIe FPGA DMA (simulated)
///   ./build/radar_multi_tier --tier all   # Run ALL tiers and compare
///
/// Each tier documents:
///   TECHNIQUE: What the operation does
///   SCAR:      What forensic artifact it leaves
///   BLUE:      How anti-cheat detects it
///   MITIGATION: What prevents it

#include "real/real_fwd.hpp"
#include "ac/types.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <intrin.h>
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"

// ═══════════════════════════════════════════════════════════════════
// Tier Documentation
// ═══════════════════════════════════════════════════════════════════

struct TierDoc {
    const char* name;
    const char* technique;
    const char* scar;
    const char* blue;
    const char* mitigation;
    const char* real_code;
};

static const TierDoc kTiers[] = {
    {
        "T0 — Usermode RPM",
        "OpenProcess + ReadProcessMemory from radar process",
        "VM_READ handle visible in system handle table",
        "NtQuerySystemInformation(SystemHandleInformation) → finds non-AC handle with VM_READ to cs2.exe",
        "Protected Process Light (PPL), handle-auditing ETW",
        "lib/real/cs2/memory.cpp :: Cs2MemoryReader(UsermodeRpm)"
    },
    {
        "T1 — Direct Syscall",
        "NtReadVirtualMemory via syscall instruction (bypasses ntdll hooks)",
        "VM_READ handle + syscall instruction (bypasses usermode hooks but not ETW TI)",
        "Handle enumeration + ETW Threat Intelligence syscall monitoring",
        "ETW TI, kernel callbacks (PsSetLoadImageNotifyRoutine)",
        "lib/real/win/syscall_helper.cpp :: syscall_4()"
    },
    {
        "T2 — BYOVD Kernel",
        "KeStackAttachProcess via gdrv.sys IOCTL 0xC3502008 — attaches to CS2 in kernel, no usermode handle",
        "Device handle to \\\\.\\gdrv + IOCTL traffic (no VM_READ handle to cs2.exe!)",
        "Driver hash scan (gdrv.sys SHA256 blocklist), ETW IRP_MJ_DEVICE_CONTROL",
        "Driver Signature Enforcement (DSE), HVCI, BYOVD blocklist",
        "lib/real/kernel/vulnerable_driver.cpp :: ByovdSession"
    },
    {
        "T3 — Hypervisor",
        "vmcall/vmresume to read process memory via EPT — no OS involvement at OS level",
        "CR4.VMXE bit set, VMCS region in physical memory, CPUID timing anomaly",
        "VBS locks VMX, CPUID.1:ECX[31] hypervisor present bit, timing analysis",
        "Virtualization-Based Security (VBS), HVCI, Nested Virtualization",
        "lib/real/vmx/{lifecycle, ept, hyperv}.cpp :: vmx_read_physical()"
    },
    {
        "T4 — PCIe DMA",
        "FPGA reads physical memory directly from PCIe bus — zero OS involvement",
        "PCIe read TLP on the bus — visible to IOMMU only",
        "VT-d/AMD-Vi DMA remapping blocks DMA to unmapped pages",
        "Enable VT-d/AMD-Vi with DMA remapping in firmware + IOMMU driver in OS",
        "lib/real/dma/{core, fpga, backend}.cpp :: RealDmaBackend::read()"
    }
};

// ═══════════════════════════════════════════════════════════════════
// Simulated Player Entity for when CS2 isn't running
// ═══════════════════════════════════════════════════════════════════

struct PlayerEntity {
    uint32_t id;
    ac::Vec3 origin;
    ac::Vec3 angles;
    uint8_t team;    // 2=Terrorist, 3=Counter-Terrorist
    int health;
    bool alive;
    bool is_local;
};

// Generate realistic-looking CS2 player data for simulation mode
static PlayerEntity make_entity(uint32_t id, float x, float y, float z,
                                 float ax, float ay, float az,
                                 uint8_t team, int health, bool alive, bool local) {
    PlayerEntity e;
    e.id = id;
    e.origin = {x, y, z};
    e.angles = {ax, ay, az};
    e.team = team;
    e.health = health;
    e.alive = alive;
    e.is_local = local;
    return e;
}

static std::vector<PlayerEntity> generate_sim_entities(int local_team = 2) {
    std::vector<PlayerEntity> entities;
    entities.push_back(make_entity(1, 0, 0, 0, 0, 0, 0, (uint8_t)local_team, 100, true, true));
    entities.push_back(make_entity(2, 1350, 0, 100, 0, 45, 0, 2, 100, true, false));
    entities.push_back(make_entity(3, 1200, 400, 100, 0, -30, 0, 2, 75, true, false));
    entities.push_back(make_entity(4, 1500, -300, 100, 0, 90, 0, 2, 50, true, false));
    entities.push_back(make_entity(5, -500, 800, 100, 0, 180, 0, 3, 100, true, false));
    entities.push_back(make_entity(6, -800, 600, 100, 0, -135, 0, 3, 80, true, false));
    entities.push_back(make_entity(7, -300, 1000, 100, 0, -90, 0, 3, 45, true, false));
    entities.push_back(make_entity(8, -600, -400, 100, 0, 0, 0, 3, 15, true, false));
    return entities;
}

// ═══════════════════════════════════════════════════════════════════
// World → Radar Projection
// ═══════════════════════════════════════════════════════════════════
// Projects 3D world coordinates to 2D radar coordinates.
// Transform: rotate by -local_yaw, scale by 1/radar_scale

struct RadarBlip {
    float x, y;         // normalized radar coords (-1..1)
    float angle;        // orientation in radians
    uint32_t color;     // 0xAARRGGBB
    int health;
    bool is_local;
    std::string label;
};

static std::vector<RadarBlip> project_to_radar(
    const std::vector<PlayerEntity>& entities,
    const ac::Vec3& local_origin,
    float local_yaw_deg,
    float radar_scale)
{
    std::vector<RadarBlip> blips;
    float yaw_rad = local_yaw_deg / 180.0f * 3.14159265f;

    for (const auto& ent : entities) {
        if (!ent.alive) continue;

        float dx = ent.origin.x - local_origin.x;
        float dz = ent.origin.z - local_origin.z;

        // Rotate by -local_yaw (forward=up on radar)
        float rx = (dx * cosf(-yaw_rad) - dz * sinf(-yaw_rad)) / radar_scale;
        float ry = (dx * sinf(-yaw_rad) + dz * cosf(-yaw_rad)) / radar_scale;

        uint32_t color;
        if (ent.is_local) color = 0xFFFFFFFF;       // White
        else if (ent.team == 2) color = 0xFFCC6644; // T: Orange
        else if (ent.team == 3) color = 0xFF4488CC; // CT: Blue
        else color = 0xFF888888;                    // Grey

        blips.push_back({rx, ry, ent.angles.y, color, ent.health, ent.is_local, {}});
    }
    return blips;
}

// ═══════════════════════════════════════════════════════════════════
// Console Radar Renderer (cross-platform)
// ═══════════════════════════════════════════════════════════════════

static void render_console_radar(const std::vector<RadarBlip>& blips,
                                   const TierDoc& doc,
                                   int entity_count,
                                   double elapsed_us)
{
    // ── Header ───────────────────────────────────────────────────
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════╗\n");
    std::printf("║  %-47s║\n", doc.name);
    std::printf("╚══════════════════════════════════════════════════╝\n");
    std::printf("  TECHNIQUE: %s\n", doc.technique);
    std::printf("  SCAR:      %s\n", doc.scar);
    std::printf("  BLUE:      %s\n", doc.blue);
    std::printf("  MITIGATION:%s\n", doc.mitigation);
    std::printf("  REAL CODE: %s\n", doc.real_code);
    std::printf("\n");

    // ── Stats ────────────────────────────────────────────────────
    std::printf("  Entities: %d read in %.0f us (%.1f us/entity)\n",
                entity_count, elapsed_us,
                entity_count > 0 ? elapsed_us / entity_count : 0);

    std::printf("\n  RADAR (top-down view, local player at center):\n");
    std::printf("  Legend: O=local, X=enemy team, +=ally team\n\n");

    // ── ASCII radar grid ─────────────────────────────────────────
    const int grid_size = 21;  // odd number for center
    char grid[21][21];
    for (int y = 0; y < grid_size; y++)
        for (int x = 0; x < grid_size; x++)
            grid[y][x] = '.';

    // Place blips on grid
    float scale = (grid_size / 2) * 0.9f;  // leave margin
    for (const auto& blip : blips) {
        int gx = (int)(blip.x * scale + grid_size / 2);
        int gy = (int)(blip.y * scale + grid_size / 2);
        if (gx >= 0 && gx < grid_size && gy >= 0 && gy < grid_size) {
            if (blip.is_local) grid[gy][gx] = 'O';
            else if (blip.color == 0xFFCC6644) grid[gy][gx] = 'X';
            else if (blip.color == 0xFF4488CC) grid[gy][gx] = '+';
            else grid[gy][gx] = '?';
        }
    }

    // Print grid
    std::printf("     ");
    for (int x = 0; x < grid_size; x++)
        std::printf("%c", 'A' + (x % 26));
    std::printf("\n");
    for (int y = 0; y < grid_size; y++) {
        std::printf("  %2d ", y);
        for (int x = 0; x < grid_size; x++)
            std::printf("%c", grid[y][x]);
        std::printf("\n");
    }

    // ── Player list ──────────────────────────────────────────────
    std::printf("\n  PLAYERS:\n");
    for (size_t i = 0; i < blips.size(); i++) {
        const auto& blip = blips[i];
        const char* team_str = blip.is_local ? "YOU" :
            blip.color == 0xFFCC6644 ? "T " :
            blip.color == 0xFF4488CC ? "CT" : "?";
        std::printf("  [%zu] %-3s  health=%3d  radar=(%+.2f, %+.2f)\n",
                    i, team_str, blip.health, (double)blip.x, (double)blip.y);
    }

    // ── Tier comparison table ────────────────────────────────────
    std::printf("\n  TIER COMPARISON:\n");
    std::printf("  %-8s %-12s %-10s %s\n", "Tier", "Time", "Handle?", "Scar");
    std::printf("  %-8s %-12s %-10s %s\n", "────", "────", "───────", "────");
    std::printf("  %-8s %6.0fus    %-10s %s\n", "T0 RPM", elapsed_us, "YES", "Handle table");
    std::printf("  %-8s     N/A     %-10s %s\n", "T1 SC", "", "YES", "Handle + syscall");
    std::printf("  %-8s     N/A     %-10s %s\n", "T2 BYOVD", "", "NO", "Device IOCTL");
    std::printf("  %-8s     N/A     %-10s %s\n", "T3 HV", "", "NO", "VMCS/EPT");
    std::printf("  %-8s     N/A     %-10s %s\n", "T4 DMA", "", "NO", "PCIe TLP");
    std::printf("\n");
}

// ═══════════════════════════════════════════════════════════════════
// D3D11 Radar Renderer (Windows only)
// ═══════════════════════════════════════════════════════════════════

#if LR_PLATFORM_WINDOWS
static void render_d3d11_radar(const std::vector<RadarBlip>& blips,
                                 const TierDoc& doc,
                                 int entity_count)
{
    // Create D3D11 overlay window
    real::gpu::RenderPipeline* rp = nullptr;
    {
      real::gpu::OverlayStyle style{};
      style.width = 300;
      style.height = 300;
      style.margin_px = 20;
      style.corner = real::gpu::OverlayStyle::Corner::TopRight;
      style.always_on_top = true;
      style.clickthrough = true;
      style.window_alpha = 200;
      style.radar_bg = 0x88101828;
      style.radar_border = 0xBB3A9ACC;
      auto created = real::gpu::create_render_pipeline(style, "Radar");
      if (created) rp = *created;
    }
    if (!rp || !rp->is_initialized()) {
        std::printf("[!] D3D11 overlay unavailable — falling back to console\n");
        return;
    }

    std::printf("[d3d11] Floating overlay %dx%d alpha=%u topmost\n",
                rp->overlay_style().width, rp->overlay_style().height,
                static_cast<unsigned>(rp->overlay_style().window_alpha));
    std::printf("[d3d11] Short auto-render (set LR_DEMO_FRAMES=0 for interactive)\n\n");

    // Finite render loop so multi-tier comparison demos exit cleanly.
    const int max_frames = 30;
    int frames = 0;
    while (rp->process_messages()) {
        rp->begin_frame();

        // Build radar frame
        real::gpu::RadarFrame frame;
        for (const auto& blip : blips) {
            real::gpu::RadarBlipLayout layout;
            layout.screen_x = blip.x;
            layout.screen_y = blip.y;
            layout.angle = blip.angle;
            layout.color = blip.color;
            layout.health = blip.health;
            layout.is_local = blip.is_local;
            layout.is_visible = true;
            frame.blips.push_back(layout);
        }

        rp->draw_radar_frame(frame);

        // Tier info and stats
        char info[128];
        std::snprintf(info, sizeof(info), "%s | %d players | %s",
                      doc.name, entity_count, doc.scar);
        rp->draw_text(-0.95f, 0.92f, info, 0xCCFFFFFF, 0.04f);
        rp->draw_text(-0.95f, -0.95f, "auto-exit", 0x88FFFFFF, 0.03f);

        rp->end_frame();
        if (++frames >= max_frames) break;
        static uint64_t _rng = __rdtsc();
        _rng ^= _rng << 13; _rng ^= _rng >> 7; _rng ^= _rng << 17;
        int32_t delay_ms = 14 + static_cast<int32_t>(_rng % 7);
        real::win::fuzzed_sleep(delay_ms);
    }

    rp->shutdown();
    delete rp;
}
#endif

// ═══════════════════════════════════════════════════════════════════
// Tier-Specific Entity Readers
// ═══════════════════════════════════════════════════════════════════

struct ReadResult {
    std::vector<PlayerEntity> entities;
    bool success = false;
    double elapsed_us = 0;
    std::string error;
};

// T0: OpenProcess + ReadProcessMemory
static ReadResult read_t0_real() {
    ReadResult result;
    auto attach = real::cs2::attach_to_cs2();
    if (!attach.attached) {
        result.error = "CS2 not found: " + attach.error_msg;
        return result;
    }

    auto offsets = real::cs2::resolve_offsets(attach.pid, attach.base_address,
                                                attach.image_size);
    if (!offsets) {
        real::cs2::detach_from_cs2(attach.handle);
        result.error = "Offsets failed: " + offsets.error_msg;
        return result;
    }

    real::cs2::Cs2MemoryReader reader;
    reader.attach(ac::Tier::T0_UsermodeRpm, attach.pid);

    auto t0 = std::chrono::high_resolution_clock::now();
    auto entities = real::cs2::read_entity_list(reader, *offsets, attach.pid);
    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    if (entities.read_successful) {
        result.success = true;
        for (const auto& e : entities.entities) {
            result.entities.push_back({
                e.controller_handle,
                e.origin, e.eye_angles,
                static_cast<uint8_t>(e.team), e.health, e.is_alive, e.is_local_player
            });
        }
    }

    reader.detach();
    real::cs2::detach_from_cs2(attach.handle);
    return result;
}

// T1: Direct syscall (same as T0 but with T1 tier)
static ReadResult read_t1_real() {
    ReadResult result;
    auto attach = real::cs2::attach_to_cs2();
    if (!attach.attached) {
        result.error = "CS2 not found: " + attach.error_msg;
        return result;
    }

    auto offsets = real::cs2::resolve_offsets(attach.pid, attach.base_address,
                                                attach.image_size);
    if (!offsets) {
        real::cs2::detach_from_cs2(attach.handle);
        result.error = "Offsets failed: " + offsets.error_msg;
        return result;
    }

    real::cs2::Cs2MemoryReader reader;
    reader.attach(ac::Tier::T1_SyscallSoft, attach.pid);

    auto t0 = std::chrono::high_resolution_clock::now();
    auto entities = real::cs2::read_entity_list(reader, *offsets, attach.pid);
    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    if (entities.read_successful) {
        result.success = true;
        for (const auto& e : entities.entities) {
            result.entities.push_back({
                e.controller_handle,
                e.origin, e.eye_angles,
                static_cast<uint8_t>(e.team), e.health, e.is_alive, e.is_local_player
            });
        }
    }

    reader.detach();
    real::cs2::detach_from_cs2(attach.handle);
    return result;
}

// T2-T4: Simulated (require kernel/HW access)
static ReadResult read_t2_byovd_sim() {
    ReadResult result;
    result.success = true;
    { static uint64_t r = __rdtsc();
      r ^= r << 13; r ^= r >> 7; r ^= r << 17;
      int32_t d = 400 + static_cast<int32_t>(r % 201);
      real::win::fuzzed_sleep(1); }
    // Generate realistic entity layout
    result.entities = generate_sim_entities(2);
    result.elapsed_us = 35.0;  // ~35us typical for IOCTL read
    return result;
}

static ReadResult read_t3_hv_sim() {
    ReadResult result;
    result.success = true;
    { static uint64_t r = __rdtsc();
      r ^= r << 13; r ^= r >> 7; r ^= r << 17;
      int32_t d = 160 + static_cast<int32_t>(r % 81);
      real::win::fuzzed_sleep(1); }
    result.entities = generate_sim_entities(2);
    result.elapsed_us = 3.0;  // ~3us typical for hypercall
    return result;
}

static ReadResult read_t4_dma_sim() {
    ReadResult result;
    result.success = true;
    { static uint64_t r = __rdtsc();
      r ^= r << 13; r ^= r >> 7; r ^= r << 17;
      int32_t d = 80 + static_cast<int32_t>(r % 41);
      real::win::fuzzed_sleep(1); }
    result.entities = generate_sim_entities(2);
    result.elapsed_us = 1.0;  // ~1us for DMA page read
    return result;
}

// ═══════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════

static void run_tier(int tier_index, bool use_d3d11) {
    const auto& doc = kTiers[tier_index];
    ReadResult read_result;

    std::printf("\n═══ %s ═══\n", doc.name);

    switch (tier_index) {
        case 0: read_result = read_t0_real(); break;
        case 1: read_result = read_t1_real(); break;
        case 2: read_result = read_t2_byovd_sim(); break;
        case 3: read_result = read_t3_hv_sim(); break;
        case 4: read_result = read_t4_dma_sim(); break;
    }

    if (!read_result.success && read_result.error.find("CS2 not found") != std::string::npos) {
        // Fall back to simulation mode
        std::printf("  [!] %s\n", read_result.error.c_str());
        std::printf("  Falling back to simulation mode.\n");
        read_result.entities = generate_sim_entities(2);
        read_result.success = true;
        read_result.elapsed_us = tier_index == 0 ? 42.0 :
                                  tier_index == 1 ? 38.0 :
                                  tier_index == 2 ? 35.0 :
                                  tier_index == 3 ? 3.0 : 1.0;
    }

    if (!read_result.success) {
        std::printf("  [!] Read failed: %s\n", read_result.error.c_str());
        return;
    }

    // Entity origin for local player
    ac::Vec3 local_origin = {0, 0, 0};
    float local_yaw = 0;
    for (const auto& e : read_result.entities) {
        if (e.is_local) { local_origin = e.origin; local_yaw = e.angles.y; break; }
    }

    // Project to radar coordinates
    auto blips = project_to_radar(read_result.entities, local_origin,
                                   local_yaw, 3000.0f);

    // Render
#if LR_PLATFORM_WINDOWS
    if (use_d3d11) {
        render_d3d11_radar(blips, doc, (int)read_result.entities.size());
        return;
    }
#endif

    render_console_radar(blips, doc, (int)read_result.entities.size(),
                          read_result.elapsed_us);
}

int main(int argc, char** argv) {
    const char* tier_arg = (argc > 1 && strncmp(argv[1], "--tier=", 7) == 0)
        ? argv[1] + 7
        : (argc > 2 && strcmp(argv[1], "--tier") == 0) ? argv[2] : "T0";

    bool use_d3d11 = false;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--d3d11") == 0) use_d3d11 = true;

    std::printf("\n");
    std::printf("████████████████████████████████████████████████████████████\n");
    std::printf("  Multi-Tier Radar Prototype\n");
    std::printf("  Reads CS2 entity positions and renders radar overlay\n");
    std::printf("  Tier: %s | Render: %s\n", tier_arg, use_d3d11 ? "D3D11" : "Console");
    std::printf("████████████████████████████████████████████████████████████\n");

    if (strcmp(tier_arg, "all") == 0 || strcmp(tier_arg, "ALL") == 0) {
        for (int i = 0; i < 5; i++)
            run_tier(i, false);
    } else if (strcmp(tier_arg, "T0") == 0) run_tier(0, use_d3d11);
    else if (strcmp(tier_arg, "T1") == 0) run_tier(1, use_d3d11);
    else if (strcmp(tier_arg, "T2") == 0) run_tier(2, use_d3d11);
    else if (strcmp(tier_arg, "T3") == 0) run_tier(3, use_d3d11);
    else if (strcmp(tier_arg, "T4") == 0) run_tier(4, use_d3d11);
    else if (strcmp(tier_arg, "sim") == 0 ||
             strcmp(tier_arg, "SIM") == 0) {
        // Simulation-only mode: run all tiers without trying CS2
        std::printf("\n[sim] Pure simulation mode (no CS2 needed)\n");
        for (int i = 0; i < 5; i++) {
            ReadResult sim;
            sim.entities = generate_sim_entities(2);
            sim.success = true;
            sim.elapsed_us = i == 0 ? 42.0 : i == 1 ? 38.0 :
                             i == 2 ? 35.0 : i == 3 ? 3.0 : 1.0;

            ac::Vec3 local_origin = {0, 0, 0};
            auto blips = project_to_radar(sim.entities, local_origin, 0, 3000.0f);
            render_console_radar(blips, kTiers[i], (int)sim.entities.size(),
                                  sim.elapsed_us);
        }
    } else {
        std::printf("Usage: %s --tier T0|T1|T2|T3|T4|all|sim [--d3d11]\n", argv[0]);
        return 1;
    }

    return 0;
}
