#pragma once
// Embedded CS2 offset snapshot (auto-updated by scripts/update_cs2_signatures.py).
// Values are RVAs relative to client.dll base unless noted as field offsets.
// Source: https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/
// Generated: 2026-08-09T11:12:20Z
//
// NOTE: entities.cpp / live_radar_stack.cpp require the full FIELD_MAP set
// (core + armor, dormant, spotted, bomb, observer). emit_hpp emits every key.

#include <cstddef>
#include <cstdint>

namespace real::cs2::snapshot {

inline constexpr const char* kSourceUrl =
    "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/";
inline constexpr const char* kModule = "client.dll";
inline constexpr const char* kGeneratedAt = "2026-08-09T11:12:20Z";

namespace globals {
inline constexpr std::uint64_t dwEntityList = 0x254FE70;
inline constexpr std::uint64_t dwGameEntitySystem = 0x254FE70;
inline constexpr std::uint64_t dwGameEntitySystem_highestEntityIndex = 0x2090;
inline constexpr std::uint64_t dwLocalPlayerPawn = 0x23A5238;
inline constexpr std::uint64_t dwLocalPlayerController = 0x237FB70;
inline constexpr std::uint64_t dwViewMatrix = 0x23AA340;
inline constexpr std::uint64_t dwViewAngles = 0x23BAE18;
inline constexpr std::uint64_t dwPlantedC4 = 0x236F658;
inline constexpr std::uint64_t dwCSGOInput = 0x23BA790;
inline constexpr std::uint64_t dwSensitivity = 0x23A2228;
}  // namespace globals

namespace fields {
// C_BaseEntity
inline constexpr std::uint64_t m_iHealth = 0x34C;
inline constexpr std::uint64_t m_iTeamNum = 0x3E7;
inline constexpr std::uint64_t m_lifeState = 0x354;
inline constexpr std::uint64_t m_pGameSceneNode = 0x330;
// C_BasePlayerPawn
inline constexpr std::uint64_t m_vOldOrigin = 0x13B8;
// C_CSPlayerPawn
inline constexpr std::uint64_t m_angEyeAngles = 0x3340;
// CCSPlayerController
inline constexpr std::uint64_t m_hPlayerPawn = 0x914;
// CGameSceneNode
inline constexpr std::uint64_t m_vecAbsOrigin = 0xC8;
inline constexpr std::uint64_t m_bDormant = 0x103;
// C_CSPlayerPawn
inline constexpr std::uint64_t m_ArmorValue = 0x1C9C;
inline constexpr std::uint64_t m_entitySpottedState = 0x1C58;
// EntitySpottedState_t
inline constexpr std::uint64_t m_bSpotted = 0x8;
// C_BasePlayerPawn
inline constexpr std::uint64_t m_pObserverServices = 0x1220;
// CPlayer_ObserverServices
inline constexpr std::uint64_t m_iObserverMode = 0x48;
// C_PlantedC4
inline constexpr std::uint64_t m_bBombTicking = 0x11A0;
inline constexpr std::uint64_t m_nBombSite = 0x11A4;
inline constexpr std::uint64_t m_flC4Blow = 0x11D0;
inline constexpr std::uint64_t m_bBeingDefused = 0x11DC;
}  // namespace fields

namespace constants {
inline constexpr std::uint64_t entity_identity_stride = 0x70;
inline constexpr std::uint64_t entity_chunk_table_offset = 0x10;
inline constexpr std::uint32_t entity_handle_index_mask = 0x7FFF;
inline constexpr std::uint32_t entity_chunk_shift = 9u;
}  // namespace constants

}  // namespace real::cs2::snapshot
