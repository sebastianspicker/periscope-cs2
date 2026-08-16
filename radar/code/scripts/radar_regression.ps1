# radar_regression.ps1 - Smoke regression for live radar lab demos.
#
# Offline gates (always run):
#   [1] scale / nudge pure math (matches demos/radar_shared.hpp + render_pipeline)
#   [2] expected radar demo build artifacts must exist (hard fail if missing)
#
# Live gates (optional):
#   When -SkipLive is set, or cs2.exe is not running, live steps skip cleanly
#   without failing offline gates. When live, demos must produce real content.
#
# Usage (from code/):
#   powershell -ExecutionPolicy Bypass -File scripts/radar_regression.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/radar_regression.ps1 -SkipLive
#   powershell -ExecutionPolicy Bypass -File scripts/radar_regression.ps1 -BuildDir build/Release
#   powershell -ExecutionPolicy Bypass -File scripts/radar_regression.ps1 -SelfTest
#
# Exit codes:
#   0  all required gates passed (live skipped or passed)
#   1  offline math/artifact failure, or live content assertion failure

param(
  [switch]$SkipLive,
  [switch]$SelfTest,
  [string]$BuildDir = "build/Release",
  [int]$LiveTimeoutSec = 25
)

$ErrorActionPreference = "Stop"
$script:FailCount = 0
$script:PassCount = 0

function Write-Status {
  param([string]$Message)
  Write-Information -MessageData $Message -InformationAction Continue
}

# Resolve repo code/ root from this script location (scripts/ -> parent).
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Pure formulas (kept in-script so offline gates exercise shipped script logic)
# world_edge: demos/radar_shared.hpp radar_world_scale(cl) ~= 750 / cl
#   at default cl_radar_scale=0.7 -> 750/0.7 ~= 1071.428
# resolution nudge: lib/real/gpu/render_pipeline.cpp resolution_center_nudge_px
#   round(base * client_h / 800), base default 24

function Get-RadarWorldScale {
  param([double]$ClRadarScale = 0.7, [double]$WorldEdgeAtScale1 = 750.0)
  if ($ClRadarScale -le 0.0) {
    throw "cl_radar_scale must be > 0 (got $ClRadarScale)"
  }
  return $WorldEdgeAtScale1 / $ClRadarScale
}

function Get-ResolutionCenterNudgePx {
  param([int]$ClientH, [int]$BaseNudgeAt800p = 24)
  if ($ClientH -le 0) {
    throw "client_h must be > 0 (got $ClientH)"
  }
  if ($BaseNudgeAt800p -lt 0) { $BaseNudgeAt800p = 24 }
  return [int][math]::Round($BaseNudgeAt800p * ($ClientH / 800.0))
}

function Assert-True {
  param([bool]$Condition, [string]$Message)
  if (-not $Condition) {
    Write-Error -Message ("  FAIL  " + $Message) -ErrorAction Continue
    $script:FailCount++
    throw ("ASSERT FAIL: " + $Message)
  }
  Write-Status ("  OK  " + $Message)
  $script:PassCount++
}

function Test-ScaleNudgeMath {
  Write-Status "[1] scale / nudge formulas (pure)"

  $edge = Get-RadarWorldScale -ClRadarScale 0.7
  $okEdge = [math]::Abs($edge - 1071.4285714285713) -lt 0.01
  Assert-True $okEdge ("world_scale(0.7) = {0:F3} (~1071.429)" -f $edge)

  $edge1 = Get-RadarWorldScale -ClRadarScale 1.0
  Assert-True ([math]::Abs($edge1 - 750.0) -lt 0.001) "world_scale(1.0) = 750"

  $edge05 = Get-RadarWorldScale -ClRadarScale 0.5
  Assert-True ([math]::Abs($edge05 - 1500.0) -lt 0.001) "world_scale(0.5) = 1500"

  $n800 = Get-ResolutionCenterNudgePx -ClientH 800
  Assert-True ($n800 -eq 24) "nudge at 800p = 24"

  $n1080 = Get-ResolutionCenterNudgePx -ClientH 1080
  Assert-True ($n1080 -eq 32) "nudge at 1080p = 32"

  $n720 = Get-ResolutionCenterNudgePx -ClientH 720
  Assert-True ($n720 -eq 22) "nudge at 720p = 22"

  $n1440 = Get-ResolutionCenterNudgePx -ClientH 1440 -BaseNudgeAt800p 24
  Assert-True ($n1440 -eq 43) "nudge at 1440p = 43"

  # Cross-check inverse law used by scale_probe diagnostics.
  $implied = 1071.4285714285713 * 0.7
  Assert-True ([math]::Abs($implied - 750.0) -lt 0.01) "inverse: edge*0.7 ~= 750"
}

function Test-BuildArtifacts {
  param([string]$Dir)
  Write-Status ("[2] build artifacts under " + $Dir)

  if (-not (Test-Path -LiteralPath $Dir)) {
    throw ("ASSERT FAIL: build directory missing: " + $Dir + " (build Release radar demos first)")
  }

  $required = @(
    "live_radar.exe",
    "radar_diag.exe",
    "scale_probe.exe"
  )
  foreach ($n in $required) {
    $p = Join-Path $Dir $n
    Assert-True (Test-Path -LiteralPath $p) ($n + " exists")
  }

  # Optional lab companions - report only (do not fail offline gate).
  $optional = @("radar_t0.exe", "cs2_radar_t0.exe", "radar_multi_tier.exe")
  foreach ($n in $optional) {
    $p = Join-Path $Dir $n
    if (Test-Path -LiteralPath $p) {
      Write-Status ("  OK  " + $n + " exists (optional)")
    } else {
      Write-Status ("  --  " + $n + " absent (optional)")
    }
  }
}

function Test-SelfTestOnly {
  Write-Status "=== radar_regression -SelfTest (pure math only) ==="
  Test-ScaleNudgeMath
  # Negative-path structural checks on pure helpers (must throw).
  $threw = $false
  try { Get-RadarWorldScale -ClRadarScale 0.0 | Out-Null } catch { $threw = $true }
  Assert-True $threw "Get-RadarWorldScale rejects cl_radar_scale <= 0"
  $threw2 = $false
  try { Get-ResolutionCenterNudgePx -ClientH 0 | Out-Null } catch { $threw2 = $true }
  Assert-True $threw2 "Get-ResolutionCenterNudgePx rejects client_h <= 0"
  Write-Status ("=== SELF-TEST PASSED (" + $script:PassCount + " assertions) ===")
}

function Invoke-LiveScaleProbe {
  param([string]$Dir)
  Write-Status "[3] live scale_probe"
  $exe = Join-Path $Dir "scale_probe.exe"
  Assert-True (Test-Path -LiteralPath $exe) "scale_probe.exe present for live run"

  $prev = $env:LR_SKIP_CVAR_SCAN
  $env:LR_SKIP_CVAR_SCAN = "1"
  try {
    $out = & $exe 2>&1 | Out-String
  } finally {
    if ($null -eq $prev) { Remove-Item Env:LR_SKIP_CVAR_SCAN -ErrorAction SilentlyContinue }
    else { $env:LR_SKIP_CVAR_SCAN = $prev }
  }
  Write-Status $out
  Assert-True ($out -match "1071") "scale_probe reports ~1071"
  Assert-True ($out -match "local=") "scale_probe produced local= line"
}

function Invoke-LiveRadarBrief {
  param([string]$Dir, [int]$TimeoutSec)
  Write-Status ("[4] live_radar brief (" + $TimeoutSec + "s)")
  $exe = Join-Path $Dir "live_radar.exe"
  Assert-True (Test-Path -LiteralPath $exe) "live_radar.exe present for live run"

  $stdout = Join-Path $Dir "regression_live.txt"
  $stderr = Join-Path $Dir "regression_live_err.txt"
  foreach ($f in @($stdout, $stderr)) {
    if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
  }

  $workDir = (Resolve-Path -LiteralPath $Dir).Path
  $proc = Start-Process -FilePath $exe -WorkingDirectory $workDir `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
    -PassThru -NoNewWindow

  $sw = [Diagnostics.Stopwatch]::StartNew()
  while (-not $proc.HasExited -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
    Start-Sleep -Milliseconds 300
  }
  if (-not $proc.HasExited) {
    Write-Status ("  (timeout " + $TimeoutSec + "s - stopping live_radar)")
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    try { $proc.WaitForExit(3000) | Out-Null } catch { Write-Warning "Unable to confirm live_radar stopped after timeout: $($_.Exception.Message)" }
  }

  $txt = ""
  if (Test-Path -LiteralPath $stdout) {
    $txt = Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue
  }
  if ([string]::IsNullOrWhiteSpace($txt)) {
    throw ("ASSERT FAIL: live_radar produced empty stdout (see " + $stderr + ")")
  }
  Write-Status $txt

  Assert-True ($txt -match "SNAP") "live_radar SNAP layout"
  $nudgeOk = ($txt -match "nudge\s*[=:]?\s*2[0-9]") -or ($txt -match "nudgeSE\s*=\s*2") -or ($txt -match "nudge=2")
  Assert-True $nudgeOk "live_radar nudge near 2x"
  Assert-True ($txt -match "entities\s*=") "live_radar entity count"
}

# Main

try {
  if ($SelfTest) {
    Test-SelfTestOnly
    exit 0
  }

  Write-Status "=== radar_regression ==="
  Write-Status ("root=" + $root)
  Write-Status ("BuildDir=" + $BuildDir + " SkipLive=" + $SkipLive)

  Test-ScaleNudgeMath
  Test-BuildArtifacts -Dir $BuildDir

  if ($SkipLive) {
    Write-Status "SkipLive set - offline gates only."
    Write-Status ("=== OFFLINE CHECKS PASSED (" + $script:PassCount + " assertions) ===")
    exit 0
  }

  $cs2 = Get-Process -Name cs2 -ErrorAction SilentlyContinue
  if (-not $cs2) {
    Write-Status "SKIP live (cs2 process not running) - offline gates passed."
    Write-Status ("=== OFFLINE CHECKS PASSED (" + $script:PassCount + " assertions); live skipped ===")
    exit 0
  }

  Write-Status ("cs2.exe detected (PID(s): " + ($cs2.Id -join ", ") + ")")
  Invoke-LiveScaleProbe -Dir $BuildDir
  Invoke-LiveRadarBrief -Dir $BuildDir -TimeoutSec $LiveTimeoutSec

  Write-Status ("=== ALL CHECKS PASSED (" + $script:PassCount + " assertions) ===")
  exit 0
}
catch {
  Write-Information -MessageData "" -InformationAction Continue
  Write-Error -Message $_.Exception.Message -ErrorAction Continue
  Write-Error -Message ("=== FAILED (passed=" + $script:PassCount + ") ===") -ErrorAction Continue
  exit 1
}
