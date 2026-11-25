#
# build-level.ps1
# Purpose: Clean build a specific USB_BRINGUP_LEVEL, run size analysis, and log results.
# 
# Robustness Features:
# - Handles paths with spaces and special characters safely
# - Validates all external tools before execution
# - Ensures working directory is correctly set for cmake -S .
# - Proper error propagation with exit codes
# - Log files created with ISO 8601 timestamps
# - Uses common-build.ps1 for shared utilities
#
# Usage: .\tools\build-level.ps1 -Level 2 -OutputDir ./build-normal
#        .\tools\build-level.ps1 -Level 3 -OutputDir ./build-cdc
#        .\tools\build-level.ps1 -Level 4 -OutputDir ./build-level4
#

param(
    [ValidateSet(0, 1, 2, 3, 4)]
    [int]$Level = 2,
    
    [string]$OutputDir = "build-level$Level",
    
    [string]$ToolchainPath = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin",
    [string]$NinjaPath = "$env:USERPROFILE\.pico-sdk\ninja\v1.13.2",
    
    [switch]$NoClean,
    [switch]$NoLog
)

$ErrorActionPreference = 'Stop'

# Source common build utilities
$commonBuild = Join-Path $PSScriptRoot "common-build.ps1"
if(!(Test-Path $commonBuild)) {
    Write-Output "✗ Common build library not found: $commonBuild"
    exit 1
}
. $commonBuild

# Setup environment
try {
    Setup-BuildEnvironment -ToolchainPath $ToolchainPath -NinjaPath $NinjaPath
} catch {
    Write-Output "✗ Failed to setup build environment: $_"
    exit 1
}

$repoRoot = Split-Path -Parent $PSScriptRoot

# Save original location and change to repo root to ensure correct cmake -S . resolution
$originalLocation = Get-Location
try {
    Set-Location $repoRoot
} catch {
    Write-Output "✗ Failed to change to repository root: $_"
    exit 1
}

# Validate required tools
try {
    Test-RequiredTools @("cmake.exe", "ninja.exe", "arm-none-eabi-size.exe", "arm-none-eabi-nm.exe")
} catch {
    Write-Output "✗ $_"
    exit 1
}

# Timestamps and paths
$timestamp = Get-Date -Format "yyyy-MM-dd HHmmss"
$logDir = Join-Path $repoRoot "build-logs"
$logFile = if($NoLog) { $null } else { Join-Path $logDir "build-info-Level${Level}-${timestamp}.txt" }

# Ensure build-logs directory exists
if(!$NoLog) {
    try {
        New-LogDirectory $logDir
    } catch {
        Write-Output "✗ Failed to create log directory: $_"
        exit 1
    }
}

# Log function
function Log-Message {
    param([string]$Message)
    Save-BuildLog $logFile $Message
}

# Header
Log-Message "=== Build Summary ==="
Log-Message "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Log-Message "USB_BRINGUP_LEVEL: $Level"
Log-Message "Output Directory: $OutputDir"
Log-Message "Working Directory: $(Get-Location)"
Log-Message ""

# Clean old build if not -NoClean
if(!$NoClean -and (Test-Path $OutputDir)) {
    Log-Message "Removing old build directory..."
    try {
        Remove-Item -Recurse -Force $OutputDir -ErrorAction Stop
        Log-Message "✓ Old build directory removed"
    } catch {
        Log-Message "✗ Failed to remove old build directory: $_"
        exit 1
    }
}

Log-Message ""

# Determine features based on Level
$cdcEnabled = $Level -ge 3
$fftEnabled = $Level -ne 3

Log-Message "[Build Configuration]"
Log-Message "Level: $Level"
Log-Message "CDC: $(if($cdcEnabled){'ON'}else{'OFF'})"
Log-Message "FFT: $(if($fftEnabled){'ON'}else{'OFF'})"
Log-Message "Display: ON"
Log-Message ""

# CMake configuration
Log-Message "[CMake Configuration]"
Log-Message "Command: cmake -S . -B $OutputDir -G Ninja -DUSB_AUDIO_CLASS_VERSION=2 -DUSB_BRINGUP_LEVEL=$Level"
Log-Message ""

Write-Output "Configuring with CMake..."
$cmakeOutput = @()
try {
    $cmakeArgs = "-S", ".", "-B", $OutputDir, "-G", "Ninja", "-DUSB_AUDIO_CLASS_VERSION=2", "-DUSB_BRINGUP_LEVEL=$Level"
    $cmakeOutput = & cmake $cmakeArgs 2>&1
    $cmakeExitCode = $LASTEXITCODE
} catch {
    Log-Message "[CMake Error]"
    Log-Message "Exception: $_"
    Log-Message "✗ CMake execution failed"
    exit 1
}

if($cmakeExitCode -ne 0) {
    Log-Message "[CMake Error]"
    $cmakeOutput | ForEach-Object { Log-Message $_ }
    Log-Message "✗ CMake configuration failed (exit code: $cmakeExitCode)"
    exit 1
}
Log-Message "✓ CMake configuration complete"
Log-Message ""

# Build
Log-Message "[Build]"
Write-Output "Building with Ninja..."
$buildStart = Get-Date
$buildOutput = @()
try {
    $buildArgs = "--build", $OutputDir
    $buildOutput = & cmake $buildArgs 2>&1
    $buildExitCode = $LASTEXITCODE
} catch {
    Log-Message "[Build Error]"
    Log-Message "Exception: $_"
    Log-Message "✗ Build execution failed"
    exit 1
}
$buildEnd = Get-Date
$buildTime = [math]::Round(($buildEnd - $buildStart).TotalSeconds, 2)

if($buildExitCode -ne 0) {
    Log-Message "[Build Error]"
    $buildOutput | ForEach-Object { Log-Message $_ }
    Log-Message "✗ Build failed (exit code: $buildExitCode)"
    exit 1
}

# Extract final link line
$buildOutput | Where-Object { $_ -match "Linking" } | ForEach-Object { Log-Message $_ }
Log-Message "✓ Build complete ($($buildTime)s)"
Log-Message ""

# Size analysis
Log-Message "[Size Analysis]"
$elfFile = Get-ChildItem -Path "$OutputDir\src" -Filter "*.elf" -ErrorAction SilentlyContinue | Select-Object -First 1

if(!$elfFile) {
    Log-Message "✗ ELF file not found in $OutputDir\src"
    exit 1
}

Log-Message "ELF File: $($elfFile.FullName)"
$sizeOutput = @()
try {
    $sizeOutput = & arm-none-eabi-size.exe $elfFile.FullName 2>&1
    $sizeExitCode = $LASTEXITCODE
} catch {
    Log-Message "[Size Analysis Error]"
    Log-Message "Exception: $_"
    exit 1
}

if($sizeExitCode -ne 0) {
    Log-Message "[Size Analysis Error]"
    Log-Message "arm-none-eabi-size failed (exit code: $sizeExitCode)"
}

$sizeOutput | ForEach-Object { Log-Message $_ }

# Report the RAM-backed heap capacity used by malloc, not total RAM minus BSS.
$lastLine = $sizeOutput | Select-Object -Last 1
if($lastLine -match '(\d+)\s+(\d+)\s+(\d+)') {
    [int]$bss = $matches[3]

    $symbolOutput = & arm-none-eabi-nm.exe -n $elfFile.FullName 2>&1
    $endLine = $symbolOutput | Where-Object { $_ -match '\s__end__$' } | Select-Object -First 1
    $heapLimitLine = $symbolOutput | Where-Object { $_ -match '\s__HeapLimit$' } | Select-Object -First 1
    $endMatch = [regex]::Match([string]$endLine, '^\s*([0-9A-Fa-f]+)\s+\w\s+__end__$')
    $heapLimitMatch = [regex]::Match([string]$heapLimitLine, '^\s*([0-9A-Fa-f]+)\s+\w\s+__HeapLimit$')
    if($endMatch.Success -and $heapLimitMatch.Success) {
        $heapStart = [Convert]::ToInt64($endMatch.Groups[1].Value, 16)
        $heapLimit = [Convert]::ToInt64($heapLimitMatch.Groups[1].Value, 16)
        $heapAvailable = $heapLimit - $heapStart

        Log-Message ""
        Log-Message "[Memory Summary]"
        Log-Message "BSS: $bss B"
        Log-Message "Heap capacity before allocations: $heapAvailable B"
        $ringFramesLine = Select-String -Path (Join-Path $repoRoot "src\common.h") `
            -Pattern '^\s*#define\s+SIZE_AUDIO_RING_FRAMES\s+\(?(\d+)\)?' |
            Select-Object -First 1
        if($ringFramesLine -and $ringFramesLine.Matches[0].Groups[1].Success) {
            $ringBufferBytes = [long]$ringFramesLine.Matches[0].Groups[1].Value * 2 * 4
            Log-Message "Audio ring-buffer allocation: $ringBufferBytes B"
            Log-Message "Heap after ring-buffer allocation: $($heapAvailable - $ringBufferBytes) B"
        }
        Log-Message ""
    } else {
        Log-Message "[Warning] Could not read __end__ and __HeapLimit from ELF"
    }
} else {
    Log-Message "[Warning] Could not parse size output"
}

Log-Message "[Build Complete]"
Log-Message "Output: $OutputDir"
Log-Message "Log: $logFile"
Log-Message ""

Write-Output ""
Write-Output "✓ Build completed successfully"
if(!$NoLog) {
    Write-Output "✓ Log saved to: $logFile"
}

# Cleanup: Return to original location
Set-Location $originalLocation
Write-Output ""
Write-Output "✓ Returned to original directory"
