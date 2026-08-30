// periscope_entity.cpp — Periscope-level entity collector implementation.

#include "real/cs2/periscope_entity.hpp"
#include "real/cs2/periscope_scanner.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

// __rdtsc / read_timestamp
#if LR_COMPILER_MSVC
#  include <intrin.h>
#else
#  include <x86intrin.h>
#endif

static inline uint64_t read_timestamp() noexcept {
#if LR_COMPILER_MSVC
    unsigned int aux;
    return __rdtscp(&aux);
#else
    unsigned int aux;
    return __builtin_ia32_rdtscp(&aux);
#endif
}

namespace real::cs2::periscope {

// ====================================================================
// YawState
// ====================================================================

void YawState::update(float newYaw, bool valid) noexcept {
    if (valid && !std::isnan(newYaw) &&
        newYaw >= -360.0f && newYaw <= 360.0f) {
        heldYaw = newYaw;
        badFrameCount = 0;
    } else {
        ++badFrameCount;
        if (badFrameCount > kMaxBadFrames) {
            heldYaw = 0.0f;
        }
    }
}

float YawState::get() const noexcept {
    return heldYaw;
}

// ====================================================================
// OriginState
// ====================================================================

Vector3 OriginState::hold(const Vector3& current, bool valid) noexcept {
    if (valid && current.length_sqr() > 0.01f) {
        lastGood = current;
        hasLast = true;
        return current;
    }
    if (hasLast) {
        return lastGood;
    }
    return current;
}

// ====================================================================
// EntityCollector — private helpers
// ====================================================================

bool EntityCollector::collect_controller(uint64_t addr, PlayerData& out,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    uint8_t team;
    if (!readFn(addr + 0x3E7, &team, sizeof(team)))
        return false;

    out.team = static_cast<int>(team);
    out.controllerAddr = addr;
    return true;
}

bool EntityCollector::collect_pawn(uint64_t addr, PlayerData& out,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Batch read health, origin, eye angles
    struct PawnData {
        float health;
        uint8_t pad1[0x13B8 - 0x34C - 4];
        Vector3 origin;
        uint8_t pad2[0x3340 - 0x13B8 - 12];
        Vector3 eyeAngles;
    };

    PawnData data;
    if (!readFn(addr + 0x34C, &data, sizeof(data)))
        return false;

    out.health = data.health;
    out.origin = data.origin;
    out.eyeAngles = data.eyeAngles;
    out.pawnAddr = addr;
    out.alive = (data.health > 0.0f && data.health <= 100.0f);
    return true;
}

bool EntityCollector::collect_local(uint64_t pawnAddr, uint64_t viewAnglesAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!pawnAddr) return false;

    PlayerData local{};
    if (!collect_pawn(pawnAddr, local, readFn))
        return false;

    m_local.player = local;
    m_local.pawnAddr = pawnAddr;
    m_local.valid = local.alive;

    // Read view angles — read into local variable then assign
    Vector3 angles{};
    if (viewAnglesAddr && readFn(viewAnglesAddr, &angles, sizeof(Vector3))) {
        m_local.viewAngles = angles;
    }

    return m_local.valid;
}

bool EntityCollector::collect_bomb(uint64_t bombAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!bombAddr) return false;

    struct BombDataRaw {
        uint8_t pad1[0x6C];
        uint32_t bombSite;
        uint8_t pad2[4];
        uint8_t isPlanted;
        uint8_t pad3[0xB0 - 0x6C - 4 - 4 - 1];
        Vector3 origin;
        float timer;
    };

    BombDataRaw raw;
    if (!readFn(bombAddr, &raw, sizeof(raw)))
        return false;

    m_bomb.planted = (raw.isPlanted != 0);
    m_bomb.position = raw.origin;
    m_bomb.timer = raw.timer;
    return m_bomb.planted;
}

// ====================================================================
// Multi-source yaw probes
// ====================================================================

float EntityCollector::probe_yaw_viewangles(uint64_t viewAnglesAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!viewAnglesAddr) return NAN;
    Vector2 angles;
    if (!readFn(viewAnglesAddr, &angles, sizeof(angles)))
        return NAN;
    return angles.y;
}

float EntityCollector::probe_yaw_csgoinput(uint64_t csgoInputAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!csgoInputAddr) return NAN;
    // CSGOInput yaw is at offset 0x100
    float yaw;
    if (!readFn(csgoInputAddr + 0x100, &yaw, sizeof(yaw)))
        return NAN;
    return yaw;
}

float EntityCollector::probe_yaw_eyeangles(uint64_t pawnAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!pawnAddr) return NAN;
    Vector3 eyeAngles;
    if (!readFn(pawnAddr + SchemaOffsets::m_angEyeAngles,
                &eyeAngles, sizeof(eyeAngles)))
        return NAN;
    return eyeAngles.y;
}

// ====================================================================
// EntityCollector — public interface
// ====================================================================

size_t EntityCollector::collect(uint64_t entityListAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    m_playerCount = 0;

    if (!entityListAddr || !readFn) return 0;

    // ---- Anti-detection: randomized entity walk ----
    // Per-frame XorShift PRNG seed (not process-global)
    uint64_t rng_state = read_timestamp();
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;

    // Build shuffled slot index list (45-63 slots per frame)
    int indices[kMaxPlayers];
    int count = 45 + static_cast<int>(rng_state % 19);
    for (int i = 0; i < count; ++i) indices[i] = i;

    // Fisher-Yates shuffle with PRNG advancement
    for (int i = count - 1; i > 0; --i) {
        int j = static_cast<int>(rng_state % (i + 1));
        std::swap(indices[i], indices[j]);
        rng_state = rng_state * 0x5851F42D4C957F2DULL +
                    0x14057B7EF767814FULL;
    }

    // Pick 1-3 dummy slots within our selected range
    // (dummy reads break stride-prediction heuristics)
    int dummy_count = 1 + static_cast<int>(rng_state % 3);
    bool is_dummy[64] = {};
    for (int d = 0; d < dummy_count; ++d) {
        rng_state = rng_state * 0x5851F42D4C957F2DULL +
                    0x14057B7EF767814FULL;
        is_dummy[rng_state % count] = true;
    }

    // Iterate shuffled slots (non-sequential, randomized stride)
    for (int idx = 0; idx < count; ++idx) {
        uint32_t i = static_cast<uint32_t>(indices[idx]);
        uint64_t entryAddr = entityListAddr +
            static_cast<uint64_t>(i) * SchemaOffsets::kIdentityStride;

        // Dummy read: touch the slot but discard — breaks stride detection
        if (is_dummy[idx]) {
            uint64_t dummy;
            readFn(entryAddr, &dummy, sizeof(dummy));
            continue;
        }

        PlayerData player{};

        // Read controller address from slot
        uint64_t controllerAddr = 0;
        if (!readFn(entryAddr, &controllerAddr, sizeof(controllerAddr)))
            continue;
        if (!controllerAddr) continue;

        if (!collect_controller(controllerAddr, player, readFn))
            continue;

        // Read pawn handle from controller
        uint64_t pawnHandle = 0;
        if (!readFn(controllerAddr + SchemaOffsets::m_hPlayerPawn,
                    &pawnHandle, sizeof(pawnHandle)))
            continue;

        if (pawnHandle) {
            uint32_t pawnIndex = static_cast<uint32_t>(pawnHandle & 0x7FFF);
            uint64_t pawnEntryAddr = entityListAddr +
                static_cast<uint64_t>(pawnIndex) * SchemaOffsets::kIdentityStride;
            uint64_t pawnAddr = 0;
            if (readFn(pawnEntryAddr, &pawnAddr, sizeof(pawnAddr)) && pawnAddr) {
                collect_pawn(pawnAddr, player, readFn);
            }
        }

        player.valid = player.alive;

        if (player.valid) {
            m_players[m_playerCount++] = player;
        }
    }

    return m_playerCount;
}

bool EntityCollector::set_local_player(uint64_t pawnAddr,
    uint64_t viewAnglesAddr, uint64_t csgoInputAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Collect local player pawn data
    if (!collect_local(pawnAddr, viewAnglesAddr, readFn))
        return false;

    // Multi-source yaw fusion for local player
    float yawView = probe_yaw_viewangles(viewAnglesAddr, readFn);
    float yawInput = probe_yaw_csgoinput(csgoInputAddr, readFn);
    float yawEye = probe_yaw_eyeangles(pawnAddr, readFn);

    float bestYaw = NAN;
    if (!std::isnan(yawView)) bestYaw = yawView;
    else if (!std::isnan(yawInput)) bestYaw = yawInput;
    else if (!std::isnan(yawEye)) bestYaw = yawEye;

    m_local.yaw = bestYaw;
    bool yawValid = !std::isnan(bestYaw);
    m_yawState.update(bestYaw, yawValid);
    if (yawValid) {
        m_local.yaw = m_yawState.get();
    }

    return m_local.valid;
}

bool EntityCollector::set_bomb_data(uint64_t bombAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    return collect_bomb(bombAddr, readFn);
}

void EntityCollector::reset() noexcept {
    m_playerCount = 0;
    m_local = {};
    m_bomb = {};
}

} // namespace real::cs2::periscope
