#pragma once

// Shared types for host_sim multi-TU split.
#include "fpga_dma.h"
#include "obf.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <random>

class Cs2MemoryImage {
public:
    struct PlayerTemplate {
        int health, team;
        float ox, oy, oz, yaw;
        float vx, vy, vz;
        const char* name;
    };

    // ── Layout constants ──
    // 2 GiB physical model: enough room for a real 1GB-aligned large page at 0x40000000.
    static constexpr size_t kPS            = 0x80000000ULL;
    static constexpr uint64_t kPml4        = 0x00100000;
    static constexpr uint64_t kPdpUser     = 0x00101000;
    static constexpr uint64_t kPdClient    = 0x00102000;
    static constexpr uint64_t kPdData      = 0x00103000;
    static constexpr uint64_t kPtEntity    = 0x00104000;
    static constexpr uint64_t kPtCtrl      = 0x00105000;
    static constexpr uint64_t kPtPawn      = 0x00106000;
    static constexpr uint64_t kPdpKernel   = 0x00107000;
    static constexpr uint64_t kPdKernel    = 0x00108000;

    static constexpr uint64_t kClientVa    = 0x180000000ULL;  // 2MB large pages
    static constexpr uint64_t kEntityListVa = 0x1C0000000ULL; // 4KB pages
    static constexpr uint64_t kControllerVa = 0x1D0000000ULL;
    static constexpr uint64_t kPawnVa      = 0x1E0000000ULL;
    static constexpr uint64_t kHuge1GVa    = 0x80000000ULL;   // 1GB large page (PDPE.PS)

    static constexpr uint64_t kClientPa    = 0x00200000;   // 2MB-aligned image base
    static constexpr uint64_t kEprocessPa  = 0x00180000;
    static constexpr uint64_t kEntityListPa = 0x11000000;
    static constexpr uint64_t kControllerPa = 0x12000000;
    static constexpr uint64_t kPawnPa      = 0x13000000;
    static constexpr uint64_t kHuge1GPa    = 0x40000000ULL;  // 1GB-aligned phys base

    Cs2MemoryImage()
        : m_phys_mem(std::make_unique<uint8_t[]>(kPS)),
          m_client_base_pa(kClientPa),
          m_cr3(kPml4),
          m_entity_list_pa(kEntityListPa) {
        std::memset(m_phys_mem.get(), 0, kPS);
        std::mt19937_64 rng(42);   // deterministic
        build_page_tables();
        build_client_dll();
        build_eprocess();
        build_entities();
    }

    // ── Physical read / write ──
    bool phys_read(uint64_t pa, void* buf, size_t sz) const {
        if (!buf || pa + sz > kPS) return false;
        std::memcpy(buf, m_phys_mem.get() + pa, sz);
        return true;
    }
    bool phys_write(uint64_t pa, const void* buf, size_t sz) {
        if (!buf || pa + sz > kPS) return false;
        std::memcpy(m_phys_mem.get() + pa, buf, sz);
        return true;
    }

    // ── Accessors ──
    uint64_t client_base_pa() const { return m_client_base_pa; }
    uint64_t cr3() const { return m_cr3; }
    uint32_t pid() const { return 1234; }
    uint64_t entity_list_pa() const { return m_entity_list_pa; }
    int num_players() const { return 5; }

    static bool phys_cb(uint64_t pa, void* buf, size_t sz) {
        return s_inst && s_inst->phys_read(pa, buf, sz);
    }
    static Cs2MemoryImage* s_inst;

private:
    std::unique_ptr<uint8_t[]> m_phys_mem;
    uint64_t m_client_base_pa, m_cr3, m_entity_list_pa;

    void set_u64(uint64_t pa, uint64_t v) {
        if (pa + 8 <= kPS) std::memcpy(m_phys_mem.get() + pa, &v, 8);
    }
    void set_u32(uint64_t pa, uint32_t v) {
        if (pa + 4 <= kPS) std::memcpy(m_phys_mem.get() + pa, &v, 4);
    }
    void set_u8(uint64_t pa, uint8_t v) {
        if (pa + 1 <= kPS) std::memcpy(m_phys_mem.get() + pa, &v, 1);
    }
    void set_floats(uint64_t pa, const float* f, size_t n) {
        if (pa + n * 4 <= kPS) std::memcpy(m_phys_mem.get() + pa, f, n * 4);
    }

    // ── Build 4-level page tables ──
    void build_page_tables() {
        // PML4 → PDP (user) + kernel
        set_u64(kPml4 + 0 * 8, kPdpUser | 0x07);                    // PML4[0] user
        set_u64(kPml4 + 0x1FF * 8, kPdpKernel | 0x03);              // PML4[511] kernel

        // PDP[2] → 1GB large page (PS=1) for VA 0x80000000 → PA 0x40000000
        // Present|RW|User|PS = 0x87. Frame must be 1GB-aligned (bits 29:0 clear).
        set_u64(kPdpUser + 2 * 8, kHuge1GPa | 0x87);

        // PDP[6] → PD for client.dll (VA 0x180000000, 2MB pages)
        set_u64(kPdpUser + 6 * 8, kPdClient | 0x07);
        // PDP[7] → PD for entity list / controllers / pawns (VA 0x1C0..0x1FF)
        set_u64(kPdpUser + 7 * 8, kPdData | 0x07);

        // PD[0..] = client.dll: 19 × 2MB pages cover the full ~38MB image (PS=1)
        for (uint32_t j = 0; j < 19; ++j) {
            set_u64(kPdClient + j * 8,
                    (m_client_base_pa + static_cast<uint64_t>(j) * 0x200000) | 0x87);
        }

        // PD for data region: index 0 → entity list PT, 0x80 → ctrl PT, 0x100 → pawn PT
        set_u64(kPdData + 0 * 8, kPtEntity | 0x07);
        set_u64(kPdData + 0x80 * 8, kPtCtrl | 0x07);
        set_u64(kPdData + 0x100 * 8, kPtPawn | 0x07);

        // Entity list: 4 pages of 4KB at PA 0x11000000
        for (uint32_t j = 0; j < 4; ++j) {
            set_u64(kPtEntity + j * 8, (kEntityListPa + j * 0x1000) | 0x07);
        }
        // Controllers: 64 pages
        for (uint32_t j = 0; j < 64; ++j) {
            set_u64(kPtCtrl + j * 8, (kControllerPa + j * 0x1000) | 0x07);
        }
        // Pawns: 64 pages
        for (uint32_t j = 0; j < 64; ++j) {
            set_u64(kPtPawn + j * 8, (kPawnPa + j * 0x1000) | 0x07);
        }

        // Marker payload at start of 1GB mapping (for translate / read_virtual tests)
        set_u64(kHuge1GPa, 0x1CEB0001C0FFEULL);
        set_u64(kHuge1GPa + 0x1000, 0x1CEB0002C0FFEULL);

        // Minimal kernel mapping (identity 2MB page)
        set_u64(kPdpKernel + 0 * 8, kPdKernel | 0x03);
        set_u64(kPdKernel + 0 * 8, 0x000000000ULL | 0x83);
    }

    // ── client.dll image: MZ header + entity list pointer at dwEntityList ──
    void build_client_dll() {
        // PE headers ("MZ")
        m_phys_mem[m_client_base_pa] = 'M';
        m_phys_mem[m_client_base_pa + 1] = 'Z';

        // dwEntityList RVA → store the entity list VA pointer there
        const uint64_t rva = Cs2Offsets::dec(Cs2Offsets::dwEntityList);
        set_u64(m_client_base_pa + rva, kEntityListVa);
    }

    // ── EPROCESS: name + PID + DirectoryTableBase at both Win10/Win11 offsets ──
    void build_eprocess() {
        // Win10 layout: ImageFileName +0x2B8, PID +0x2E0
        const char* name = OBF("cs2.exe");
        std::memcpy(m_phys_mem.get() + kEprocessPa + 0x2B8, name, 8);
        set_u32(kEprocessPa + 0x2E0, pid());
        set_u64(kEprocessPa + 0x28, m_cr3);

        // Win11 layout: ImageFileName +0x3E0, PID +0x408
        std::memcpy(m_phys_mem.get() + kEprocessPa + 0x3E0, name, 8);
        set_u32(kEprocessPa + 0x408, pid());

        // PEB pointer (not used by the DMA walk, but realistic)
        set_u64(kEprocessPa + 0x550, 0x0000000100000000ULL);
    }

    // ── Entity list + player objects ──
    void build_entities() {
        PlayerTemplate pts[] = {
            {100, 3,  123.4f,  56.7f,  1.2f,  45.0f,  10.0f, -5.0f, 0.0f, "Local"},
            { 85, 3,  456.7f, 123.4f,  0.5f, 120.0f, -20.0f, 3.0f, 0.0f, "Enemy1"},
            { 67, 3,  789.1f, 234.5f,  2.1f, 200.0f,   5.0f, 30.0f, 0.0f, "Enemy2"},
            { 42, 2,  111.2f, 333.4f,  1.8f, 300.0f,   0.0f, 15.0f, 0.0f, "Enemy3"},
            { 91, 2,  555.6f, 777.8f,  0.9f,  15.0f,   0.0f,  0.0f, 0.0f, "Enemy4"},
            {  0, 0,      0,      0,    0,     0,       0,    0,    0,   "Dead"},
        };

        const uint64_t stride = Cs2Offsets::dec(Cs2Offsets::kIdentityStride);

        for (int i = 0; i < 64; ++i) {
            // Controllers occupy slots 0..5; their pawns occupy slots 64..69.
            if (i < 6) {
                const auto& pt = pts[i];
                const uint64_t ctrl_pa = kControllerPa + static_cast<uint64_t>(i) * 0x1000;
                const uint64_t pawn_pa = kPawnPa + static_cast<uint64_t>(i) * 0x1000;

                // Entity slot entry: controller VA pointer
                set_u64(kEntityListPa + static_cast<uint64_t>(i) * stride,
                        kControllerVa + static_cast<uint64_t>(i) * 0x1000);
                // Pawn slot entry: pawn VA pointer
                set_u64(kEntityListPa + static_cast<uint64_t>(64 + i) * stride,
                        kPawnVa + static_cast<uint64_t>(i) * 0x1000);

                // Controller fields: team + pawn handle (handle → index 64+i)
                set_u8(ctrl_pa + Cs2Offsets::dec(Cs2Offsets::m_iTeamNum),
                       static_cast<uint8_t>(pt.team));
                const uint64_t handle =
                    (static_cast<uint64_t>(0xAAAA) << 32) |
                    static_cast<uint64_t>(64 + i);
                set_u64(ctrl_pa + Cs2Offsets::dec(Cs2Offsets::m_hPlayerPawn), handle);

                // Pawn fields: health, origin, eye angles, velocity
                const uint32_t hp = static_cast<uint32_t>(pt.health);
                set_u32(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_iHealth), hp);

                float origin[3] = {pt.ox, pt.oy, pt.oz};
                set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vOldOrigin), origin, 3);

                float eye_angles[3] = {0.0f, pt.yaw, 0.0f};
                set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_angEyeAngles), eye_angles, 3);

                float velocity[3] = {pt.vx, pt.vy, pt.vz};
                set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vecVelocity), velocity, 3);

                // Player name at controller + 0x100 (cosmetic)
                std::memcpy(m_phys_mem.get() + ctrl_pa + 0x100, pt.name,
                            std::strlen(pt.name) + 1);
            }
        }
    }
};

