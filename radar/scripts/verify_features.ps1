# verify_features.ps1 - Verify T0.12 / T0.13 / T0.14 radar enhancement surface.
# Windows-native twin of verify_features.sh (same checks, full report, non-zero on miss).
#
# Usage (from code/ or anywhere):
#   powershell -ExecutionPolicy Bypass -File scripts/verify_features.ps1
#
# Exit: 0 all present, 1 one or more MISSING.

param()

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script:Errors = 0
$script:Passed = 0

function Write-Status([string]$Message) {
  Write-Information -MessageData $Message -InformationAction Continue
}

function Write-Ok([string]$msg) {
  Write-Status ("  [OK] " + $msg)
  $script:Passed++
}
function Write-Missing([string]$msg) {
  Write-Error -Message ("  [MISSING] " + $msg) -ErrorAction Continue
  $script:Errors++
}

function Check-File([string]$rel) {
  $path = Join-Path $Root $rel
  if (Test-Path -LiteralPath $path -PathType Leaf) {
    Write-Ok $rel
    return $true
  }
  Write-Missing $rel
  return $false
}

function Check-Grep {
  param(
    [string]$Rel,
    [string]$Pattern,
    [string]$Desc
  )
  $path = Join-Path $Root $Rel
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    Write-Missing ($Desc + " (file absent: " + $Rel + ")")
    return $false
  }
  $hit = Select-String -LiteralPath $path -Pattern $Pattern -Quiet
  if ($hit) {
    Write-Ok ($Desc + " in " + $Rel)
    return $true
  }
  Write-Missing ($Desc + " in " + $Rel)
  return $false
}

Write-Status ""
Write-Status "=== Verification: T0.12 - T0 Red Team Radar Enhancement ==="
Write-Status ("root=" + $Root)
Write-Status ""

# 1. API table
Write-Status "--- 1. API table files ---"
[void](Check-File "adapters/real/win/api_table.hpp")
[void](Check-File "adapters/real/win/api_table.cpp")
[void](Check-Grep -Rel "adapters/real/win/api_table.hpp" -Pattern "NtQuerySystemInformation" -Desc "NtQuerySystemInformation declaration")
[void](Check-Grep -Rel "adapters/real/win/api_table.cpp" -Pattern "resolve\(" -Desc "resolve() implementation")

# 2. Hijack reader
Write-Status ""
Write-Status "--- 2. Hijack reader files ---"
[void](Check-File "adapters/real/cs2/hijack_reader.hpp")
[void](Check-File "adapters/real/cs2/hijack_reader.cpp")
[void](Check-Grep -Rel "adapters/real/cs2/hijack_reader.hpp" -Pattern "HijackReader" -Desc "HijackReader class")
# NtQuerySystemInformation is used in the discovery split (not redeclared in the facade).
[void](Check-Grep -Rel "adapters/real/cs2/hijack_reader_discover.cpp" -Pattern "NtQuerySystemInformation" -Desc "NtQuerySystemInformation usage")

# 3. Entity collector
Write-Status ""
Write-Status "--- 3. Entity collector files ---"
[void](Check-File "adapters/real/cs2/periscope_entity.hpp")
[void](Check-Grep -Rel "adapters/real/cs2/periscope_entity.hpp" -Pattern "EntityCollector" -Desc "EntityCollector class")
[void](Check-Grep -Rel "adapters/real/cs2/periscope_entity.hpp" -Pattern "YawState" -Desc "YawState struct")
[void](Check-Grep -Rel "adapters/real/cs2/periscope_entity.hpp" -Pattern "OriginState" -Desc "OriginState struct")

# 4. HUD radar
Write-Status ""
Write-Status "--- 4. HUD radar files ---"
[void](Check-File "adapters/real/cs2/periscope_hud.hpp")
[void](Check-Grep -Rel "adapters/real/cs2/periscope_hud.hpp" -Pattern "HudRadarReader" -Desc "HudRadarReader class")
[void](Check-Grep -Rel "adapters/real/cs2/periscope_hud.hpp" -Pattern "CvarManager" -Desc "CvarManager class")

# 5. Overlay
Write-Status ""
Write-Status "--- 5. Overlay files ---"
[void](Check-File "adapters/real/gpu/periscope_overlay.hpp")
[void](Check-File "adapters/real/gpu/periscope_overlay.cpp")
[void](Check-Grep -Rel "adapters/real/gpu/periscope_overlay.hpp" -Pattern "StealthOverlay" -Desc "StealthOverlay class")
[void](Check-Grep -Rel "adapters/real/gpu/periscope_overlay.hpp" -Pattern "generate_random_name" -Desc "generate_random_name function")
[void](Check-Grep -Rel "adapters/real/gpu/periscope_overlay.hpp" -Pattern "ensure_capture_exclusion" -Desc "ensure_capture_exclusion method")

# T0 Red Team
Write-Status ""
Write-Status "--- T0.12 Red Team enhancements ---"
[void](Check-Grep -Rel "src/lab_components/teams/t0_red/evasion_advanced.hpp" -Pattern "enable_temporal_jitter" -Desc "temporal jitter method")
[void](Check-Grep -Rel "src/lab_components/teams/t0_red/evasion_advanced.hpp" -Pattern "enable_batch_shuffle" -Desc "batch shuffle method")
[void](Check-Grep -Rel "src/lab_components/teams/t0_red/evasion_advanced.hpp" -Pattern "enable_memory_hide" -Desc "memory hide method")
[void](Check-Grep -Rel "src/lab_components/teams/t0_red/evasion_advanced.hpp" -Pattern "enable_decoy_render" -Desc "decoy render method")

# T0 Blue Team
Write-Status ""
Write-Status "--- T0.13 Blue Team enhancements ---"

# T0.14 Demo
Write-Status ""
Write-Status "--- T0.14 Demo ---"
[void](Check-File "apps/demos/cs2_radar/t0/main.cpp")
# Demo resolves the API table via g_Api() and checks .resolved (auto-init).
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "g_Api\(" -Desc "API table g_Api()")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "\.resolved" -Desc "API table resolved flag")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "find_cs2" -Desc "find_cs2 call")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "g_Hijack\(" -Desc "hijack g_Hijack()")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "\.setup\(" -Desc "hijack setup")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "ensure_capture_exclusion" -Desc "WDA per frame")
[void](Check-Grep -Rel "apps/demos/cs2_radar/t0/main.cpp" -Pattern "generate_random_name" -Desc "randomized overlay names")

# Build system
Write-Status ""
Write-Status "--- Build system ---"
[void](Check-Grep -Rel "cmake/RealSources.cmake" -Pattern "periscope_overlay\.cpp" -Desc "periscope_overlay in CMake")
[void](Check-Grep -Rel "cmake/Options.cmake" -Pattern "SECTION_SEED" -Desc "SECTION_SEED in CMake")
[void](Check-Grep -Rel "cmake/Options.cmake" -Pattern "LR_ENABLE_SHELLCODE_INJECTION" -Desc "LR_ENABLE_SHELLCODE_INJECTION option")
[void](Check-Grep -Rel "cmake/Options.cmake" -Pattern "LR_ENABLE_REAL_GPU" -Desc "LR_ENABLE_REAL_GPU option")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kSectionSeed" -Desc "kSectionSeed in template")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kGuidSeed" -Desc "kGuidSeed in template")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kSurfaceSeed" -Desc "kSurfaceSeed in template")

Write-Status ""
Write-Status "==========================================="
if ($script:Errors -eq 0) {
  Write-Status ("All checks passed! (" + $script:Passed + " OK)")
  Write-Status "==========================================="
  exit 0
}
Write-Error -Message ($script:Errors.ToString() + " check(s) failed (" + $script:Passed + " OK).") -ErrorAction Continue
Write-Status "==========================================="
exit 1
