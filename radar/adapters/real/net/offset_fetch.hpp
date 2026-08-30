#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <string>

namespace real::net {

struct OffsetData {
    uint64_t dwEntityList = 0;
    uint64_t dwLocalPlayer = 0;
    uint64_t dwViewMatrix = 0;
    uint64_t dwViewAngles = 0;
    uint64_t dwGlowObjectManager = 0;
    uint64_t dwSensitivity = 0;
    uint64_t dwSensitivitySensitivity = 0;
    uint64_t dwForceJump = 0;
    uint64_t dwForceAttack = 0;
    uint64_t dwPlayerResource = 0;
    uint32_t cs2Version = 0;
    uint32_t checksum = 0;
};

// Fetch offsets from remote server over HTTPS with encrypted payload.
// Decrypts, verifies integrity via checksum, and stores result in encrypted memory.
std::optional<OffsetData> fetch_offsets(const char* endpoint);

// Verify offsets match an expected CS2 version.
// Returns false if a version mismatch is detected (offsets likely stale).
bool verify_offsets(const OffsetData& offsets, uint32_t expected_cs2_version);

} // namespace real::net
