// host_sim.cpp — FPGA DMA T4 CS2 entity simulator entry + self-check.

#include "host_sim_fpga.hpp"
#include "host_sim_memory.hpp"
#include "fpga_dma.h"
#include "obf.hpp"

#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

static bool nearly(float a, float b) { return (a - b) < 0.01f && (b - a) < 0.01f; }

static int run_self_check() {
    int failures = 0;
    Cs2MemoryImage mem;
    Cs2MemoryImage::s_inst = &mem;
    auto pr = [](uint64_t pa, void* b, size_t s) -> bool {
        return Cs2MemoryImage::s_inst->phys_read(pa, b, s);
    };

    auto check = [&failures](bool cond, const char* what) {
        printf("%s %s\n", cond ? "PASS" : "FAIL", what);
        if (!cond) failures++;
    };

    // 1. Page translation: client.dll 2MB page
    uint64_t pa = PageTableWalker::translate(
        Cs2MemoryImage::kClientVa, mem.cr3(), pr);
    check(pa == mem.client_base_pa(), "translate(client.dll VA) == client PA");

    // 2. Page translation: entity list 4KB page
    pa = PageTableWalker::translate(
        Cs2MemoryImage::kEntityListVa, mem.cr3(), pr);
    check(pa == mem.entity_list_pa(), "translate(entity list VA) == entity list PA");

    // 3. Page translation: controller + pawn
    pa = PageTableWalker::translate(
        Cs2MemoryImage::kControllerVa, mem.cr3(), pr);
    check(pa == Cs2MemoryImage::kControllerPa, "translate(controller VA) == controller PA");
    pa = PageTableWalker::translate(
        Cs2MemoryImage::kPawnVa, mem.cr3(), pr);
    check(pa == Cs2MemoryImage::kPawnPa, "translate(pawn VA) == pawn PA");

    // 4. 1GB large page (PDPE.PS)
    pa = PageTableWalker::translate(
        Cs2MemoryImage::kHuge1GVa, mem.cr3(), pr);
    check(pa == Cs2MemoryImage::kHuge1GPa, "translate(1GB VA) == 1GB PA");
    pa = PageTableWalker::translate(
        Cs2MemoryImage::kHuge1GVa + 0x12345678ULL, mem.cr3(), pr);
    check(pa == Cs2MemoryImage::kHuge1GPa + 0x12345678ULL,
          "translate(1GB VA+offset) preserves offset");

    // 5. Unmapped VA must return 0
    pa = PageTableWalker::translate(0x7FF000000000ULL, mem.cr3(), pr);
    check(pa == 0, "translate(unmapped VA) == 0");

    // 6. EPROCESS scan finds CR3
    uint64_t cr3 = PageTableWalker::find_process_cr3(mem.pid(), pr);
    check(cr3 == mem.cr3(), "find_process_cr3(1234) == CR3");

    // 7. Entity read: 5 valid players with expected values
    DmaEntityData entities[64];
    size_t n = Cs2DmaReader::read_entities(
        pr, mem.pid(), mem.client_base_pa(), mem.cr3(), entities, 64);
    check(n == 5, "read_entities returns 5 valid players");
    if (n >= 1) {
        check(entities[0].health == 100, "player0 health == 100");
        check(entities[0].team == 3, "player0 team == 3");
        check(nearly(entities[0].origin_x, 123.4f) &&
              nearly(entities[0].origin_y, 56.7f) &&
              nearly(entities[0].origin_z, 1.2f), "player0 origin matches");
        check(nearly(entities[0].yaw, 45.0f), "player0 yaw == 45");
        check(nearly(entities[0].velocity_x, 10.0f) &&
              nearly(entities[0].velocity_y, -5.0f), "player0 velocity matches");
    }
    if (n >= 3) {
        check(entities[3].team == 2, "player3 team == 2 (T side)");
    }

    printf("\nSelf-check: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

// ═══════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    bool do_iommu = false;
    int iterations = 1;
    bool do_check = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--iommu") do_iommu = true;
        else if (a == "--quiet") host_sim_set_quiet(true);
        else if (a == "--check") do_check = true;
        else if (a == "--iterations" && i + 1 < argc) {
            iterations = std::atoi(argv[++i]);
            if (iterations < 1) iterations = 1;
        } else if (a == "--help") {
            printf("Usage: host_sim [--iommu] [--iterations N] [--quiet] [--check]\n");
            return 0;
        }
    }

    if (do_check) return run_self_check();

    host_sim_log("HOST", "================================================");
    host_sim_log("HOST", "T4 PCIe DMA: CS2 Entity Read via FPGA");
    host_sim_log("HOST", "Zero OS - no handle, no driver, no syscall");
    host_sim_log("HOST", "================================================");
    host_sim_log("", "");

    // ── Memory image ──
    Cs2MemoryImage mem;
    Cs2MemoryImage::s_inst = &mem;
    host_sim_log("MEM", "client.dll PA = 0x%016" PRIx64, mem.client_base_pa());
    host_sim_log("MEM", "CS2 CR3 = 0x%016" PRIx64, mem.cr3());
    host_sim_log("MEM", "CS2 PID = %u", mem.pid());
    host_sim_log("", "");

    // ── FPGA device ──
    FpgaDevice fpga;
    fpga.mem = &mem;
    fpga.iommu_enabled = do_iommu;
    FpgaDevice::s_inst = &fpga;
    fpga.boot();
    host_sim_log("", "");

    // DMA read path goes through the FPGA (models TLP latency + IOMMU gate)
    PhysReadFn fpga_read = &FpgaDevice::phys_cb;

    // ── Phase 1: Find CS2 via physical scan ──
    host_sim_log("HOST", "PHASE 1: Physical scan for CS2 EPROCESS");
    uint64_t cr3 = PageTableWalker::find_process_cr3(mem.pid(), fpga_read);
    if (!cr3) {
        cr3 = mem.cr3();
        host_sim_log("T4", "Using known CR3 (scan failed)");
    } else {
        host_sim_log("T4", "CS2 found! CR3=0x%016" PRIx64, cr3);
    }
    host_sim_log("", "");

    // ── Phase 2: Page table walk client.dll VA → PA ──
    host_sim_log("HOST", "PHASE 2: Page walk client.dll VA -> PA");
    uint64_t cpa = PageTableWalker::translate(
        Cs2MemoryImage::kClientVa, cr3, fpga_read);
    if (!cpa) cpa = mem.client_base_pa();
    host_sim_log("T4", "client.dll VA 0x%016" PRIx64 " -> PA 0x%016" PRIx64,
        Cs2MemoryImage::kClientVa, cpa);
    host_sim_log("", "");

    // ── Phase 3: Read entity list pointer ──
    host_sim_log("HOST", "PHASE 3: Read entity list pointer");
    uint64_t el_va = 0;
    fpga_read(cpa + Cs2Offsets::dec(Cs2Offsets::dwEntityList), &el_va, 8);
    host_sim_log("T4", "Entity list VA = 0x%016" PRIx64, el_va);
    uint64_t el_pa = PageTableWalker::translate(el_va, cr3, fpga_read);
    host_sim_log("T4", "Entity list PA = 0x%016" PRIx64, el_pa);
    host_sim_log("", "");

    // ── Phase 4: Descriptor-ring bulk DMA demo ──
    host_sim_log("HOST", "PHASE 4: Descriptor-ring bulk DMA (entity list pages)");
    DmaDescriptor ring[3] = {
        { el_pa,             0,      4096, DMAF_READ | DMAF_CHAIN, 0, 0 },
        { el_pa + 0x1000,    0x1000, 4096, DMAF_READ | DMAF_CHAIN, 0, 0 },
        { 0,                 0,         0, DMAF_LAST,              0, 0 },
    };
    fpga.wr(REG_DESC_COUNT, 3);
    fpga.run_dma(ring, 3);
    host_sim_log("T4", "FPGA data buffer[0..7] = %02x %02x %02x %02x %02x %02x %02x %02x",
        fpga.db[0], fpga.db[1], fpga.db[2], fpga.db[3],
        fpga.db[4], fpga.db[5], fpga.db[6], fpga.db[7]);
    host_sim_log("", "");

    // ── Phase 5: Full entity read via DMA reader ──
    host_sim_log("HOST", "PHASE 5: CS2 DMA entity read");
    host_sim_log("HOST", "Using Cs2DmaReader::read_entities() via PCIe DMA");
    host_sim_log("HOST", "No OpenProcess, no kernel, no OS - pure DMA reads over PCIe");
    host_sim_log("", "");

    size_t n = 0;
    DmaEntityData entities[64];
    for (int iter = 0; iter < iterations; ++iter) {
        n = Cs2DmaReader::read_entities(
            fpga_read, mem.pid(), cpa, cr3, entities, 64);
    }

    host_sim_log("T4", "Found %zu valid entities", n);
    host_sim_log("", "");
    printf("  %-4s %-6s %-5s %-12s %-12s %-12s %-6s %-10s\n",
           "Slot", "Health", "Team", "Origin.X", "Origin.Y", "Origin.Z", "Yaw", "Speed");
    printf("  ---- ------ ----- ------------ ------------ ------------ ------ ----------\n");
    for (size_t i = 0; i < n; i++) {
        const float speed = std::sqrt(
            entities[i].velocity_x * entities[i].velocity_x +
            entities[i].velocity_y * entities[i].velocity_y +
            entities[i].velocity_z * entities[i].velocity_z);
        printf("  %-4u %-6u %-5u %-12.1f %-12.1f %-12.1f %-6.1f %-10.1f\n",
               entities[i].slot_index, entities[i].health, entities[i].team,
               entities[i].origin_x, entities[i].origin_y, entities[i].origin_z,
               entities[i].yaw, speed);
    }

    // Deterministic exit: success requires the synthetic 5-player table.
    // IOMMU mode is an intentional failure path for evidence; non-IOMMU must match.
    if (!do_iommu) {
        if (n != 5) {
            host_sim_log("ERR", "expected 5 valid entities, got %zu", n);
            return 1;
        }
        if (entities[0].health != 100 || entities[0].team != 3 ||
            !nearly(entities[0].origin_x, 123.4f) ||
            !nearly(entities[0].origin_y, 56.7f) ||
            !nearly(entities[0].origin_z, 1.2f) ||
            !nearly(entities[0].yaw, 45.0f)) {
            host_sim_log("ERR", "player0 fields mismatch synthetic fixture");
            return 1;
        }
    } else {
        host_sim_log("IOMMU", "entity read blocked as expected (found %zu)", n);
        if (n != 0 || fpga.dma_err != DMA_ERR_IOMMU) {
            host_sim_log("ERR", "IOMMU path should yield 0 entities and DMA_ERR_IOMMU");
            return 1;
        }
    }
    host_sim_log("", "");

    // ── Summary ──
    host_sim_log("HOST", "================================================");
    host_sim_log("HOST", "T4 PCIe DMA Summary");
    host_sim_log("HOST", "================================================");
    host_sim_log("T4",   "FPGA:       Custom Xilinx Artix-7 bitstream");
    host_sim_log("T4",   "PCIe:       gen2 x4 @ 5.0 GT/s");
    host_sim_log("T4",   "Latency:    %" PRIu64 " us simulated", fpga.total_us);
    host_sim_log("T4",   "TLPs:       %" PRIu64 " simulated read TLPs", fpga.tlp_count);
    host_sim_log("T4",   "Bandwidth:  %" PRIu64 " bytes over %" PRIu64 " us",
        fpga.total_bytes, fpga.total_us);
    host_sim_log("", "");
    host_sim_log("SCAR",  "PCIe read TLP on bus - no OS involvement");
    host_sim_log("BLUE",  "IOMMU DMA remapping blocks this attack");
    host_sim_log("BLUE",  "ACS (Access Control Services) in PCIe switch");
    host_sim_log("FIX",   "Enable VT-d/AMD-Vi with DMA remapping in firmware");
    host_sim_log("", "");
    host_sim_log("HOST", "Tier comparison for reading CS2 entities:");
    host_sim_log("HOST", "  T0 OpenProcess+RPM:  HANDLE VISIBLE, ~42 us");
    host_sim_log("HOST", "  T1 Syscall:          HANDLE VISIBLE, ~38 us");
    host_sim_log("HOST", "  T2 BYOVD IOCTL:      DEVICE VISIBLE, ~35 us");
    host_sim_log("HOST", "  T4 PCIe DMA:         NO OS ARTIFACT, ~2 us/page");
    host_sim_log("", "");
    host_sim_log("HOST", "T4 wins: zero evidence in OS. Loses: needs FPGA hardware.");
    return 0;
}
