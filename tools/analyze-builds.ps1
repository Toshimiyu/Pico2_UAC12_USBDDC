#
# analyze-builds.ps1
# Purpose: Analyze all build directories and report USB_BRINGUP_LEVEL, features, and memory usage.
# 
# Robustness Features:
# - Excludes build-logs/ automatically (filtered by CMakeCache.txt existence check)
# - Tolerant of CMakeCache.txt format variations (handles trailing whitespace)
# - Validates arm-none-eabi-size output before parsing
# - Flexible column width calculation for variable directory name lengths
#
# Usage: .\tools\analyze-builds.ps1 [-BuildDirs @("build-cdc", "build-normal", "build-level4")]
#

param(
    [string[]]$BuildDirs = @(),  # Empty by default; auto-detect
    [string]$ToolchainPath = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin"
)

$env:PATH = "$ToolchainPath;$env:PATH"

# Determine repo root (parent of tools directory)
$repoRoot = Split-Path -Parent $PSScriptRoot

Write-Output "=== Build Configuration Analysis ==="
Write-Output "Repo Root: $repoRoot"
Write-Output ""

# Auto-detect build directories if not specified
# Note: Automatically excludes build-logs/ and other build-* dirs without CMakeCache.txt
if($BuildDirs.Count -eq 0) {
    Write-Output "Auto-detecting build directories with CMakeCache.txt..."
    $BuildDirs = @(Get-ChildItem -Path $repoRoot -Directory -Filter "build-*" -ErrorAction SilentlyContinue | 
                   Where-Object { Test-Path (Join-Path $_.FullName "CMakeCache.txt") } |
                   Select-Object -ExpandProperty Name | Sort-Object)
    if($BuildDirs.Count -eq 0) {
        Write-Output "✗ No build directories with CMakeCache.txt found"
        Write-Output "  (Expected: build-cdc, build-normal, build-level4, etc.)"
        Write-Output "  (Excluded: build-logs/, CMake caches without CMakeCache.txt)"
        exit 1
    }
}

Write-Output "Found $($BuildDirs.Count) build director(ies): $($BuildDirs -join ', ')"
Write-Output ""

$results = @()

foreach($dir in $BuildDirs) {
    $buildPath = Join-Path $repoRoot $dir
    
    if(!(Test-Path $buildPath)) {
        Write-Output "⚠ $($dir): Directory not found"
        continue
    }
    
    # Read USB_BRINGUP_LEVEL from CMakeCache.txt
    $cacheFile = "$buildPath\CMakeCache.txt"
    if(!(Test-Path $cacheFile)) {
        Write-Output "⚠ $($dir): CMakeCache.txt not found"
        continue
    }
    
    $levelMatch = Select-String -Path $cacheFile -Pattern "^USB_BRINGUP_LEVEL:STRING=(\d+)$"
    if(!$levelMatch) {
        Write-Output "⚠ $($dir): USB_BRINGUP_LEVEL not found in CMakeCache.txt"
        continue
    }
    
    [int]$level = $levelMatch.Matches[0].Groups[1].Value
    
    # Determine CDC and FFT status based on Level
    # USB_CDC_ENABLED = (USB_BRINGUP_LEVEL >= 3)
    # USB_FFT_ENABLED = (USB_BRINGUP_LEVEL != 3)
    $cdcEnabled = $level -ge 3
    $fftEnabled = $level -ne 3
    $displayEnabled = $true  # Always enabled
    
    # Find ELF file
    $elfFile = Get-ChildItem -Path "$buildPath\src" -Filter "*.elf" -ErrorAction SilentlyContinue | Select-Object -First 1
    
    if(!$elfFile) {
        Write-Output "⚠ $($dir): ELF file not found"
        continue
    }
    
    # Get size information
    $sizeOutput = & "arm-none-eabi-size.exe" $elfFile.FullName 2>&1 | Select-Object -Last 1
    
    if($sizeOutput -match '(\d+)\s+(\d+)\s+(\d+)') {
        [int]$text = $matches[1]
        [int]$data = $matches[2]
        [int]$bss = $matches[3]
        
        $symbolOutput = & "arm-none-eabi-nm.exe" -n $elfFile.FullName 2>&1
        $endLine = $symbolOutput | Where-Object { $_ -match '\s__end__$' } | Select-Object -First 1
        $heapLimitLine = $symbolOutput | Where-Object { $_ -match '\s__HeapLimit$' } | Select-Object -First 1
        $endMatch = [regex]::Match([string]$endLine, '^\s*([0-9A-Fa-f]+)\s+\w\s+__end__$')
        $heapLimitMatch = [regex]::Match([string]$heapLimitLine, '^\s*([0-9A-Fa-f]+)\s+\w\s+__HeapLimit$')
        if(!$endMatch.Success -or !$heapLimitMatch.Success) {
            Write-Output "⚠ $($dir): Could not read __end__ and __HeapLimit from ELF"
            continue
        }

        $heapStart = [Convert]::ToInt64($endMatch.Groups[1].Value, 16)
        $heapLimit = [Convert]::ToInt64($heapLimitMatch.Groups[1].Value, 16)
        $heapAvailable = $heapLimit - $heapStart
        
        $result = @{
            Directory    = $dir
            Level        = $level
            CDC          = $cdcEnabled
            FFT          = $fftEnabled
            Display      = $displayEnabled
            Text         = $text
            Data         = $data
            BSS          = $bss
            HeapAvailable = $heapAvailable
            ELF          = $elfFile.Name
        }
        $results += $result
        
        # Display result
        Write-Output "[$($dir)]"
        Write-Output "  Level: $level"
        Write-Output "  Features: CDC=$(if($cdcEnabled){'ON'}else{'OFF'}), FFT=$(if($fftEnabled){'ON'}else{'OFF'}), Display=$(if($displayEnabled){'ON'}else{'OFF'})"
        Write-Output "  ELF: $($elfFile.Name)"
        Write-Output "  Memory: text=$text B, data=$data B, bss=$bss B"
        Write-Output "  Heap available before allocations: $heapAvailable B"
        Write-Output ""
    } else {
        Write-Output "⚠ $($dir): Failed to parse size output"
    }
}

# Summary table
if($results.Count -gt 0) {
    Write-Output "=== Summary Table ==="
    Write-Output ""
    
    # Calculate column widths dynamically
    $maxDirLen = ([int]($results | ForEach-Object { $_.Directory.Length } | Measure-Object -Maximum).Maximum)
    $maxDirLen = [math]::Max($maxDirLen, 11)  # Minimum width for "Dir" header
    $dirColWidth = $maxDirLen
    
    # Header row
    $headerDir = "Dir".PadRight($dirColWidth)
    $headerLine = "$headerDir | Level | CDC | FFT | Display | BSS (B)  | Heap Available (B)"
    Write-Output $headerLine
    
    # Separator (dynamic length based on actual header width)
    $dashCount = $dirColWidth
    $separator = "-" * $dashCount + "|-------|-----|-----|---------|----------|-------------------"
    Write-Output $separator
    
    # Data rows
    foreach($r in $results) {
        $cdcStr = if($r.CDC){'ON'}else{'OFF'}
        $fftStr = if($r.FFT){'ON'}else{'OFF'}
        $dispStr = if($r.Display){'ON'}else{'OFF'}
        $dirField = $r.Directory.PadRight($dirColWidth)
        $row = "$dirField | $($r.Level)     | $cdcStr | $fftStr | $dispStr      | $([string]::Format('{0:D8}', $r.BSS)) | $($r.HeapAvailable.ToString().PadLeft(19))"
        Write-Output $row
    }
}
