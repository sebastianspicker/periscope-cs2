// fpga_dma.h — T4 PCIe DMA interface + CS2 entity reader (hardened, complete).
//
// Full-stack header for the FPGA-based CS2 memory reader:
//   - PCIe device identity (hardened, randomized VID/DID)
//   - BAR0 register map (128KB, 64-bit non-prefetchable)
//   - DMA descriptor ring + flags
//   - CS2 offsets (XOR-encrypted, Periscope-validated)
//   - DmaEntityData / DmaEntityRequest wire structs
//   - PageTableWalker (4-level x86-64 VA->PA translation)
//   - Cs2DmaReader (entity list walk entirely via physical reads)
//
// FIRMWARE VERSION: 0x20260726 (build-date-derived)
//
// Reference:
//   Periscope prototype/src/cs2/entity.hpp
//   Periscope prototype/src/cs2/offsets.hpp
//   anti-cheat-legit-radar code/lib/real/dma/

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <array>
#include <type_traits>

// ═══════════════════════════════════════════════════════════════════════
// PCIe DEVICE IDENTIFICATION
// ═══════════════════════════════════════════════════════════════════════

#define DMA_VENDOR_ID         0x1B73   // Fresco Logic (spoofed vendor)
#define DMA_DEVICE_ID         0xA0B0   // Randomized device ID
#define DMA_REVISION          0x02
#define DMA_CLASS_CODE        0x058000 // DMA controller
#define DMA_FW_VERSION        0x20260726

// Real DMA hardware timing
#define DMA_GEN2_RATE         5.0      // GT/s per lane
#define DMA_LANES             4        // x4 configuration
#define DMA_READ_4KB_US       2.5f     // Avg 4KB read latency
#define DMA_DESC_FETCH_US     0.8f     // Avg descriptor fetch latency
#define DMA_MAX_PAYLOAD       256      // Max PCIe payload bytes

// ═══════════════════════════════════════════════════════════════════════
// BAR0 REGISTER MAP (128KB, 64-bit non-prefetchable)
// ═══════════════════════════════════════════════════════════════════════
// Randomized register offsets — custom layout, not PCILeech.
// All registers are 32 bits wide unless noted.

// Register base shift to randomize register map offsets
#define REG_BASE_SHIFT      0x0080

// Identification
#define REG_VERSION         (0x0000 + REG_BASE_SHIFT)  // RO: Firmware version (0x20260726)
#define REG_SCRATCH         (0x0004 + REG_BASE_SHIFT)  // RW: Scratch/test register
#define REG_STATUS          (0x0008 + REG_BASE_SHIFT)  // RO: Status flags
#define REG_CTRL            (0x000C + REG_BASE_SHIFT)  // RW: Control flags

// Descriptor ring
#define REG_DESC_ADDR_LO    (0x0010 + REG_BASE_SHIFT)  // RW: Desc ring phys addr (low 32)
#define REG_DESC_ADDR_HI    (0x0014 + REG_BASE_SHIFT)  // RW: Desc ring phys addr (high 32)
#define REG_DESC_COUNT      (0x0018 + REG_BASE_SHIFT)  // RW: Number of descriptors
#define REG_DESC_COMPLETED  (0x001C + REG_BASE_SHIFT)  // RO: Completed descriptor count

// DMA engine
#define REG_DMA_CTRL        (0x0020 + REG_BASE_SHIFT)  // WO: DMA engine control (doorbell)
#define REG_DMA_ABORT       (0x0024 + REG_BASE_SHIFT)  // WO: Abort running DMA
#define REG_DMA_STATUS      (0x0028 + REG_BASE_SHIFT)  // RO: DMA engine status
#define REG_DMA_ERROR       (0x002C + REG_BASE_SHIFT)  // RO: Error code

// Platform features
#define REG_ACS_BYPASS      (0x0030 + REG_BASE_SHIFT)  // RW: ACS bypass control
#define REG_IOMMU_STATE     (0x0034 + REG_BASE_SHIFT)  // RO: IOMMU detection result

// Performance monitoring
#define REG_DMA_LATENCY     (0x0038 + REG_BASE_SHIFT)  // RO: Last DMA latency (us)
#define REG_TIMESTAMP       (0x003C + REG_BASE_SHIFT)  // RO: Free-running us timer
#define REG_BYTES_TRANSFERRED (0x0040 + REG_BASE_SHIFT) // RO: Total bytes transferred
#define REG_DESC_ERROR_IDX  (0x0044 + REG_BASE_SHIFT)  // RO: Descriptor index that caused error

// Buffer
#define REG_DATA_BUFFER     (0x1000 + REG_BASE_SHIFT)  // RW: 64KB data buffer
#define DATA_BUFFER_SIZE    0x10000

// Aliases for host_sim compatibility
#define REG_FPGA_VERSION    REG_VERSION
#define REG_FPGA_SCRATCH    REG_SCRATCH
#define REG_FPGA_STATUS     REG_STATUS
#define REG_FPGA_CTRL       REG_CTRL

// ═══════════════════════════════════════════════════════════════════════
// STATUS / CONTROL FLAGS
// ═══════════════════════════════════════════════════════════════════════

// REG_STATUS
#define STATUS_READY        0x00000001
#define STATUS_DMA_BUSY     0x00000002
#define STATUS_DMA_DONE     0x00000004
#define STATUS_DMA_ERROR    0x00000008
#define STATUS_IOMMU        0x00010000

// REG_DMA_STATUS
#define DMA_STATUS_IDLE     0x00000000
#define DMA_STATUS_BUSY     0x00000001
#define DMA_STATUS_DONE     0x00000002
#define DMA_STATUS_ERROR    0x00000003

// REG_CTRL
#define CTRL_DMA_ENABLE     0x00000001
#define CTRL_LOOP           0x00000002
#define CTRL_INT_ENABLE     0x00000004
#define CTRL_ACS_BYPASS     0x00000008
#define CTRL_FULL_RESET     0x80000000

// DMA error codes (REG_DMA_ERROR)
#define DMA_ERR_NONE        0x00000000
#define DMA_ERR_IOMMU       0x00000001
#define DMA_ERR_TIMEOUT     0x00000002
#define DMA_ERR_MASTER_ABORT 0x00000003
#define DMA_ERR_BAD_DESC    0x00000004
#define DMA_ERR_ACS         0x00000005

// ═══════════════════════════════════════════════════════════════════════
// DMA DESCRIPTOR (36-byte, randomized — non-standard size)
// ═══════════════════════════════════════════════════════════════════════

#pragma pack(push, 1)
struct DmaDescriptor {
    uint64_t src_addr;       // Source physical address
    uint64_t dst_addr;       // Destination address (FPGA buffer or host)
    uint32_t size;           // Transfer size (bytes, max 4096)
    uint32_t flags;          // Control flags
    uint64_t user_data;      // User-defined data (e.g., descriptor ID)
    uint32_t padding;        // Anti-fingerprint: changes descriptor size from 32→36 bytes
};
static_assert(sizeof(DmaDescriptor) == 36, "Randomized descriptors=36B (was 32B)");
#pragma pack(pop)

// DMA descriptor flags
#define DMAF_READ           0x00000001  // Direction: FPGA reads from host
#define DMAF_WRITE          0x00000002  // Direction: FPGA writes to host
#define DMAF_CHAIN          0x00000004  // Continue to next descriptor
#define DMAF_LAST           0x00000008  // Last descriptor in ring
#define DMAF_NULL           0x00000010  // Skip this descriptor
#define DMAF_INT_ON_COMP    0x00000020  // Interrupt on completion
#define DMAF_SCRAMBLE       0x00000040  // XOR-scramble data (anti-scan)

// ═══════════════════════════════════════════════════════════════════════
// CS2 T4 DMA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════
// Reference: Periscope prototype/src/cs2/entity.hpp
//            Periscope prototype/src/cs2/offsets.hpp

// Periscope-validated CS2 offsets (RVA within client.dll).
// All values are XOR-encrypted with kKey to defeat static scanning.
struct Cs2Offsets {
    static constexpr uint64_t kKey                  = 0xA5A5A5A5A5A5A5A5ULL;
    static constexpr uint64_t dwEntityList            = 0x254EE60ULL ^ kKey;
    static constexpr uint64_t dwLocalPlayerPawn       = 0x23A4238ULL ^ kKey;
    static constexpr uint64_t dwLocalPlayerController = 0x237EBA0ULL ^ kKey;
    static constexpr uint64_t dwViewAngles            = 0x23B9C78ULL ^ kKey;
    static constexpr uint64_t dwCSGOInput             = 0x23B95F0ULL ^ kKey;
    static constexpr uint64_t dwPlantedC4             = 0x236E678ULL ^ kKey;
    static constexpr uint64_t kIdentityStride         = 0x70ULL ^ kKey;
    static constexpr uint64_t kMaxPlayers             = 64ULL ^ kKey;

    // Schema field offsets (within C_BaseEntity / CCSPlayerController / C_CSPlayerPawn)
    static constexpr uint64_t m_iHealth              = 0x34CULL ^ kKey;
    static constexpr uint64_t m_iTeamNum             = 0x3E7ULL ^ kKey;  // uint8
    static constexpr uint64_t m_vOldOrigin           = 0x13B8ULL ^ kKey;
    static constexpr uint64_t m_angEyeAngles         = 0x3340ULL ^ kKey;
    static constexpr uint64_t m_hPlayerPawn          = 0x914ULL ^ kKey;
    static constexpr uint64_t m_pGameSceneNode       = 0x330ULL ^ kKey;
    static constexpr uint64_t m_iWeaponID            = 0x10B0ULL ^ kKey;
    static constexpr uint64_t m_vecVelocity          = 0x1170ULL ^ kKey;

    // Decrypt helper: XOR with kKey to restore original value
    static constexpr uint64_t dec(uint64_t v) noexcept { return v ^ kKey; }
    // Encrypt helper
    static constexpr uint64_t enc(uint64_t v) noexcept { return v ^ kKey; }
};

// A single player entity read via DMA (60 bytes, packed)
#pragma pack(push, 1)
struct DmaEntityData {
    uint32_t slot_index;         // Entity list slot
    uint32_t health;             // 0 = invalid/dead
    uint32_t team;               // 2 = T, 3 = CT
    uint32_t pawn_valid;         // 1 if pawn was resolved
    float    origin_x;           // World position
    float    origin_y;
    float    origin_z;
    float    yaw;                // Eye angle yaw
    uint64_t controller_pa;      // Physical address of controller
    uint64_t pawn_pa;            // Physical address of pawn
    float    velocity_x;         // Movement speed
    float    velocity_y;
    float    velocity_z;

    bool valid() const noexcept {
        return health > 0 && health <= 100 && pawn_valid != 0;
    }
};
static_assert(sizeof(DmaEntityData) == 60,
              "DmaEntityData must be 60 bytes (packed)");
#pragma pack(pop)

// DMA entity read request (host -> FPGA)
#pragma pack(push, 1)
struct DmaEntityRequest {
    uint64_t cs2_cr3;             // CR3 of CS2 process (for page walk)
    uint64_t client_dll_base_pa;  // Physical address of client.dll
    uint64_t entity_list_pa;      // Physical address of entity list ptr
    uint32_t start_slot;          // First slot to read
    uint32_t slot_count;          // Number of slots (max 64)
    uint32_t stride;              // Entity list stride (0x70)
    uint32_t flags;               // Request flags
};
static_assert(sizeof(DmaEntityRequest) == 40,
              "DmaEntityRequest must be 40 bytes (packed)");
#pragma pack(pop)

// ═══════════════════════════════════════════════════════════════════════
// PHYSICAL READ CALLBACK
// ═══════════════════════════════════════════════════════════════════════
// All DMA reads funnel through this callback — the FPGA performs a PCIe
// memory read TLP to the target physical address. There is NO OS call.

using PhysReadFn = bool (*)(uint64_t phys_addr, void* buf, size_t size);

// ═══════════════════════════════════════════════════════════════════════
// x86-64 PAGE TABLE WALKER (for DMA VA→PA translation)
// ═══════════════════════════════════════════════════════════════════════
// Reference: Intel SDM Vol 3A, Chapter 4: Paging

struct PageTableWalker {
    // Walk 4-level x86-64 page tables to translate VA → PA
    static uint64_t translate(uint64_t virt_addr, uint64_t cr3,
                              PhysReadFn phys_read_fn) noexcept;

    // Read `size` bytes starting at virtual address via DMA (handles page
    // crossing). Returns false on any unmapped page or read failure.
    static bool read_virtual(uint64_t virt_addr, void* buf, size_t size,
                             uint64_t cr3, PhysReadFn phys_read_fn) noexcept;

    // Find CS2's EPROCESS by scanning physical memory for its PID + name
    static uint64_t find_process_cr3(uint32_t target_pid,
                                     PhysReadFn phys_read_fn) noexcept;

    // Find the physical address of the EPROCESS block for target_pid
    static uint64_t find_eprocess(uint32_t target_pid,
                                  PhysReadFn phys_read_fn) noexcept;

    // Page table constants (Intel SDM Vol 3A Ch.4 — 4-level paging, MAXPHYADDR=52)
    static constexpr int PML4_SHIFT = 39;
    static constexpr int PDP_SHIFT  = 30;
    static constexpr int PD_SHIFT   = 21;
    static constexpr int PT_SHIFT   = 12;
    static constexpr uint64_t PAGE_MASK       = 0x000FFFFFFFFFF000ULL;  // bits 51:12 (4KB)
    static constexpr uint64_t LARGE_PAGE_MASK = 0x000FFFFFFFFFE00000ULL; // bits 51:21 (2MB)
    static constexpr uint64_t ONE_GB_MASK     = 0x000FFFFFC0000000ULL;  // bits 51:30 (1GB)
    static constexpr uint64_t PAGE_PRESENT   = 0x001ULL;
    static constexpr uint64_t PAGE_RW        = 0x002ULL;
    static constexpr uint64_t PAGE_USER      = 0x004ULL;
    static constexpr uint64_t PAGE_PS        = 0x080ULL;  // Page Size (1GB/2MB)
    static constexpr uint64_t PAGE_NX        = 0x8000000000000000ULL;
    static constexpr uint64_t PAGE_SIZE_4K   = 0x1000ULL;
    static constexpr uint64_t PAGE_SIZE_2M   = 0x200000ULL;
    static constexpr uint64_t PAGE_SIZE_1G   = 0x40000000ULL;
};

// ═══════════════════════════════════════════════════════════════════════
// CS2 DMA ENTITY READER
// ═══════════════════════════════════════════════════════════════════════
// Reads CS2 entity data via PCIe DMA. No OS interaction required.
//
// Flow:
//   1. Find CS2's CR3 by scanning physical memory for EPROCESS
//   2. Translate client.dll base VA → PA via page walk
//   3. Read entity list pointer from client.dll at dwEntityList offset
//   4. Translate entity list VA → PA
//   5. Walk 64 entity slots at stride 0x70
//   6. For each slot: read controller address, resolve pawn via handle
//   7. Read health, team, origin from controller + pawn
//   8. Return DmaEntityData array

struct Cs2DmaReader {
    // Read CS2 entities via DMA
    static size_t read_entities(
        PhysReadFn phys_read_fn,
        uint32_t cs2_pid,
        uint64_t client_base_pa,
        uint64_t cs2_cr3,
        DmaEntityData* out_entities,
        size_t max_entities = 64) noexcept;

    // Read a single entity from a slot
    // returns: true if entity was valid (health > 0)
    static bool read_single_entity(
        PhysReadFn phys_read_fn,
        uint64_t cs2_cr3,
        uint64_t entity_list_pa,
        uint32_t slot_index,
        DmaEntityData& out) noexcept;

    // Resolve pawn physical address from a controller's m_hPlayerPawn handle
    static uint64_t resolve_pawn(
        PhysReadFn phys_read_fn,
        uint64_t cs2_cr3,
        uint64_t entity_list_pa,
        uint64_t pawn_handle) noexcept;
};
