Describe "radar PowerShell output streams" {
  BeforeAll {
    $pwshPath = (Get-Command pwsh -ErrorAction Stop).Source

    function Get-CodeRoot {
      return Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    }

    function Invoke-ScriptCapture {
      param(
        [string]$ScriptPath,
        [string[]]$ArgumentList = @()
      )

      $stdout = Join-Path $TestDrive "stdout.txt"
      $stderr = Join-Path $TestDrive "stderr.txt"
      & $pwshPath -NoProfile -File $ScriptPath @ArgumentList 1> $stdout 2> $stderr

      return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        StdOut = Get-Content -LiteralPath $stdout -Raw
        StdErr = Get-Content -LiteralPath $stderr -Raw
      }
    }
  }

  It "renders the regression self-test progress and exits successfully" {
    $result = Invoke-ScriptCapture -ScriptPath (Join-Path (Get-CodeRoot) "scripts/radar_regression.ps1") -ArgumentList @("-SelfTest")

    $result.ExitCode | Should -Be 0
    $result.StdOut | Should -Match "=== radar_regression -SelfTest"
    $result.StdOut | Should -Match "=== SELF-TEST PASSED"
    $result.StdErr | Should -BeNullOrEmpty
  }

  It "reports missing feature checks through the error stream and exits nonzero" {
    $fixtureScripts = Join-Path $TestDrive "scripts"
    $null = New-Item -ItemType Directory -Path $fixtureScripts
    $fixtureScript = Join-Path $fixtureScripts "verify_features.ps1"
    Copy-Item -LiteralPath (Join-Path (Get-CodeRoot) "scripts/verify_features.ps1") -Destination $fixtureScript

    $result = Invoke-ScriptCapture -ScriptPath $fixtureScript

    $result.ExitCode | Should -Be 1
    $result.StdOut | Should -Match "Verification: T0.12"
    $result.StdErr | Should -Match "\[MISSING\]"
    $result.StdErr | Should -Match "check\(s\) failed"
  }
}
