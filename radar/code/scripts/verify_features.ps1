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

function Write-Ok([string]$msg) {
  Write-Host ("  [OK] " + $msg) -ForegroundColor Green
  $script:Passed++
}
function Write-Missing([string]$msg) {
  Write-Host ("  [MISSING] " + $msg) -ForegroundColor Red
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

Write-Host ""
Write-Host "=== Verification: T0.12 - T0 Red Team Radar Enhancement ==="
Write-Host ("root=" + $Root)
Write-Host ""

# 1. API table
Write-Host "--- 1. API table files ---"
[void](Check-File "lib/real/win/api_table.hpp")
[void](Check-File "lib/real/win/api_table.cpp")
[void](Check-Grep -Rel "lib/real/win/api_table.hpp" -Pattern "NtQuerySystemInformation" -Desc "NtQuerySystemInformation declaration")
[void](Check-Grep -Rel "lib/real/win/api_table.cpp" -Pattern "resolve\(" -Desc "resolve() implementation")

# 2. Hijack reader
Write-Host ""
Write-Host "--- 2. Hijack reader files ---"
[void](Check-File "lib/real/cs2/hijack_reader.hpp")
[void](Check-File "lib/real/cs2/hijack_reader.cpp")
[void](Check-Grep -Rel "lib/real/cs2/hijack_reader.hpp" -Pattern "HijackReader" -Desc "HijackReader class")
# NtQuerySystemInformation is used in the .cpp implementation (not redeclared in the header).
[void](Check-Grep -Rel "lib/real/cs2/hijack_reader.cpp" -Pattern "NtQuerySystemInformation" -Desc "NtQuerySystemInformation usage")

# 3. Entity collector
Write-Host ""
Write-Host "--- 3. Entity collector files ---"
[void](Check-File "lib/real/cs2/periscope_entity.hpp")
[void](Check-Grep -Rel "lib/real/cs2/periscope_entity.hpp" -Pattern "EntityCollector" -Desc "EntityCollector class")
[void](Check-Grep -Rel "lib/real/cs2/periscope_entity.hpp" -Pattern "YawState" -Desc "YawState struct")
[void](Check-Grep -Rel "lib/real/cs2/periscope_entity.hpp" -Pattern "OriginState" -Desc "OriginState struct")

# 4. HUD radar
Write-Host ""
Write-Host "--- 4. HUD radar files ---"
[void](Check-File "lib/real/cs2/periscope_hud.hpp")
[void](Check-Grep -Rel "lib/real/cs2/periscope_hud.hpp" -Pattern "HudRadarReader" -Desc "HudRadarReader class")
[void](Check-Grep -Rel "lib/real/cs2/periscope_hud.hpp" -Pattern "CvarManager" -Desc "CvarManager class")

# 5. Overlay
Write-Host ""
Write-Host "--- 5. Overlay files ---"
[void](Check-File "lib/real/gpu/periscope_overlay.hpp")
[void](Check-File "lib/real/gpu/periscope_overlay.cpp")
[void](Check-Grep -Rel "lib/real/gpu/periscope_overlay.hpp" -Pattern "StealthOverlay" -Desc "StealthOverlay class")
[void](Check-Grep -Rel "lib/real/gpu/periscope_overlay.hpp" -Pattern "generate_random_name" -Desc "generate_random_name function")
[void](Check-Grep -Rel "lib/real/gpu/periscope_overlay.hpp" -Pattern "ensure_capture_exclusion" -Desc "ensure_capture_exclusion method")

# T0 Red Team
Write-Host ""
Write-Host "--- T0.12 Red Team enhancements ---"
[void](Check-Grep -Rel "teams/t0_red/cheat_client.hpp" -Pattern "MemoryBackend" -Desc "MemoryBackend enum")
[void](Check-Grep -Rel "teams/t0_red/cheat_client.hpp" -Pattern "attach_hijack" -Desc "attach_hijack method")
[void](Check-Grep -Rel "teams/t0_red/cheat_client.hpp" -Pattern "collect_periscope_entities" -Desc "collect_periscope_entities method")
[void](Check-Grep -Rel "teams/t0_red/entity_pipeline.hpp" -Pattern "collect_periscope" -Desc "periscope collect method")
[void](Check-Grep -Rel "teams/t0_red/entity_pipeline.hpp" -Pattern "project_to_radar" -Desc "project_to_radar method")
[void](Check-Grep -Rel "teams/t0_red/entity_pipeline.hpp" -Pattern "yaw_state" -Desc "yaw_state accessor")
[void](Check-Grep -Rel "teams/t0_red/entity_pipeline.hpp" -Pattern "origin_hold" -Desc "origin_hold accessor")
[void](Check-Grep -Rel "teams/t0_red/radar_ui.hpp" -Pattern "init_stealth_overlay" -Desc "init_stealth_overlay method")
[void](Check-Grep -Rel "teams/t0_red/radar_ui.hpp" -Pattern "ensure_wda_every_frame" -Desc "ensure_wda_every_frame method")
[void](Check-Grep -Rel "teams/t0_red/evasion_advanced.hpp" -Pattern "enable_temporal_jitter" -Desc "temporal jitter method")
[void](Check-Grep -Rel "teams/t0_red/evasion_advanced.hpp" -Pattern "enable_batch_shuffle" -Desc "batch shuffle method")
[void](Check-Grep -Rel "teams/t0_red/evasion_advanced.hpp" -Pattern "enable_memory_hide" -Desc "memory hide method")
[void](Check-Grep -Rel "teams/t0_red/evasion_advanced.hpp" -Pattern "enable_decoy_render" -Desc "decoy render method")

# T0 Blue Team
Write-Host ""
Write-Host "--- T0.13 Blue Team enhancements ---"
[void](Check-Grep -Rel "teams/t0_blue/handle_graph_monitor.hpp" -Pattern "scan_real_handles" -Desc "scan_real_handles method")
[void](Check-Grep -Rel "teams/t0_blue/handle_graph_monitor.hpp" -Pattern "is_pid_legitimate" -Desc "is_pid_legitimate method")
[void](Check-Grep -Rel "teams/t0_blue/handle_graph_monitor.cpp" -Pattern "NtQuerySystemInformation" -Desc "NtQuerySystemInformation in real scan")
[void](Check-Grep -Rel "teams/t0_blue/process_cooccurrence.hpp" -Pattern "scan_real_processes" -Desc "scan_real_processes method")
[void](Check-Grep -Rel "teams/t0_blue/process_cooccurrence.hpp" -Pattern "matches_known_cheat_pattern" -Desc "cheat pattern detection")
[void](Check-Grep -Rel "teams/t0_blue/process_cooccurrence.cpp" -Pattern "CreateToolhelp32Snapshot" -Desc "CreateToolhelp32Snapshot in real scan")

# T0.14 Demo
Write-Host ""
Write-Host "--- T0.14 Demo ---"
[void](Check-File "demos/cs2_radar/t0/main.cpp")
# Demo resolves the API table via g_Api() and checks .resolved (auto-init).
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "g_Api\(" -Desc "API table g_Api()")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "\.resolved" -Desc "API table resolved flag")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "find_cs2" -Desc "find_cs2 call")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "g_Hijack\(" -Desc "hijack g_Hijack()")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "\.setup\(" -Desc "hijack setup")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "ensure_capture_exclusion" -Desc "WDA per frame")
[void](Check-Grep -Rel "demos/cs2_radar/t0/main.cpp" -Pattern "generate_random_name" -Desc "randomized overlay names")

# Build system
Write-Host ""
Write-Host "--- Build system ---"
[void](Check-Grep -Rel "CMakeLists.txt" -Pattern "periscope_overlay\.cpp" -Desc "periscope_overlay in CMake")
[void](Check-Grep -Rel "CMakeLists.txt" -Pattern "SECTION_SEED" -Desc "SECTION_SEED in CMake")
[void](Check-Grep -Rel "CMakeLists.txt" -Pattern "LR_ENABLE_SHELLCODE_INJECTION" -Desc "LR_ENABLE_SHELLCODE_INJECTION option")
[void](Check-Grep -Rel "CMakeLists.txt" -Pattern "LR_ENABLE_XORSTR_OBFUSCATION" -Desc "LR_ENABLE_XORSTR_OBFUSCATION option")
[void](Check-Grep -Rel "CMakeLists.txt" -Pattern "LR_ENABLE_PERISCOPE_OVERLAY" -Desc "LR_ENABLE_PERISCOPE_OVERLAY option")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kSectionSeed" -Desc "kSectionSeed in template")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kGuidSeed" -Desc "kGuidSeed in template")
[void](Check-Grep -Rel "cmake/build_keys.hpp.in" -Pattern "kSurfaceSeed" -Desc "kSurfaceSeed in template")

Write-Host ""
Write-Host "==========================================="
if ($script:Errors -eq 0) {
  Write-Host ("All checks passed! (" + $script:Passed + " OK)") -ForegroundColor Green
  Write-Host "==========================================="
  exit 0
}
Write-Host ($script:Errors.ToString() + " check(s) failed (" + $script:Passed + " OK).") -ForegroundColor Red
Write-Host "==========================================="
exit 1
