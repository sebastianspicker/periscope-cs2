// cs2_dma_reader.cpp — T4 CS2 entity list walk via PCIe DMA.
//
// Reads CS2 entity data entirely through PCIe DMA physical memory reads.
// No OS calls, no kernel driver, no process handle.
// The reader runs on a separate machine connected via PCIe FPGA.
//
// Flow:
//   1. client.dll base PA is known (found via PEB walk in a real setup,
//      or passed by the caller — host_sim demonstrates both paths).
//   2. Read the entity list pointer: physical read at
//      client_base_pa + dwEntityList (client.dll image is mapped as an
//      image; the RVA offset is a physical displacement in the PE).
//   3. Translate the entity list VA → PA via page table walk.
//   4. Walk 64 slots (stride 0x70):
//        slot[i]  → controller pointer (VA)
//        controller + m_hPlayerPawn → handle
//        pawn = (handle & 0x7FFF) * stride + entity_list
//   5. Read health, team, origin, angles from controller + pawn.
//
// Reference: Periscope prototype/src/cs2/entity.hpp entity.cpp

#include "fpga_dma.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════
// Helpers — all reads are physical (DMA), all addresses are physical
// ═══════════════════════════════════════════════════════════════════════

namespace {

template<typename T>
bool dma_read(PhysReadFn fn, uint64_t pa, T& out) {
    return fn(pa, &out, sizeof(T));
}

// Read a virtual address via DMA (page walk + phys read). Handles
// reads that cross page boundaries. Kept as a local helper for callers
// that prefer not to go through PageTableWalker::read_virtual.
[[maybe_unused]] bool dma_read_virtual(PhysReadFn fn, uint64_t cr3,
                      uint64_t va, void* buf, size_t size) noexcept {
    return PageTableWalker::read_virtual(va, buf, size, cr3, fn);
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════
// Resolve pawn physical address from a controller's m_hPlayerPawn handle
// ═══════════════════════════════════════════════════════════════════════
// Handle format (CHandle in CS2/Source2):
//   bits [31:16] = serial  (ignored here)
//   bits [14:0]  = entry index into the entity list
// The pawn pointer lives at: entity_list_base + index * kIdentityStride

uint64_t Cs2DmaReader::resolve_pawn(
    PhysReadFn phys_read_fn,
    uint64_t cs2_cr3,
    uint64_t entity_list_pa,
    uint64_t pawn_handle) noexcept
{
    if (!phys_read_fn || !pawn_handle) return 0;

    const uint64_t stride = Cs2Offsets::dec(Cs2Offsets::kIdentityStride);
    const uint32_t index  = static_cast<uint32_t>(pawn_handle & 0x7FFF);
    const uint64_t entry_pa = entity_list_pa + static_cast<uint64_t>(index) * stride;

    uint64_t pawn_ptr = 0;
    if (!dma_read(phys_read_fn, entry_pa, pawn_ptr)) return 0;
    if (!pawn_ptr) return 0;

    // pawn_ptr is a VA in the target process — translate to PA.
    return PageTableWalker::translate(pawn_ptr, cs2_cr3, phys_read_fn);
}

// ═══════════════════════════════════════════════════════════════════════
// Read a single CS2 entity from the entity list
// ═══════════════════════════════════════════════════════════════════════

bool Cs2DmaReader::read_single_entity(
    PhysReadFn phys_read_fn,
    uint64_t cs2_cr3,
    uint64_t entity_list_pa,
    uint32_t slot_index,
    DmaEntityData& out) noexcept
{
    std::memset(&out, 0, sizeof(out));
    out.slot_index = slot_index;

    if (!phys_read_fn || !entity_list_pa) return false;

    const uint64_t stride = Cs2Offsets::dec(Cs2Offsets::kIdentityStride);

    // 1. Controller pointer (VA) stored in the entity list slot.
    uint64_t controller_va = 0;
    uint64_t entry_pa = entity_list_pa + static_cast<uint64_t>(slot_index) * stride;
    if (!dma_read(phys_read_fn, entry_pa, controller_va)) return false;
    if (!controller_va) return false;   // empty slot

    // 2. Translate controller VA → PA.
    uint64_t controller_pa = PageTableWalker::translate(
        controller_va, cs2_cr3, phys_read_fn);
    if (!controller_pa) return false;

    out.controller_pa = controller_pa;

    // 3. Team number (uint8 at m_iTeamNum).
    uint8_t team = 0;
    if (!dma_read(phys_read_fn,
                  controller_pa + Cs2Offsets::dec(Cs2Offsets::m_iTeamNum), team))
        return false;
    out.team = team;

    // 4. Pawn handle (uint64 at m_hPlayerPawn).
    uint64_t pawn_handle = 0;
    if (!dma_read(phys_read_fn,
                  controller_pa + Cs2Offsets::dec(Cs2Offsets::m_hPlayerPawn),
                  pawn_handle))
        return false;
    if (!pawn_handle) return false;

    // 5. Resolve pawn PA.
    uint64_t pawn_pa = resolve_pawn(phys_read_fn, cs2_cr3, entity_list_pa, pawn_handle);
    if (!pawn_pa) return false;
    out.pawn_pa = pawn_pa;
    out.pawn_valid = 1;

    // 6. Health (int32 at m_iHealth).
    int32_t health = 0;
    if (!dma_read(phys_read_fn,
                  pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_iHealth), health))
        return false;
    out.health = static_cast<uint32_t>(health);

    // Only consider valid if health is in a sane range.
    if (health <= 0 || health > 100) return false;

    // 7. Old origin (3 floats at m_vOldOrigin).
    float origin[3] = {0, 0, 0};
    if (!dma_read(phys_read_fn,
                  pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vOldOrigin), origin))
        return false;
    out.origin_x = origin[0];
    out.origin_y = origin[1];
    out.origin_z = origin[2];

    // 8. Eye angles (3 floats at m_angEyeAngles) → yaw is [1].
    float eye_angles[3] = {0, 0, 0};
    if (dma_read(phys_read_fn,
                 pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_angEyeAngles), eye_angles)) {
        out.yaw = eye_angles[1];
    }

    // 9. Velocity (3 floats at m_vecVelocity).
    float velocity[3] = {0, 0, 0};
    if (dma_read(phys_read_fn,
                 pawn_pa + Cs2Offsets::dec(Cs2Offsets::m_vecVelocity), velocity)) {
        out.velocity_x = velocity[0];
        out.velocity_y = velocity[1];
        out.velocity_z = velocity[2];
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Read all CS2 entities
// ═══════════════════════════════════════════════════════════════════════

size_t Cs2DmaReader::read_entities(
    PhysReadFn phys_read_fn,
    uint32_t cs2_pid,
    uint64_t client_base_pa,
    uint64_t cs2_cr3,
    DmaEntityData* out_entities,
    size_t max_entities) noexcept
{
    if (!phys_read_fn || !out_entities || max_entities == 0)
        return 0;

    // Optional: if CR3 is unknown (0), discover it from the EPROCESS scan.
    // host_sim usually supplies CR3 already; real firmware may only know the PID.
    uint64_t cr3 = cs2_cr3;
    if (!cr3 && cs2_pid) {
        cr3 = PageTableWalker::find_process_cr3(cs2_pid, phys_read_fn);
        if (!cr3) return 0;
    }
    if (!cr3) return 0;

    // ── 1. Locate the entity list pointer ──
    // client_base_pa is the physical base of the client.dll image in the
    // target. The dwEntityList symbol is an RVA — the pointer lives at
    // client_base_pa + dwEntityList in physical memory.
    uint64_t entity_list_va = 0;
    if (!dma_read(phys_read_fn,
                  client_base_pa + Cs2Offsets::dec(Cs2Offsets::dwEntityList),
                  entity_list_va)) {
        return 0;
    }
    if (!entity_list_va) return 0;

    // ── 2. Translate entity list VA → PA ──
    uint64_t entity_list_pa = PageTableWalker::translate(
        entity_list_va, cr3, phys_read_fn);
    if (!entity_list_pa) return 0;

    // ── 3. Walk the entity slots ──
    size_t valid_count = 0;
    const uint32_t max_players =
        static_cast<uint32_t>(Cs2Offsets::dec(Cs2Offsets::kMaxPlayers));

    for (uint32_t i = 0; i < max_players && valid_count < max_entities; ++i) {
        DmaEntityData entity{};
        if (read_single_entity(phys_read_fn, cr3, entity_list_pa, i, entity)) {
            out_entities[valid_count++] = entity;
        }
    }

    return valid_count;
}
