#
# build-all-levels.ps1
# Purpose: Build all USB_BRINGUP_LEVEL variants (2, 3, 4) with clean build and logging.
# 
# Robustness Features:
# - Safe script invocation using & operator (not Invoke-Expression)
# - Proper exit code handling and error propagation
# - Ensures working directory is correctly set for all subprocesses
# - Handles paths with spaces and special characters safely
# - Uses common-build.ps1 for shared utilities
#
# Usage: .\tools\build-all-levels.ps1
#        .\tools\build-all-levels.ps1 -NoLog
#

param(
    [switch]$NoLog
)

$ErrorActionPreference = 'Stop'

# Source common build utilities
$toolsDir = $PSScriptRoot
$commonBuild = Join-Path $toolsDir "common-build.ps1"
if(!(Test-Path $commonBuild)) {
    Write-Output "✗ Common build library not found: $commonBuild"
    exit 1
}
. $commonBuild

# Get repo root and ensure we're in the correct directory
$repoRoot = Split-Path -Parent $toolsDir

# Verify we're in the right location
if(!(Test-Path "CMakeLists.txt")) {
    Write-Output "✗ CMakeLists.txt not found"
    Write-Output "  Expected to run from repository root: $repoRoot"
    Write-Output "  Current directory: $(Get-Location)"
    exit 1
}

# Define build configurations
$builds = @(
    @{ Level = 2; OutputDir = "build-normal"; Description = "FFT + Display (CDC OFF)" },
    @{ Level = 3; OutputDir = "build-cdc"; Description = "CDC Diagnostic (FFT OFF)" },
    @{ Level = 4; OutputDir = "build-level4"; Description = "Max Load (CDC + FFT + Display)" }
)

Write-Output "=== Building All USB_BRINGUP_LEVEL Variants ==="
Write-Output "Repository Root: $repoRoot"
Write-Output "Working Directory: $(Get-Location)"
Write-Output ""

$startTime = Get-Date
$results = @()

foreach($build in $builds) {
    Write-Output "──────────────────────────────────────────────"
    Write-Output "Building Level $($build.Level): $($build.Description)"
    Write-Output "──────────────────────────────────────────────"
    Write-Output ""
    
    # Build script path (safe, no Invoke-Expression)
    $buildScript = Join-Path $toolsDir "build-level.ps1"
    
    # Build arguments (array form for safe parameter passing)
    $buildArgs = @(
        "-Level", $build.Level,
        "-OutputDir", $build.OutputDir
    )
    
    if($NoLog) {
        $buildArgs += "-NoLog"
    }
    
    # Invoke build-level.ps1 safely
    & $buildScript @buildArgs
    $buildExitCode = $LASTEXITCODE
    
    if($buildExitCode -ne 0) {
        Write-Output "✗ Build failed for Level $($build.Level) (exit code: $buildExitCode)"
        $results += @{ Level = $build.Level; Status = "FAILED"; ExitCode = $buildExitCode }
    } else {
        Write-Output "✓ Build succeeded for Level $($build.Level)"
        $results += @{ Level = $build.Level; Status = "SUCCESS"; ExitCode = 0 }
    }
    
    Write-Output ""
}

# Summary
Write-Output "=== Build Summary ==="
Write-Output ""

$allSuccess = $true
foreach($result in $results) {
    $status = $result.Status
    $icon = if($status -eq "SUCCESS") { "✓" } else { "✗" }
    Write-Output "$icon Level $($result.Level): $status"
    if($status -ne "SUCCESS") { $allSuccess = $false }
}

$endTime = Get-Date
$duration = [math]::Round(($endTime - $startTime).TotalSeconds, 1)

Write-Output ""
Write-Output "Total Time: $($duration)s"
Write-Output ""

if($allSuccess) {
    Write-Output "✓ All builds completed successfully"
    Write-Output ""
    Write-Output "Next step - Run analysis with:"
    Write-Output "  .\tools\analyze-builds.ps1"
    Write-Output ""
    exit 0
} else {
    Write-Output "✗ Some builds failed - See details above"
    exit 1
}
