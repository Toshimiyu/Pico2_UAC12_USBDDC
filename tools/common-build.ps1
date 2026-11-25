#
# common-build.ps1
# Purpose: Shared utilities for build and capture scripts
# 
# Common Functions:
# - Setup-BuildEnvironment: Configure PATH for build tools
# - Resolve-PathSafe: Safe path resolution handling spaces and special characters
# - New-LogDirectory: Ensure log directory exists
# - Rotate-LogFile: Archive log file when it exceeds size limit
#

# Error handling
$ErrorActionPreference = 'Stop'

function Setup-BuildEnvironment {
    <#
    .SYNOPSIS
    Configure environment PATH for build tools (CMake, Ninja, ARM toolchain)
    
    .PARAMETER ToolchainPath
    Path to ARM toolchain (default: $USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin)
    
    .PARAMETER NinjaPath
    Path to Ninja build tool (default: $USERPROFILE\.pico-sdk\ninja\v1.13.2)
    
    .EXAMPLE
    Setup-BuildEnvironment
    #>
    param(
        [string]$ToolchainPath = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1\bin",
        [string]$NinjaPath = "$env:USERPROFILE\.pico-sdk\ninja\v1.13.2"
    )
    
    if(!(Test-Path $ToolchainPath)) {
        throw "Toolchain path not found: $ToolchainPath"
    }
    if(!(Test-Path $NinjaPath)) {
        throw "Ninja path not found: $NinjaPath"
    }
    
    $env:PATH = "$NinjaPath;$ToolchainPath;$env:PATH"
}

function Resolve-PathSafe {
    <#
    .SYNOPSIS
    Safely resolve a PowerShell path, handling spaces and special characters
    
    .PARAMETER Path
    Path to resolve (may be relative or contain wildcards)
    
    .EXAMPLE
    $resolved = Resolve-PathSafe ".\build-normal"
    #>
    param([string]$Path)
    
    if([string]::IsNullOrWhiteSpace($Path)) {
        throw "Path cannot be null or empty"
    }
    
    return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
}

function New-LogDirectory {
    <#
    .SYNOPSIS
    Ensure a log directory exists, creating it if necessary
    
    .PARAMETER LogDir
    Directory path to create
    
    .EXAMPLE
    New-LogDirectory "build-logs"
    #>
    param([string]$LogDir)
    
    if(![string]::IsNullOrWhiteSpace($LogDir) -and !(Test-Path $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
    }
}

function Rotate-LogFile {
    <#
    .SYNOPSIS
    Archive a log file if it exceeds the specified size limit
    
    .PARAMETER LogFile
    Path to log file
    
    .PARAMETER MaxSizeBytes
    Maximum file size before rotation (default: 10 MB)
    
    .EXAMPLE
    Rotate-LogFile "capture_events.log" -MaxSizeBytes (5 * 1MB)
    
    .NOTES
    When rotated, creates a backup file with suffix .yyyyMMdd-HHmmss.bak
    #>
    param(
        [string]$LogFile,
        [long]$MaxSizeBytes = (10 * 1MB)
    )
    
    if(![string]::IsNullOrWhiteSpace($LogFile) -and (Test-Path $LogFile)) {
        $fileInfo = Get-Item $LogFile
        if($fileInfo.Length -gt $MaxSizeBytes) {
            $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
            $backupPath = "$LogFile.$timestamp.bak"
            Move-Item $LogFile $backupPath -Force
            return $true
        }
    }
    
    return $false
}

function Test-RequiredTools {
    <#
    .SYNOPSIS
    Validate that required build tools are available in PATH
    
    .PARAMETER Tools
    Array of tool executables to check
    
    .EXAMPLE
    Test-RequiredTools @("cmake.exe", "ninja.exe", "arm-none-eabi-size.exe")
    
    .NOTES
    Throws an exception if any required tool is not found
    #>
    param([string[]]$Tools)
    
    foreach($tool in $Tools) {
        if(!(Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "Required tool not found in PATH: $tool"
        }
    }
}

function Save-BuildLog {
    <#
    .SYNOPSIS
    Write a message to both console and log file
    
    .PARAMETER LogFile
    Path to log file (if empty or $null, writes to console only)
    
    .PARAMETER Message
    Message to log
    
    .PARAMETER NoConsole
    If specified, suppress console output
    
    .EXAMPLE
    Save-BuildLog "build.log" "Build started"
    #>
    param(
        [string]$LogFile,
        [string]$Message,
        [switch]$NoConsole
    )
    
    if(!$NoConsole) {
        Write-Host $Message
    }
    
    if(![string]::IsNullOrWhiteSpace($LogFile) -and (Test-Path (Split-Path $LogFile))) {
        Add-Content -Path $LogFile -Value $Message -ErrorAction SilentlyContinue
    }
}
