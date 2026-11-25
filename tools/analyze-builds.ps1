#
# analyze-builds.ps1
# Purpose: Analyze all build directories and report USB_BRINGUP_LEVEL, features, and memory usage.
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
if($BuildDirs.Count -eq 0) {
    Write-Output "Auto-detecting build directories..."
    $BuildDirs = @(Get-ChildItem -Path $repoRoot -Directory -Filter "build-*" | 
                   Where-Object { Test-Path (Join-Path $_.FullName "CMakeCache.txt") } |
                   Select-Object -ExpandProperty Name)
    if($BuildDirs.Count -eq 0) {
        Write-Output "✗ No build directories found"
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
        
        $ramTotal = 458752  # 448 KiB in bytes
        $ramPct = [math]::Round(100.0 * $bss / $ramTotal, 1)
        $ramFree = $ramTotal - $bss
        
        $result = @{
            Directory    = $dir
            Level        = $level
            CDC          = $cdcEnabled
            FFT          = $fftEnabled
            Display      = $displayEnabled
            Text         = $text
            Data         = $data
            BSS          = $bss
            RAMPercent   = $ramPct
            RAMFree      = $ramFree
            ELF          = $elfFile.Name
        }
        $results += $result
        
        # Display result
        Write-Output "[$($dir)]"
        Write-Output "  Level: $level"
        Write-Output "  Features: CDC=$(if($cdcEnabled){'ON'}else{'OFF'}), FFT=$(if($fftEnabled){'ON'}else{'OFF'}), Display=$(if($displayEnabled){'ON'}else{'OFF'})"
        Write-Output "  ELF: $($elfFile.Name)"
        Write-Output "  Memory: text=$text B, data=$data B, bss=$bss B"
        Write-Output "  RAM: $bss B ($ramPct%) | Free: $ramFree B"
        Write-Output ""
    } else {
        Write-Output "⚠ $($dir): Failed to parse size output"
    }
}

# Summary table
if($results.Count -gt 0) {
    Write-Output "=== Summary Table ==="
    Write-Output ""
    Write-Output "Dir         | Level | CDC | FFT | Display | BSS (B)  | RAM %  | Free (B)"
    Write-Output "------------|-------|-----|-----|---------|----------|--------|----------"
    
    foreach($r in $results) {
        $cdcStr = if($r.CDC){'ON'}else{'OFF'}
        $fftStr = if($r.FFT){'ON'}else{'OFF'}
        $dispStr = if($r.Display){'ON'}else{'OFF'}
        Write-Output "$($r.Directory.PadRight(11)) | $($r.Level)     | $cdcStr | $fftStr | $dispStr      | $([string]::Format('{0:D8}', $r.BSS)) | $($r.RAMPercent.ToString().PadLeft(5))% | $([string]::Format('{0:D8}', $r.RAMFree))"
    }
}
