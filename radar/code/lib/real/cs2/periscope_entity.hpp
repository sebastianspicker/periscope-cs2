// periscope_entity.hpp — Periscope-level entity collector.
//
// Dual-representation entity model matching CS2:
//   CCSPlayerController -> lobby/scoreboard (team, name)
//   C_CSPlayerPawn -> in-world pawn (origin, health, angles)
//
// All reads go through the hijack reader. 64-slot fixed array.
// Multi-source yaw fusion with origin hold across bad frames.
//
// Reference: Periscope prototype/src/cs2/entity.hpp (219 lines)
//            Periscope prototype/src/cs2/entity_local.cpp (282 lines)

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <span>
#include <optional>
#include <array>

namespace real::cs2::periscope {

// ====================================================================
// Math types (shared with periscope_hud.hpp via include guard)
// ====================================================================
#ifndef LR_PERISCOPE_MATH_TYPES
#define LR_PERISCOPE_MATH_TYPES
struct Vector3 {
    float x{}, y{}, z{};
    float length_sqr() const noexcept { return x*x + y*y + z*z; }
};

struct Vector2 {
    float x{}, y{};
};
#endif

// ====================================================================
// Player data structure (28 fields, Periscope-compatible)
// ====================================================================
struct PlayerData {
    uint64_t controllerAddr{};
    uint64_t pawnAddr{};

    // Controller fields
    int team{};

    // Pawn fields
    Vector3 origin{};
    Vector3 absOrigin{};
    float health{};
    float yaw{};
    Vector3 eyeAngles{};
    Vector3 velocity{};
    int weaponId{};

    // State
    bool alive{false};
    bool dormant{false};
    bool spotted{false};
    bool valid{false};

    // Motion history (4-slot ring)
    Vector3 posHistory[4]{};
    int historyIndex{};

    // Enrichment
    float bombTimer{};
};

struct LocalPlayerData {
    PlayerData player{};
    Vector3 viewAngles{};
    float yaw{};         // Best-guess yaw after multi-source fusion
    uint64_t pawnAddr{};
    bool valid{};
};

struct BombData {
    bool planted{};
    Vector3 position{};
    float timer{};
};

// ====================================================================
// Yaw fusion state (multi-source with hold-over)
// ====================================================================
struct YawState {
    float heldYaw{};
    int badFrameCount{};
    static constexpr int kMaxBadFrames = 5;

    void update(float newYaw, bool valid) noexcept;
    float get() const noexcept;
};

// ====================================================================
// Origin hold state
// ====================================================================
struct OriginState {
    Vector3 lastGood{};
    bool hasLast{};

    Vector3 hold(const Vector3& current, bool valid) noexcept;
};

// ====================================================================
// Entity collector
// ====================================================================
class EntityCollector {
public:
    static constexpr size_t kMaxPlayers = 64;

    /// Collect all players from CS2 entity list.
    /// entityListAddr: resolved dwEntityList address
    /// readFn: hijack reader callback
    /// returns number of valid players found
    size_t collect(uint64_t entityListAddr,
                   bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;

    /// Get collected players.
    std::span<const PlayerData> players() const noexcept {
        return {m_players.data(), m_playerCount};
    }

    /// Get local player data.
    const LocalPlayerData& local() const noexcept { return m_local; }

    /// Get bomb data.
    const BombData& bomb() const noexcept { return m_bomb; }

    /// Set and collect local player data.
    /// Call after collect() with the resolved local pawn address and
    /// view angles/CSGOInput addresses for multi-source yaw fusion.
    /// Returns true if local player data is valid.
    bool set_local_player(uint64_t pawnAddr, uint64_t viewAnglesAddr,
                           uint64_t csgoInputAddr,
                           bool (*readFn)(uint64_t addr, void* buf,
                                          size_t size)) noexcept;

    /// Set and collect bomb data from dwPlantedC4 address.
    /// Returns true if C4 is planted.
    bool set_bomb_data(uint64_t bombAddr,
                        bool (*readFn)(uint64_t addr, void* buf,
                                       size_t size)) noexcept;

    /// Reset for next frame.
    void reset() noexcept;

private:
    std::array<PlayerData, kMaxPlayers> m_players{};
    size_t m_playerCount{};
    LocalPlayerData m_local{};
    BombData m_bomb{};
    YawState m_yawState{};
    OriginState m_originState{};

    bool collect_controller(uint64_t addr, PlayerData& out,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
    bool collect_pawn(uint64_t addr, PlayerData& out,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
    bool collect_local(uint64_t pawnAddr, uint64_t viewAnglesAddr,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
    bool collect_bomb(uint64_t bombAddr,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;

    // Multi-source yaw probes
    float probe_yaw_viewangles(uint64_t viewAnglesAddr,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
    float probe_yaw_csgoinput(uint64_t csgoInputAddr,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
    float probe_yaw_eyeangles(uint64_t pawnAddr,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
};

} // namespace real::cs2::periscope
