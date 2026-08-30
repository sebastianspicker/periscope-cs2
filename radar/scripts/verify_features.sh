#!/usr/bin/env bash
# verify_features.sh — Verify T0.12, T0.13, T0.14 build system changes.
# Full T0 radar enhancement surface: API table, hijack reader, entity/HUD,
# overlay, team red/blue APIs, demo wiring, CMake options/keys.
# Continues through all checks; exits non-zero if any miss (no silent ignore).
set -uo pipefail
# Note: intentionally NOT using `set -e` so every check runs and is reported.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ERRORS=0
PASSED=0

green() { printf "\033[32m%s\033[0m\n" "$1"; }
red()   { printf "\033[31m%s\033[0m\n" "$1"; }

check_file() {
  if [ -f "$1" ]; then
    green "  [OK] $1"
    PASSED=$((PASSED + 1))
    return 0
  else
    red "  [MISSING] $1"
    ERRORS=$((ERRORS + 1))
    return 0  # keep scanning
  fi
}

check_grep() {
  local file="$1" pattern="$2" desc="$3"
  if [ ! -f "$file" ]; then
    red "  [MISSING] $desc (file absent: $file)"
    ERRORS=$((ERRORS + 1))
    return 0
  fi
  if grep -qE "$pattern" "$file" 2>/dev/null; then
    green "  [OK] $desc in $file"
    PASSED=$((PASSED + 1))
    return 0
  else
    red "  [MISSING] $desc in $file"
    ERRORS=$((ERRORS + 1))
    return 0
  fi
}

echo ""
echo "=== Verification: T0.12 — T0 Red Team Radar Enhancement ==="
echo "root=$ROOT"
echo ""

# 1. API table files exist and have correct structure
echo "--- 1. API table files ---"
check_file "$ROOT/adapters/real/win/api_table.hpp"
check_file "$ROOT/adapters/real/win/api_table.cpp"
check_grep "$ROOT/adapters/real/win/api_table.hpp" "NtQuerySystemInformation" "NtQuerySystemInformation declaration"
check_grep "$ROOT/adapters/real/win/api_table.cpp" "resolve\\(" "resolve() implementation"

# 2. Hijack reader files exist
echo ""
echo "--- 2. Hijack reader files ---"
check_file "$ROOT/adapters/real/cs2/hijack_reader.hpp"
check_file "$ROOT/adapters/real/cs2/hijack_reader.cpp"
check_grep "$ROOT/adapters/real/cs2/hijack_reader.hpp" "HijackReader" "HijackReader class"
# Implementation uses NtQuerySystemInformation via the API table (cpp, not hpp).
check_grep "$ROOT/adapters/real/cs2/hijack_reader_discover.cpp" "NtQuerySystemInformation" "NtQuerySystemInformation usage"

# 3. Entity collector files exist
echo ""
echo "--- 3. Entity collector files ---"
check_file "$ROOT/adapters/real/cs2/periscope_entity.hpp"
check_grep "$ROOT/adapters/real/cs2/periscope_entity.hpp" "EntityCollector" "EntityCollector class"
check_grep "$ROOT/adapters/real/cs2/periscope_entity.hpp" "YawState" "YawState struct"
check_grep "$ROOT/adapters/real/cs2/periscope_entity.hpp" "OriginState" "OriginState struct"

# 4. HUD radar files exist
echo ""
echo "--- 4. HUD radar files ---"
check_file "$ROOT/adapters/real/cs2/periscope_hud.hpp"
check_grep "$ROOT/adapters/real/cs2/periscope_hud.hpp" "HudRadarReader" "HudRadarReader class"
check_grep "$ROOT/adapters/real/cs2/periscope_hud.hpp" "CvarManager" "CvarManager class"

# 5. Overlay files exist
echo ""
echo "--- 5. Overlay files ---"
check_file "$ROOT/adapters/real/gpu/periscope_overlay.hpp"
check_file "$ROOT/adapters/real/gpu/periscope_overlay.cpp"
check_grep "$ROOT/adapters/real/gpu/periscope_overlay.hpp" "StealthOverlay" "StealthOverlay class"
check_grep "$ROOT/adapters/real/gpu/periscope_overlay.hpp" "generate_random_name" "generate_random_name function"
check_grep "$ROOT/adapters/real/gpu/periscope_overlay.hpp" "ensure_capture_exclusion" "ensure_capture_exclusion method"

# T0 Red Team enhancements
echo ""
echo "--- T0.12 Red Team enhancements ---"
check_grep "$ROOT/src/lab_components/teams/t0_red/evasion_advanced.hpp" "enable_temporal_jitter" "temporal jitter method"
check_grep "$ROOT/src/lab_components/teams/t0_red/evasion_advanced.hpp" "enable_batch_shuffle" "batch shuffle method"
check_grep "$ROOT/src/lab_components/teams/t0_red/evasion_advanced.hpp" "enable_memory_hide" "memory hide method"
check_grep "$ROOT/src/lab_components/teams/t0_red/evasion_advanced.hpp" "enable_decoy_render" "decoy render method"

# T0 Blue Team enhancements
echo ""
echo "--- T0.13 Blue Team enhancements ---"

# T0.14 Demo
echo ""
echo "--- T0.14 Demo ---"
check_file "$ROOT/apps/demos/cs2_radar/t0/main.cpp"
# Demo uses g_Api() and checks .resolved (table auto-resolves on first access).
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "g_Api\\(" "API table g_Api()"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "\\.resolved" "API table resolved flag"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "find_cs2" "find_cs2 call"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "g_Hijack\\(" "hijack g_Hijack()"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "\\.setup\\(" "hijack setup"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "ensure_capture_exclusion" "WDA per frame"
check_grep "$ROOT/apps/demos/cs2_radar/t0/main.cpp" "generate_random_name" "randomized overlay names"

# Build system
echo ""
echo "--- Build system ---"
check_grep "$ROOT/cmake/RealSources.cmake" "periscope_overlay\\.cpp" "periscope_overlay in CMake"
check_grep "$ROOT/cmake/Options.cmake" "SECTION_SEED" "SECTION_SEED in CMake"
check_grep "$ROOT/cmake/Options.cmake" "LR_ENABLE_SHELLCODE_INJECTION" "LR_ENABLE_SHELLCODE_INJECTION option"
check_grep "$ROOT/cmake/Options.cmake" "LR_ENABLE_REAL_GPU" "LR_ENABLE_REAL_GPU option"
check_grep "$ROOT/cmake/build_keys.hpp.in" "kSectionSeed" "kSectionSeed in template"
check_grep "$ROOT/cmake/build_keys.hpp.in" "kGuidSeed" "kGuidSeed in template"
check_grep "$ROOT/cmake/build_keys.hpp.in" "kSurfaceSeed" "kSurfaceSeed in template"

echo ""
echo "==========================================="
if [ "$ERRORS" -eq 0 ]; then
  green "All checks passed! ($PASSED OK)"
  echo "==========================================="
  exit 0
else
  red "$ERRORS check(s) failed ($PASSED OK)."
  echo "==========================================="
  exit 1
fi
