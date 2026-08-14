// gdrv_pack.hpp — Host-side ABI packing helpers (no WDK, no driver load).
//
// These call into the same layout definitions as the kernel driver and
// usermode client. Host unit tests drive THESE functions — not a parallel
// re-implementation of the wire format.

#pragma once

#include "gdrv_abi.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace gdrv {

/// Pack PHYS_READ / PHYS_WRITE header. For PHYS_WRITE, append payload after.
inline std::vector<std::uint8_t> pack_phys_req(std::uint64_t phys_addr, std::uint32_t size) {
    GDRV_PHYS_REQ req{};
    req.phys_addr = phys_addr;
    req.size = size;
    req.reserved = 0;
    std::vector<std::uint8_t> out(sizeof(req));
    std::memcpy(out.data(), &req, sizeof(req));
    return out;
}

/// Pack VIRT_READ / VIRT_WRITE request. output_buffer is a usermode pointer value.
inline std::vector<std::uint8_t> pack_virt_req(std::uint32_t pid, std::uint64_t address,
                                               std::uint32_t size, std::uint64_t output_buffer) {
    GDRV_VIRT_REQ req{};
    req.process_id = pid;
    req.target_address = address;
    req.output_buffer = output_buffer;
    req.size = size;
    req.flags = 0;
    std::vector<std::uint8_t> out(sizeof(req));
    std::memcpy(out.data(), &req, sizeof(req));
    return out;
}

/// Pack ENTITY_WALK request header.
inline std::vector<std::uint8_t> pack_entity_walk_req(
    std::uint32_t pid, std::uint64_t entity_list, std::uint64_t health_off,
    std::uint64_t team_off, std::uint64_t origin_off, std::uint64_t pawn_off,
    std::uint32_t start_index, std::uint32_t count, std::uint64_t stride) {
    GDRV_ENTITY_WALK_REQ req{};
    req.process_id = pid;
    req.entity_list_addr = entity_list;
    req.schema_health = health_off;
    req.schema_team = team_off;
    req.schema_origin = origin_off;
    req.schema_pawn = pawn_off;
    req.start_index = start_index;
    req.count = count;
    req.stride = stride;
    std::vector<std::uint8_t> out(sizeof(req));
    std::memcpy(out.data(), &req, sizeof(req));
    return out;
}

/// Pack CALLBACK_STRIP request.
inline std::vector<std::uint8_t> pack_callback_strip_req(std::uint32_t type,
                                                         std::uint32_t action) {
    GDRV_CALLBACK_STRIP_REQ req{};
    req.callback_type = type;
    req.action = action;
    req.count = 0;
    req.status_hint = 0;
    std::vector<std::uint8_t> out(sizeof(req));
    std::memcpy(out.data(), &req, sizeof(req));
    return out;
}

/// Pack MODULE_LIST request header.
inline std::vector<std::uint8_t> pack_module_list_req(std::uint32_t pid,
                                                      std::uint32_t max_modules) {
    GDRV_MODULE_LIST_REQ req{};
    req.process_id = pid;
    req.max_modules = max_modules;
    req.count = 0;
    std::vector<std::uint8_t> out(sizeof(req));
    std::memcpy(out.data(), &req, sizeof(req));
    return out;
}

/// Decode PHYS header from a packed buffer. Returns false if too small.
inline bool decode_phys_req(const std::uint8_t* data, std::size_t len,
                            GDRV_PHYS_REQ* out) {
    if (!data || !out || len < sizeof(GDRV_PHYS_REQ)) return false;
    std::memcpy(out, data, sizeof(GDRV_PHYS_REQ));
    return true;
}

/// Decode VIRT header from a packed buffer.
inline bool decode_virt_req(const std::uint8_t* data, std::size_t len,
                            GDRV_VIRT_REQ* out) {
    if (!data || !out || len < sizeof(GDRV_VIRT_REQ)) return false;
    std::memcpy(out, data, sizeof(GDRV_VIRT_REQ));
    return true;
}

/// Return the canonical device path opened by the usermode client.
inline const char* usermode_device_path() { return GDRV_USERMODE_PATH_A; }

/// IOCTL code at index [0, IOCTL_GDRV_COUNT).
inline std::uint32_t ioctl_code_at(unsigned index) {
    static const std::uint32_t kCodes[IOCTL_GDRV_COUNT] = {
        IOCTL_GDRV_PHYS_READ,      IOCTL_GDRV_PHYS_WRITE, IOCTL_GDRV_VIRT_READ,
        IOCTL_GDRV_VIRT_WRITE,     IOCTL_GDRV_ENTITY_WALK, IOCTL_GDRV_PROCESS_SCAN,
        IOCTL_GDRV_CALLBACK_STRIP, IOCTL_GDRV_MODULE_LIST,
    };
    return (index < IOCTL_GDRV_COUNT) ? kCodes[index] : 0;
}

// ── Attach-safe staging layout (mirrors kernel post-detach publish) ──
// Kernel VIRT/ENTITY/MODULE paths stage into pool while attached, then
// publish into the caller buffer only after KeUnstackDetachProcess.
// These helpers are the same byte math the driver uses for that publish.

inline std::size_t entity_stage_bytes(std::uint32_t count) {
    return static_cast<std::size_t>(count) * sizeof(GDRV_ENTITY_DATA);
}

inline std::size_t module_stage_bytes(std::uint32_t count) {
    return static_cast<std::size_t>(count) * sizeof(GDRV_MODULE_ENTRY);
}

/// Publish staged entity records after the ENTITY_WALK request header.
/// Returns false if the output buffer is too small.
inline bool publish_entity_stage(const GDRV_ENTITY_DATA* stage, std::uint32_t count,
                                 std::uint8_t* out, std::size_t out_len) {
    if (!stage || !out) return false;
    const std::size_t need = sizeof(GDRV_ENTITY_WALK_REQ) + entity_stage_bytes(count);
    if (out_len < need) return false;
    std::memcpy(out + sizeof(GDRV_ENTITY_WALK_REQ), stage, entity_stage_bytes(count));
    return true;
}

/// Publish MODULE_LIST header + staged module entries (caller AS only).
inline bool publish_module_stage(const GDRV_MODULE_LIST_REQ& hdr,
                                 const GDRV_MODULE_ENTRY* stage, std::uint32_t count,
                                 std::uint8_t* out, std::size_t out_len) {
    if (!out) return false;
    if (count > 0 && !stage) return false;
    const std::size_t need = sizeof(GDRV_MODULE_LIST_REQ) + module_stage_bytes(count);
    if (out_len < need) return false;
    GDRV_MODULE_LIST_REQ h = hdr;
    h.count = count;
    std::memcpy(out, &h, sizeof(h));
    if (count > 0) {
        std::memcpy(out + sizeof(h), stage, module_stage_bytes(count));
    }
    return true;
}

/// Pure model of attach-safe VIRT_READ staging: target bytes land in a
/// kernel-side stage, then are published to the caller output buffer.
/// Host tests drive this to lock the protocol the driver implements.
inline bool virt_read_publish_stage(const std::uint8_t* stage, std::uint32_t size,
                                    std::uint8_t* caller_out, std::size_t out_cap) {
    if (!stage || !caller_out || size == 0) return false;
    if (out_cap < size) return false;
    std::memcpy(caller_out, stage, size);
    return true;
}

}  // namespace gdrv
