#
# build-all-levels.ps1
# Purpose: Build all USB_BRINGUP_LEVEL variants (2, 3, 4) with clean build and logging.
# Usage: .\tools\build-all-levels.ps1
#

param(
    [switch]$NoLog
)

$repoRoot = Split-Path -Parent $PSScriptRoot
$toolsDir = $PSScriptRoot

# Define build configurations
$builds = @(
    @{ Level = 2; OutputDir = "build-normal"; Description = "FFT + Display (CDC OFF)" },
    @{ Level = 3; OutputDir = "build-cdc"; Description = "CDC Diagnostic (FFT OFF)" },
    @{ Level = 4; OutputDir = "build-level4"; Description = "Max Load (CDC + FFT + Display)" }
)

Write-Output "=== Building All USB_BRINGUP_LEVEL Variants ==="
Write-Output ""

$startTime = Get-Date
$results = @()

foreach($build in $builds) {
    Write-Output "──────────────────────────────────────────────"
    Write-Output "Building Level $($build.Level): $($build.Description)"
    Write-Output "──────────────────────────────────────────────"
    Write-Output ""
    
    # Call build-level.ps1
    $buildCmd = "& '$toolsDir\build-level.ps1' -Level $($build.Level) -OutputDir `"$($build.OutputDir)`""
    if($NoLog) { $buildCmd += " -NoLog" }
    
    Invoke-Expression $buildCmd
    
    if($LASTEXITCODE -ne 0) {
        Write-Output "✗ Build failed for Level $($build.Level)"
        $results += @{ Level = $build.Level; Status = "FAILED" }
    } else {
        $results += @{ Level = $build.Level; Status = "SUCCESS" }
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
$duration = ($endTime - $startTime).TotalSeconds

Write-Output ""
Write-Output "Total Time: $([math]::Round($duration, 1))s"
Write-Output ""

if($allSuccess) {
    Write-Output "✓ All builds completed successfully"
    Write-Output ""
    Write-Output "Run analysis with:"
    Write-Output "  .\tools\analyze-builds.ps1"
} else {
    Write-Output "✗ Some builds failed"
    exit 1
}
