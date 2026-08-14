"""Shared constants for CS2 signature update modules."""
from __future__ import annotations

DEFAULT_URL_BASE = (
    "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/"
)

GLOBAL_KEYS = [
    "dwEntityList",
    "dwGameEntitySystem",
    "dwGameEntitySystem_highestEntityIndex",
    "dwLocalPlayerPawn",
    "dwLocalPlayerController",
    "dwViewMatrix",
    "dwViewAngles",
    "dwPlantedC4",
    "dwCSGOInput",
    "dwSensitivity",
]

CRITICAL_GLOBALS = ("dwEntityList", "dwLocalPlayerPawn")

FIELD_MAP = [
    ("C_BaseEntity", "m_iHealth"),
    ("C_BaseEntity", "m_iTeamNum"),
    ("C_BaseEntity", "m_lifeState"),
    ("C_BaseEntity", "m_pGameSceneNode"),
    ("C_BasePlayerPawn", "m_vOldOrigin"),
    ("C_CSPlayerPawn", "m_angEyeAngles"),
    ("CCSPlayerController", "m_hPlayerPawn"),
    ("CGameSceneNode", "m_vecAbsOrigin"),
    # Extended set required by entities.cpp / live_radar_stack.cpp
    ("CGameSceneNode", "m_bDormant"),
    ("C_CSPlayerPawn", "m_ArmorValue"),
    ("C_CSPlayerPawn", "m_entitySpottedState"),
    ("EntitySpottedState_t", "m_bSpotted"),
    ("C_BasePlayerPawn", "m_pObserverServices"),
    ("CPlayer_ObserverServices", "m_iObserverMode"),
    ("C_PlantedC4", "m_bBombTicking"),
    ("C_PlantedC4", "m_nBombSite"),
    ("C_PlantedC4", "m_flC4Blow"),
    ("C_PlantedC4", "m_bBeingDefused"),
]

ENTITY_CONSTANTS = {
    "entity_identity_stride": 0x70,
    "entity_chunk_table_offset": 0x10,
    "entity_handle_index_mask": 0x7FFF,
    "entity_chunk_shift": 9,
}
