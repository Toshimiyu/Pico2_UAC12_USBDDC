#
# build-level.ps1
# Purpose: Clean build a specific USB_BRINGUP_LEVEL, run size analysis, and log results.
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

# Setup environment
$env:PATH = "$NinjaPath;$ToolchainPath;$env:PATH"
$repoRoot = Split-Path -Parent $PSScriptRoot

# Timestamps and paths
$timestamp = Get-Date -Format "yyyy-MM-dd HHmmss"
$logDir = Join-Path $repoRoot "build-logs"
$logFile = Join-Path $logDir "build-info-Level${Level}-${timestamp}.txt"

# Log function
function Log-Message {
    param([string]$Message)
    Write-Output $Message
    if(!$NoLog -and (Test-Path $logDir)) {
        Add-Content -Path $logFile -Value $Message
    }
}

# Header
Log-Message "=== Build Summary ==="
Log-Message "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Log-Message "USB_BRINGUP_LEVEL: $Level"
Log-Message "Output Directory: $OutputDir"
Log-Message ""

# Clean old build if not -NoClean
if(!$NoClean -and (Test-Path $OutputDir)) {
    Log-Message "Removing old build directory..."
    Remove-Item -Recurse -Force $OutputDir
    Log-Message "✓ Old build directory removed"
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
$cmakeCmd = "cmake -S . -B $OutputDir -G Ninja -DUSB_AUDIO_CLASS_VERSION=2 -DUSB_BRINGUP_LEVEL=$Level"
Log-Message "Command: $cmakeCmd"
Log-Message ""

Write-Output "Configuring with CMake..."
$cmakeOutput = & cmake -S . -B $OutputDir -G Ninja -DUSB_AUDIO_CLASS_VERSION=2 -DUSB_BRINGUP_LEVEL=$Level 2>&1
if($LASTEXITCODE -ne 0) {
    Log-Message "[CMake Error]"
    Log-Message $cmakeOutput
    Log-Message "✗ CMake configuration failed"
    exit 1
}
Log-Message "✓ CMake configuration complete"
Log-Message ""

# Build
Log-Message "[Build]"
Write-Output "Building with Ninja..."
$buildStart = Get-Date
$buildOutput = & cmake --build $OutputDir 2>&1
$buildEnd = Get-Date
$buildTime = ($buildEnd - $buildStart).TotalSeconds

if($LASTEXITCODE -ne 0) {
    Log-Message "[Build Error]"
    Log-Message $buildOutput
    Log-Message "✗ Build failed"
    exit 1
}

# Extract final link line
$buildOutput | Where-Object { $_ -match "Linking" } | ForEach-Object { Log-Message $_ }
Log-Message "✓ Build complete (${buildTime}s)"
Log-Message ""

# Size analysis
Log-Message "[Size Analysis]"
$elfFile = Get-ChildItem -Path "$OutputDir\src" -Filter "*.elf" -ErrorAction SilentlyContinue | Select-Object -First 1

if(!$elfFile) {
    Log-Message "✗ ELF file not found"
    exit 1
}

Log-Message "ELF File: $($elfFile.FullName)"
$sizeOutput = & arm-none-eabi-size.exe $elfFile.FullName 2>&1

$sizeOutput | ForEach-Object { Log-Message $_ }

# Parse size for memory summary
$lastLine = $sizeOutput | Select-Object -Last 1
if($lastLine -match '(\d+)\s+(\d+)\s+(\d+)') {
    [int]$text = $matches[1]
    [int]$data = $matches[2]
    [int]$bss = $matches[3]
    
    $ramTotal = 458752  # 448 KiB
    $ramPct = [math]::Round(100.0 * $bss / $ramTotal, 1)
    $ramFree = $ramTotal - $bss
    
    Log-Message ""
    Log-Message "[Memory Summary]"
    Log-Message "BSS: $bss B ($ramPct% of $ramTotal B)"
    Log-Message "Free: $ramFree B"
    Log-Message ""
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
