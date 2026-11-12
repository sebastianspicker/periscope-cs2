#pragma once
// Embedded CS2 offset snapshot (auto-updated by scripts/update_cs2_signatures.py).
// Values are RVAs relative to client.dll base unless noted as field offsets.
// Source: https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/
// Generated: 2026-11-12T20:04:18Z
//
// NOTE: entities.cpp / live_radar_stack.cpp require the full FIELD_MAP set
// (core + armor, dormant, spotted, bomb, observer). emit_hpp emits every key.
//
// PINNED DUMP, November 2026. Refresh with
// radar/scripts/update_cs2_signatures.py; `--check` reports drift read-only.
// Never hand-edit values here: the updater regenerates this file wholesale.

#include <cstddef>
#include <cstdint>

#include "real/cs2/encoded_rva.hpp"

namespace real::cs2::snapshot {

inline constexpr const char* kSourceUrl =
    "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/";
inline constexpr const char* kModule = "client.dll";
inline constexpr const char* kGeneratedAt = "2026-11-12T20:04:18Z";

namespace globals {
inline constexpr std::uint64_t dwEntityList = encoded::encode(0x271FBB8);
inline constexpr std::uint64_t dwGameEntitySystem = encoded::encode(0x271FBB8);
inline constexpr std::uint64_t dwGameEntitySystem_highestEntityIndex = encoded::encode(0x2120);
inline constexpr std::uint64_t dwLocalPlayerPawn = encoded::encode(0x256A948);
inline constexpr std::uint64_t dwLocalPlayerController = encoded::encode(0x2542548);
inline constexpr std::uint64_t dwViewMatrix = encoded::encode(0x2570250);
inline constexpr std::uint64_t dwViewAngles = encoded::encode(0x2581098);
inline constexpr std::uint64_t dwPlantedC4 = encoded::encode(0x24D0B70);
inline constexpr std::uint64_t dwCSGOInput = encoded::encode(0x2580A10);
inline constexpr std::uint64_t dwSensitivity = encoded::encode(0x2567C48);
inline constexpr std::uint64_t dwGlobalVars = encoded::encode(0x2235108);
}  // namespace globals

namespace fields {
// C_BaseEntity
inline constexpr std::uint64_t m_iHealth = 0x34C;
inline constexpr std::uint64_t m_iTeamNum = 0x3E7;
inline constexpr std::uint64_t m_lifeState = 0x354;
inline constexpr std::uint64_t m_pGameSceneNode = 0x330;
// C_BasePlayerPawn
inline constexpr std::uint64_t m_vOldOrigin = 0x14A4;
// C_CSPlayerPawn
inline constexpr std::uint64_t m_angEyeAngles = 0x3600;
// CCSPlayerController
inline constexpr std::uint64_t m_hPlayerPawn = 0x92C;
// CGameSceneNode
inline constexpr std::uint64_t m_vecAbsOrigin = 0xC8;
inline constexpr std::uint64_t m_bDormant = 0x103;
// C_CSPlayerPawn
inline constexpr std::uint64_t m_ArmorValue = 0x1ED0;
inline constexpr std::uint64_t m_entitySpottedState = 0x1E88;
// EntitySpottedState_t
inline constexpr std::uint64_t m_bSpotted = 0x8;
// C_BasePlayerPawn
inline constexpr std::uint64_t m_pObserverServices = 0x1308;
// CPlayer_ObserverServices
inline constexpr std::uint64_t m_iObserverMode = 0x48;
// C_PlantedC4
inline constexpr std::uint64_t m_bBombTicking = 0x1288;
inline constexpr std::uint64_t m_nBombSite = 0x128C;
inline constexpr std::uint64_t m_flC4Blow = 0x12B8;
inline constexpr std::uint64_t m_bBeingDefused = 0x12C4;
// C_BaseEntity
inline constexpr std::uint64_t m_vecVelocity = 0x430;
inline constexpr std::uint64_t m_hOwnerEntity = 0x520;
// C_BaseModelEntity
inline constexpr std::uint64_t m_vecViewOffset = 0xF60;
// CCSPlayerController
inline constexpr std::uint64_t m_sSanitizedPlayerName = 0x878;
// C_BasePlayerPawn
inline constexpr std::uint64_t m_pWeaponServices = 0x12F0;
// CPlayer_WeaponServices
inline constexpr std::uint64_t m_hActiveWeapon = 0x60;
// C_EconEntity
inline constexpr std::uint64_t m_AttributeManager = 0x1290;
// C_AttributeContainer
inline constexpr std::uint64_t m_Item = 0x50;
// C_EconItemView
inline constexpr std::uint64_t m_iItemDefinitionIndex = 0x1BA;
// CPlayer_ObserverServices
inline constexpr std::uint64_t m_hObserverTarget = 0x4C;
}  // namespace fields

namespace constants {
inline constexpr std::uint64_t entity_identity_stride = 0x70;
inline constexpr std::uint64_t entity_chunk_table_offset = 0x10;
inline constexpr std::uint32_t entity_handle_index_mask = 0x7FFF;
inline constexpr std::uint32_t entity_chunk_shift = 9u;
}  // namespace constants

}  // namespace real::cs2::snapshot
