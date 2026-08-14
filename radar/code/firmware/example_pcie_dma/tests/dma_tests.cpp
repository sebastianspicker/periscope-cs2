// dma_tests.cpp — Unit tests for the T4 PCIe DMA firmware stack.
//
// Verifies:
//   - x86-64 page table translation (4KB/2MB/1GB pages, unmapped VAs)
//   - virtual memory reads across page boundaries
//   - EPROCESS scan / CR3 discovery
//   - CS2 entity list read pipeline
//   - FPGA register map / descriptor ring DMA
//
// BUILD (CMake):  cmake --build build --target dma_tests
// RUN:   ./build/bin/dma_tests   (exit code 0 = all pass)

#include "fpga_dma.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const char* what) {
    g_checks++;
    if (!cond) {
        g_failures++;
        printf("  [FAIL] %s\n", what);
    }
}

bool nearly(float a, float b) {
    return (a - b) < 0.001f && (b - a) < 0.001f;
}

// ── Minimal synthetic memory for tests ────────────────────────────────
// Mirrors the layout built by host_sim's Cs2MemoryImage.

constexpr uint64_t kPS          = 0x80000000ULL;  // 2 GiB (fits 1GB-aligned frame)
constexpr uint64_t kPml4        = 0x00100000;
constexpr uint64_t kPdpUser     = 0x00101000;
constexpr uint64_t kPdClient    = 0x00102000;
constexpr uint64_t kPdData      = 0x00103000;
constexpr uint64_t kPtEntity    = 0x00104000;
constexpr uint64_t kPtCtrl      = 0x00105000;
constexpr uint64_t kPtPawn      = 0x00106000;

constexpr uint64_t kClientVa    = 0x180000000ULL;
constexpr uint64_t kEntityListVa = 0x1C0000000ULL;
constexpr uint64_t kControllerVa = 0x1D0000000ULL;
constexpr uint64_t kPawnVa      = 0x1E0000000ULL;
constexpr uint64_t kHuge1GVa    = 0x80000000ULL;   // 1GB PDPE.PS mapping

constexpr uint64_t kClientPa    = 0x00200000;
constexpr uint64_t kEprocessPa  = 0x00180000;
constexpr uint64_t kEntityListPa = 0x11000000;
constexpr uint64_t kControllerPa = 0x12000000;
constexpr uint64_t kPawnPa      = 0x13000000;
constexpr uint64_t kHuge1GPa    = 0x40000000ULL;  // 1GB-aligned phys base

class TestMemory {
public:
    TestMemory() : m_mem(std::make_unique<uint8_t[]>(kPS)) {
        std::memset(m_mem.get(), 0, kPS);
        build_page_tables();
        build_client_dll();
        build_eprocess();
        build_entities();
    }

    bool phys_read(uint64_t pa, void* buf, size_t sz) const {
        if (pa + sz > kPS) return false;
        std::memcpy(buf, m_mem.get() + pa, sz);
        return true;
    }

    uint64_t cr3() const { return kPml4; }
    uint32_t pid() const { return 1234; }

    static bool phys_cb(uint64_t pa, void* buf, size_t sz) {
        return s_inst && s_inst->phys_read(pa, buf, sz);
    }
    static TestMemory* s_inst;

private:
    std::unique_ptr<uint8_t[]> m_mem;

    void set_u64(uint64_t pa, uint64_t v) {
        if (pa + 8 <= kPS) std::memcpy(m_mem.get() + pa, &v, 8);
    }
    void set_u32(uint64_t pa, uint32_t v) {
        if (pa + 4 <= kPS) std::memcpy(m_mem.get() + pa, &v, 4);
    }
    void set_u8(uint64_t pa, uint8_t v) {
        if (pa + 1 <= kPS) std::memcpy(m_mem.get() + pa, &v, 1);
    }
    void set_floats(uint64_t pa, const float* f, size_t n) {
        if (pa + n * 4 <= kPS) std::memcpy(m_mem.get() + pa, f, n * 4);
    }

    void build_page_tables() {
        set_u64(kPml4 + 0 * 8, kPdpUser | 0x07);
        set_u64(kPml4 + 0x1FF * 8, 0x00107000 | 0x03);
        // 1GB large page: VA 0x80000000 → PA 0x40000000 (PDPE.PS=1, 1GB-aligned)
        set_u64(kPdpUser + 2 * 8, kHuge1GPa | 0x87);
        set_u64(kPdpUser + 6 * 8, kPdClient | 0x07);
        set_u64(kPdpUser + 7 * 8, kPdData | 0x07);
        for (uint32_t j = 0; j < 19; ++j) {
            set_u64(kPdClient + j * 8, (kClientPa + j * 0x200000) | 0x87);
        }
        set_u64(kPdData + 0 * 8, kPtEntity | 0x07);
        set_u64(kPdData + 0x80 * 8, kPtCtrl | 0x07);
        set_u64(kPdData + 0x100 * 8, kPtPawn | 0x07);
        for (uint32_t j = 0; j < 4; ++j) {
            set_u64(kPtEntity + j * 8, (kEntityListPa + j * 0x1000) | 0x07);
        }
        for (uint32_t j = 0; j < 64; ++j) {
            set_u64(kPtCtrl + j * 8, (kControllerPa + j * 0x1000) | 0x07);
            set_u64(kPtPawn + j * 8, (kPawnPa + j * 0x1000) | 0x07);
        }
        // Identifiable marker at base of 1GB mapping
        set_u64(kHuge1GPa, 0x1CEB0001C0FFEULL);
        set_u64(kHuge1GPa + 0xABCDE0, 0x1CEB00ABCDEULL);
    }

    void build_client_dll() {
        m_mem[kClientPa] = 'M';
        m_mem[kClientPa + 1] = 'Z';
        const uint64_t rva = Cs2Offsets::dec(Cs2Offsets::dwEntityList);
        set_u64(kClientPa + rva, kEntityListVa);
    }

    void build_eprocess() {
        const char* name = "cs2.exe";
        std::memcpy(m_mem.get() + kEprocessPa + 0x2B8, name, 8);
        set_u32(kEprocessPa + 0x2E0, pid());
        set_u64(kEprocessPa + 0x28, kPml4);
        std::memcpy(m_mem.get() + kEprocessPa + 0x3E0, name, 8);
        set_u32(kEprocessPa + 0x408, pid());
    }

    void build_entities() {
        const uint64_t stride = Cs2Offsets::dec(Cs2Offsets::kIdentityStride);
        struct T { int h, t; float ox, oy, oz, yaw; float vx, vy, vz; };
        const T pts[5] = {
            {100, 3, 123.4f,  56.7f, 1.2f,  45.0f,  10.0f, -5.0f, 0.0f},
            { 85, 3, 456.7f, 123.4f, 0.5f, 120.0f, -20.0f,  3.0f, 0.0f},
            { 67, 3, 789.1f, 234.5f, 2.1f, 200.0f,   5.0f, 30.0f, 0.0f},
            { 42, 2, 111.2f, 333.4f, 1.8f, 300.0f,   0.0f, 15.0f, 0.0f},
            { 91, 2, 555.6f, 777.8f, 0.9f,  15.0f,   0.0f,  0.0f, 0.0f},
        };
        for (int i = 0; i < 5; ++i) {
            const uint64_t ctrl_pa = kControllerPa + (uint64_t)i * 0x1000;
            const uint64_t pawn_pa = kPawnPa + (uint64_t)i * 0x1000;
            set_u64(kEntityListPa + (uint64_t)i * stride, kControllerVa + (uint64_t)i * 0x1000);
            set_u64(kEntityListPa + (uint64_t)(64 + i) * stride, kPawnVa + (uint64_t)i * 0x1000);
            set_u8(ctrl_pa + Cs2Offsets::dec(Cs2Offsets::m_iTeamNum), (uint8_t)pts[i].t);
            set_u64(ctrl_pa + Cs2Offsets::dec(Cs2Offsets::m_hPlayerPawn),
                    (0xAAAAULL << 32) | static_cast<uint64_t>(64 + i));
            set_u32(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_iHealth), (uint32_t)pts[i].h);
            float org[3] = {pts[i].ox, pts[i].oy, pts[i].oz};
            set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vOldOrigin), org, 3);
            float ea[3] = {0.0f, pts[i].yaw, 0.0f};
            set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_angEyeAngles), ea, 3);
            float vel[3] = {pts[i].vx, pts[i].vy, pts[i].vz};
            set_floats(pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vecVelocity), vel, 3);
        }
    }
};
TestMemory* TestMemory::s_inst = nullptr;

// ── Tests ─────────────────────────────────────────────────────────────

void test_page_translation() {
    printf("test_page_translation\n");
    TestMemory mem;
    TestMemory::s_inst = &mem;
    auto pr = [](uint64_t pa, void* b, size_t s) -> bool {
        return TestMemory::s_inst->phys_read(pa, b, s);
    };

    // 2MB page: client.dll
    check(PageTableWalker::translate(kClientVa, mem.cr3(), pr) == kClientPa,
          "2MB page: client.dll VA -> PA");
    // offset within 2MB page
    check(PageTableWalker::translate(kClientVa + 0x1234, mem.cr3(), pr) ==
          kClientPa + 0x1234, "2MB page: offset preserved");

    // 4KB page: entity list
    check(PageTableWalker::translate(kEntityListVa, mem.cr3(), pr) == kEntityListPa,
          "4KB page: entity list VA -> PA");
    check(PageTableWalker::translate(kEntityListVa + 0xFFF, mem.cr3(), pr) ==
          kEntityListPa + 0xFFF, "4KB page: within-page offset");

    // controllers / pawns
    check(PageTableWalker::translate(kControllerVa, mem.cr3(), pr) == kControllerPa,
          "4KB page: controller VA -> PA");
    check(PageTableWalker::translate(kPawnVa + 3 * 0x1000, mem.cr3(), pr) ==
          kPawnPa + 3 * 0x1000, "4KB page: pawn index 3");

    // 1GB large page (PDPE.PS)
    check(PageTableWalker::translate(kHuge1GVa, mem.cr3(), pr) == kHuge1GPa,
          "1GB page: base VA -> PA");
    check(PageTableWalker::translate(kHuge1GVa + 0x12345678ULL, mem.cr3(), pr) ==
          kHuge1GPa + 0x12345678ULL, "1GB page: offset preserved");
    check(PageTableWalker::translate(kHuge1GVa + 0xABCDE0, mem.cr3(), pr) ==
          kHuge1GPa + 0xABCDE0, "1GB page: mid-page VA -> PA");

    // unmapped VA
    check(PageTableWalker::translate(0x000000000000ULL, mem.cr3(), pr) == 0,
          "unmapped VA (null) -> 0");
    check(PageTableWalker::translate(0x0000800000000000ULL, mem.cr3(), pr) == 0,
          "unmapped VA (non-canonical) -> 0");
    check(PageTableWalker::translate(0x7FF000000000ULL, mem.cr3(), pr) == 0,
          "unmapped VA -> 0");

    // null read callback
    check(PageTableWalker::translate(kClientVa, mem.cr3(), nullptr) == 0,
          "null phys_read_fn -> 0");
}

void test_read_virtual() {
    printf("test_read_virtual\n");
    TestMemory mem;
    TestMemory::s_inst = &mem;
    auto pr = [](uint64_t pa, void* b, size_t s) -> bool {
        return TestMemory::s_inst->phys_read(pa, b, s);
    };

    // Read 8 bytes at entity list VA (page-start aligned) → controller VA pointer
    uint64_t slot0 = 0;
    bool ok = PageTableWalker::read_virtual(kEntityListVa, &slot0, 8, mem.cr3(), pr);
    check(ok && slot0 == kControllerVa, "read_virtual reads stored controller VA");

    // Cross-page read: 8 bytes at offset 0xFFC
    uint64_t crossed = 0;
    ok = PageTableWalker::read_virtual(kEntityListVa + 0xFFC, &crossed, 8, mem.cr3(), pr);
    // first 4 bytes come from page 0, next 4 from page 1 (both mapped, zeroed there)
    check(ok && crossed == 0, "read_virtual crosses page boundary");

    // 1GB-page virtual read of marker at base
    uint64_t marker = 0;
    ok = PageTableWalker::read_virtual(kHuge1GVa, &marker, 8, mem.cr3(), pr);
    check(ok && marker == 0x1CEB0001C0FFEULL, "read_virtual via 1GB page");

    // Unmapped read fails
    uint64_t dummy = 0;
    ok = PageTableWalker::read_virtual(0x7FF000000000ULL, &dummy, 8, mem.cr3(), pr);
    check(!ok, "read_virtual on unmapped VA fails");
}

void test_eprocess_scan() {
    printf("test_eprocess_scan\n");
    TestMemory mem;
    TestMemory::s_inst = &mem;
    auto pr = [](uint64_t pa, void* b, size_t s) -> bool {
        return TestMemory::s_inst->phys_read(pa, b, s);
    };

    uint64_t cr3 = PageTableWalker::find_process_cr3(mem.pid(), pr);
    check(cr3 == mem.cr3(), "find_process_cr3 returns CR3");
    check(cr3 != 0, "find_process_cr3 nonzero");

    // Wrong PID -> not found
    check(PageTableWalker::find_process_cr3(9999, pr) == 0,
          "find_process_cr3 unknown PID -> 0");
}

void test_entity_read() {
    printf("test_entity_read\n");
    TestMemory mem;
    TestMemory::s_inst = &mem;
    auto pr = [](uint64_t pa, void* b, size_t s) -> bool {
        return TestMemory::s_inst->phys_read(pa, b, s);
    };

    DmaEntityData entities[64];
    size_t n = Cs2DmaReader::read_entities(pr, mem.pid(), kClientPa, mem.cr3(),
                                           entities, 64);
    check(n == 5, "read_entities finds 5 valid players");

    check(n >= 1 && entities[0].slot_index == 0, "player0 slot index");
    check(n >= 1 && entities[0].health == 100, "player0 health");
    check(n >= 1 && entities[0].team == 3, "player0 team");
    check(n >= 1 && nearly(entities[0].origin_x, 123.4f), "player0 origin_x");
    check(n >= 1 && nearly(entities[0].origin_y, 56.7f), "player0 origin_y");
    check(n >= 1 && nearly(entities[0].origin_z, 1.2f), "player0 origin_z");
    check(n >= 1 && nearly(entities[0].yaw, 45.0f), "player0 yaw");
    check(n >= 1 && nearly(entities[0].velocity_x, 10.0f), "player0 velocity_x");
    check(n >= 1 && nearly(entities[0].velocity_y, -5.0f), "player0 velocity_y");
    check(n >= 1 && nearly(entities[0].velocity_z, 0.0f), "player0 velocity_z");

    check(n >= 2 && entities[1].health == 85, "player1 health");
    check(n >= 2 && nearly(entities[1].yaw, 120.0f), "player1 yaw");
    check(n >= 4 && entities[3].team == 2, "player3 team (T)");
    check(n >= 5 && entities[4].health == 91, "player4 health");
    check(n >= 1 && entities[0].pawn_valid == 1, "player0 pawn_valid");
    check(n >= 1 && entities[0].controller_pa != 0, "player0 controller_pa");
    check(n >= 1 && entities[0].pawn_pa != 0, "player0 pawn_pa");

    // Single-entity path (same shipped entry point)
    DmaEntityData single{};
    uint64_t el_pa = PageTableWalker::translate(kEntityListVa, mem.cr3(), pr);
    check(el_pa == kEntityListPa, "entity list PA via walker");
    check(Cs2DmaReader::read_single_entity(pr, mem.cr3(), el_pa, 0, single),
          "read_single_entity slot 0");
    check(single.health == 100 && single.team == 3, "single entity fields");

    // Sizing
    DmaEntityData one{};
    check(sizeof(one) == 60, "DmaEntityData is 60 bytes");
    check(sizeof(DmaDescriptor) == 36, "DmaDescriptor is 36 bytes");
    check(sizeof(DmaEntityRequest) == 40, "DmaEntityRequest is 40 bytes");
}

// ── FPGA register map / descriptor DMA (mirrors host_sim FpgaDevice) ──
struct TestFpga {
    uint32_t version = DMA_FW_VERSION;
    uint32_t status = STATUS_READY;
    uint32_t ctrl = 0;
    uint32_t completed = 0;
    uint32_t dma_st = DMA_STATUS_IDLE;
    uint32_t dma_err = DMA_ERR_NONE;
    uint64_t desc_addr = 0;
    uint32_t desc_count = 0;
    uint64_t total_bytes = 0;
    std::array<uint8_t, DATA_BUFFER_SIZE> db{};
    bool iommu = false;
    TestMemory* mem = nullptr;

    void wr(uint64_t off, uint32_t v) {
        switch (off) {
        case REG_CTRL: ctrl = v; break;
        case REG_DESC_ADDR_LO:
            desc_addr = (desc_addr & ~0xFFFFFFFFULL) | v; break;
        case REG_DESC_ADDR_HI:
            desc_addr = (desc_addr & 0xFFFFFFFFULL) |
                        (static_cast<uint64_t>(v) << 32); break;
        case REG_DESC_COUNT: desc_count = v; break;
        case REG_DMA_CTRL: if (v == 1) run_dma(); break;
        case REG_DMA_ABORT:
            dma_st = DMA_STATUS_IDLE;
            status &= ~(static_cast<uint32_t>(STATUS_DMA_BUSY |
                        STATUS_DMA_DONE | STATUS_DMA_ERROR));
            break;
        default: break;
        }
    }

    uint32_t rd(uint64_t off) {
        switch (off) {
        case REG_VERSION: return version;
        case REG_STATUS: return status;
        case REG_CTRL: return ctrl;
        case REG_DMA_STATUS: return dma_st;
        case REG_DMA_ERROR: return dma_err;
        case REG_DESC_COMPLETED: return completed;
        case REG_DESC_COUNT: return desc_count;
        case REG_BYTES_TRANSFERRED: return static_cast<uint32_t>(total_bytes);
        case REG_IOMMU_STATE: return iommu ? STATUS_IOMMU : 0;
        default:
            if (off >= REG_DATA_BUFFER && off < REG_DATA_BUFFER + DATA_BUFFER_SIZE)
                return db[off - REG_DATA_BUFFER];
            return 0;
        }
    }

    void run_dma(const DmaDescriptor* ring = nullptr, uint32_t count = 0) {
        if (iommu) {
            dma_err = DMA_ERR_IOMMU;
            dma_st = DMA_STATUS_ERROR;
            status |= STATUS_DMA_ERROR;
            return;
        }
        if (!ring) ring = reinterpret_cast<const DmaDescriptor*>(
            static_cast<uintptr_t>(desc_addr));
        const uint32_t n = count ? count : desc_count;
        if (!ring || n == 0) {
            dma_err = DMA_ERR_BAD_DESC;
            dma_st = DMA_STATUS_ERROR;
            status |= STATUS_DMA_ERROR;
            return;
        }
        status |= STATUS_DMA_BUSY;
        dma_st = DMA_STATUS_BUSY;
        completed = 0;
        dma_err = DMA_ERR_NONE;
        for (uint32_t i = 0; i < n; ++i) {
            const auto& d = ring[i];
            if (d.flags & DMAF_NULL) continue;
            if ((d.flags & DMAF_READ) && d.size && mem) {
                if (d.size > DATA_BUFFER_SIZE) { dma_err = DMA_ERR_BAD_DESC; break; }
                std::vector<uint8_t> tmp(d.size);
                if (!mem->phys_read(d.src_addr, tmp.data(), d.size)) {
                    dma_err = DMA_ERR_MASTER_ABORT;
                    break;
                }
                const uint64_t dst = d.dst_addr < DATA_BUFFER_SIZE ? d.dst_addr : 0;
                std::memcpy(db.data() + dst, tmp.data(), d.size);
                total_bytes += d.size;
            }
            completed++;
            if (d.flags & DMAF_LAST) break;
        }
        dma_st = (dma_err == DMA_ERR_NONE) ? DMA_STATUS_DONE : DMA_STATUS_ERROR;
        status = (status & ~static_cast<uint32_t>(STATUS_DMA_BUSY)) |
                 ((dma_err == DMA_ERR_NONE) ? STATUS_DMA_DONE : STATUS_DMA_ERROR);
    }
};

void test_register_map() {
    printf("test_register_map\n");
    TestMemory mem;
    TestFpga fpga;
    fpga.mem = &mem;

    // Version / status registers read back (shipped BAR map)
    check(fpga.rd(REG_VERSION) == DMA_FW_VERSION, "REG_VERSION == fw version");
    check(fpga.rd(REG_STATUS) == STATUS_READY, "REG_STATUS == READY");
    check(fpga.rd(REG_DMA_STATUS) == DMA_STATUS_IDLE, "REG_DMA_STATUS == IDLE");
    check(fpga.rd(REG_DMA_ERROR) == DMA_ERR_NONE, "REG_DMA_ERROR == NONE");
    check(fpga.rd(REG_DESC_COMPLETED) == 0, "REG_DESC_COMPLETED == 0");
    check(fpga.rd(REG_DATA_BUFFER) == 0, "REG_DATA_BUFFER reads 0");
    check(REG_BASE_SHIFT == 0x80, "REG_BASE_SHIFT hardens map at 0x80");
    check(REG_VERSION == 0x80, "REG_VERSION at shifted base");
    check(DMA_VENDOR_ID == 0x1B73, "spoofed Fresco Logic VID");
    check(DMA_DEVICE_ID == 0xA0B0, "randomized DID");

    // All offsets unique & in range (sanity)
    const uint64_t regs[] = { REG_VERSION, REG_SCRATCH, REG_STATUS, REG_CTRL,
        REG_DESC_ADDR_LO, REG_DESC_ADDR_HI, REG_DESC_COUNT, REG_DESC_COMPLETED,
        REG_DMA_CTRL, REG_DMA_ABORT, REG_DMA_STATUS, REG_DMA_ERROR,
        REG_ACS_BYPASS, REG_IOMMU_STATE, REG_DMA_LATENCY, REG_TIMESTAMP,
        REG_BYTES_TRANSFERRED, REG_DESC_ERROR_IDX };
    bool unique = true;
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i)
        for (size_t j = i + 1; j < sizeof(regs) / sizeof(regs[0]); ++j)
            if (regs[i] == regs[j]) unique = false;
    check(unique, "register offsets unique");
    for (uint64_t r : regs) check(r < REG_DATA_BUFFER, "control regs below data buffer");
    check(REG_DATA_BUFFER + DATA_BUFFER_SIZE <= 0x20000,
          "data buffer fits within BAR0");

    // Descriptor-ring bulk DMA of entity list first 8 bytes
    DmaDescriptor ring[2] = {
        { kEntityListPa, 0, 8, DMAF_READ | DMAF_CHAIN, 0, 0 },
        { 0, 0, 0, DMAF_LAST, 0, 0 },
    };
    fpga.wr(REG_DESC_COUNT, 2);
    fpga.run_dma(ring, 2);
    check(fpga.rd(REG_DMA_STATUS) == DMA_STATUS_DONE, "DMA completes DONE");
    check(fpga.rd(REG_DESC_COMPLETED) == 2, "two descriptors completed");
    check(fpga.total_bytes == 8, "transferred 8 bytes");
    // First entity slot stores controller VA 0x1D0000000 little-endian
    uint64_t got = 0;
    std::memcpy(&got, fpga.db.data(), 8);
    check(got == kControllerVa, "descriptor DMA payload is controller VA");

    // IOMMU blocks DMA
    TestFpga blocked;
    blocked.iommu = true;
    blocked.mem = &mem;
    blocked.run_dma(ring, 2);
    check(blocked.rd(REG_DMA_ERROR) == DMA_ERR_IOMMU, "IOMMU sets DMA_ERR_IOMMU");
    check(blocked.rd(REG_DMA_STATUS) == DMA_STATUS_ERROR, "IOMMU DMA status ERROR");
}

void test_offsets_roundtrip() {
    printf("test_offsets_roundtrip\n");
    check(Cs2Offsets::dec(Cs2Offsets::dwEntityList) == 0x254EE60ULL,
          "dwEntityList decrypts to 0x254EE60");
    check(Cs2Offsets::dec(Cs2Offsets::m_iHealth) == 0x34CULL,
          "m_iHealth decrypts to 0x34C");
    check(Cs2Offsets::dec(Cs2Offsets::kIdentityStride) == 0x70ULL,
          "kIdentityStride decrypts to 0x70");
    check(Cs2Offsets::dec(Cs2Offsets::m_hPlayerPawn) == 0x914ULL,
          "m_hPlayerPawn decrypts to 0x914");
}

}  // namespace

int main() {
    printf("T4 PCIe DMA firmware unit tests\n");
    printf("===============================\n\n");

    test_page_translation();
    test_read_virtual();
    test_eprocess_scan();
    test_entity_read();
    test_register_map();
    test_offsets_roundtrip();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
