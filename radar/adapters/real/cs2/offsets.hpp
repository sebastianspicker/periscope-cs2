// offsets.hpp — Real CS2 offset resolution via signature scanning.
//
// LESSON: Game updates change offsets. Cheats must either:
//   (a) Scan memory for byte signatures (AOB pattern scan)
//   (b) Fetch from a remote C2 / schema SaaS
//   (c) Parse the game's schema system (dump Offset tables)
//
// This file provides option (a): pattern scanning of the real cs2.exe
// binary to resolve entity list, radar, and other critical offsets.
//
// Educational design:
//   REAL MODE:   Scans the real cs2.exe process memory for signatures.
//   SIM MODE:    Uses the pre-configured offsets from cs2/offsets.hpp
//                against sim::World's synthetic memory layout.
//   Both modes teach the same offset-resolution lesson.

#pragma once

#include "real/error.hpp"
#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::cs2 {

/// A single resolved offset value.
struct ResolvedOffset {
  const char* name;       ///< Human-readable name (e.g., "entity_list")
  std::uint64_t value;    ///< Resolved offset or address
  bool resolved;          ///< Whether scanning succeeded

  std::string describe() const;
};

/// Collection of all CS2 offsets needed for radar operation.
/// These mirror the educational offsets in cs2/offsets.hpp but are
/// resolved from the REAL cs2.exe process at runtime.
struct Cs2Offsets {
  // ── Entity System ──────────────────────────────────────────────
  std::uint64_t entity_list = 0;         ///< dwEntityList
  std::uint64_t entity_list_entry = 0;   ///< Entity list entry stride
  std::uint64_t entity_controller = 0;   ///< CCSGO_Entity -> CCSGO_Controller
  std::uint64_t entity_controller_pawn = 0; ///< Controller -> Pawn
  std::uint64_t entity_team = 0;         ///< m_iTeamNum
  std::uint64_t entity_health = 0;       ///< m_iHealth
  std::uint64_t entity_lifestate = 0;    ///< m_lifeState
  std::uint64_t entity_origin = 0;       ///< m_vOldOrigin
  std::uint64_t entity_viewangles = 0;   ///< m_angEyeAngles

  // ── Radar ──────────────────────────────────────────────────────
  std::uint64_t radar_base = 0;          ///< dwRadarBase
  std::uint64_t radar_pos_x = 0;         ///< Radar position X
  std::uint64_t radar_pos_y = 0;         ///< Radar position Y
  std::uint64_t radar_scale = 0;         ///< Radar scale
  std::uint64_t radar_size = 0;          ///< Radar map size

  // ── Game State ─────────────────────────────────────────────────
  std::uint64_t local_player = 0;        ///< dwLocalPlayer
  std::uint64_t game_state = 0;          ///< dwGameState
  std::uint64_t force_attack = 0;        ///< dwForceAttack
  std::uint64_t force_jump = 0;          ///< dwForceJump
  std::uint64_t sensitivity = 0;         ///< m_flSensitivity
  std::uint64_t view_matrix = 0;         ///< dwViewMatrix
  std::uint64_t view_angles = 0;         ///< dwViewAngles

  /// Whether all critical offsets have been resolved.
  bool is_complete() const;

  /// Human-readable dump of all offsets (educational output).
  std::string describe() const;
};

/// Signature pattern with mask.
struct Pattern {
  std::string name;
  std::vector<std::uint8_t> bytes;
  std::string mask;              ///< 'x' = match, '?' = wildcard
  int32_t additional_offset = 0; ///< Add after resolved address
  bool relative = false;         ///< Resolve relative call/jmp

  /// Build a mask from the bytes (0x00 = wildcard, non-zero = match).
  static Pattern from_bytes(const std::string& name,
                            const std::vector<std::uint8_t>& pattern,
                            int32_t add_offset = 0,
                            bool is_relative = false);
};

/// Resolve all CS2 offsets via pattern scanning of the real process.
/// Reads cs2.exe memory, finds signatures, calculates final addresses.
Result<Cs2Offsets> resolve_offsets(std::uint32_t cs2_pid,
                                    std::uint64_t cs2_base,
                                    std::size_t cs2_image_size);

/// Resolve using client.dll when available (CS2 Source2 entity signatures).
Result<Cs2Offsets> resolve_offsets_for_process(std::uint32_t cs2_pid,
                                               std::uint64_t process_handle,
                                               std::uint64_t main_base,
                                               std::size_t main_image_size);

/// Apply embedded/runtime dump snapshot: absolute addresses = client_base + RVA.
/// Schema field offsets are filled from the snapshot. Preferred path for live CS2.
Cs2Offsets offsets_from_snapshot(std::uint64_t client_base);

/// Try loading a bounded, validated runtime JSON snapshot. The explicit
/// LR_CS2_OFFSETS_PATH environment path is the normal channel. Historical CWD
/// discovery is disabled unless LR_CS2_ALLOW_LEGACY_CWD_SNAPSHOT=1; failure
/// leaves the embedded snapshot in use.
bool try_load_runtime_snapshot_file(Cs2Offsets& out_fields_only);

/// Locate client.dll base/size inside the process. Returns false if missing.
bool find_client_module(std::uint32_t cs2_pid, std::uint64_t process_handle,
                        std::uint64_t& out_base, std::size_t& out_size);

/// Locate any module by exact basename (e.g. "engine2.dll", "tier0.dll").
bool find_module_by_basename(std::uint32_t cs2_pid, std::uint64_t process_handle,
                             const char* basename, std::uint64_t& out_base,
                             std::size_t& out_size);

/// Resolve a single pattern in the process memory.
Result<std::uint64_t> scan_pattern(std::uint32_t pid,
                                    std::uint64_t base,
                                    std::size_t image_size,
                                    const Pattern& pattern);

/// Pre-defined signature set for CS2 (actively maintained).
/// These correspond to the educational offsets in cs2/offsets.hpp.
const std::vector<Pattern>& default_patterns();

/// Resolve offsets from the educational cs2/offsets.hpp when in SIM mode.
/// Returns the offsets hardcoded in the sim headers.
Cs2Offsets sim_offsets();

}  // namespace real::cs2
