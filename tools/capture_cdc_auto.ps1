param(
    [ValidateNotNullOrEmpty()]
    [string]$Port = 'COM3',
    [ValidateNotNullOrEmpty()]
    [string]$OutputDir = '.',
    [ValidatePattern('^[^\\/:*?"<>|\x00-\x1f]+$')]
    [string]$Prefix = 'cap',
    [long]$LogMaxSizeBytes = (10 * 1MB)  # Rotate log at 10 MB
)

$ErrorActionPreference = 'Stop'

# Source common build utilities
$commonBuild = Join-Path $PSScriptRoot "common-build.ps1"
if (-not (Test-Path -LiteralPath $commonBuild -PathType Leaf)) {
    throw "Common build library not found: $commonBuild"
}
. $commonBuild

$captureScript = Join-Path $PSScriptRoot 'capture_cdc.ps1'
if (-not (Test-Path -LiteralPath $captureScript -PathType Leaf)) {
    throw "Capture script not found: $captureScript"
}

$directory = Resolve-PathSafe $OutputDir
New-LogDirectory $directory
$eventLog = Join-Path $directory 'capture_events.log'
$encoding = [System.Text.UTF8Encoding]::new($false)

function Write-CaptureEvent {
    param([string]$Event, [string]$Detail)
    
    # Rotate log if needed
    Rotate-LogFile $eventLog -MaxSizeBytes $LogMaxSizeBytes | Out-Null
    
    $timestamp = [DateTimeOffset]::Now.ToString('yyyy-MM-ddTHH:mm:ss.fffzzz')
    $detailLine = $Detail -replace '[\r\n]+', ' '
    $line = "$timestamp $Event $detailLine"
    [System.IO.File]::AppendAllText($eventLog, $line + [Environment]::NewLine, $encoding)
    Write-Host $line
}

$sequence = 0
Write-CaptureEvent 'WRAPPER_START' "port=$Port dir=$directory"
$wait = [System.Diagnostics.Stopwatch]::StartNew()
while ($true) {
    $missingSeen = $false
    while ([System.IO.Ports.SerialPort]::GetPortNames() -notcontains $Port) {
        if (-not $missingSeen) {
            Write-Host "Waiting for $Port. Press Ctrl+C to stop."
            $missingSeen = $true
        }
        Start-Sleep -Milliseconds 100
    }

    if ($missingSeen) {
        Write-CaptureEvent 'PORT_BACK' ("port=$Port wait_seconds={0:F3}" -f $wait.Elapsed.TotalSeconds)
    }
    elseif ($sequence -gt 0) {
        Write-CaptureEvent 'PORT_PRESENT' ("port=$Port wait_seconds={0:F3}; disappearance not observed" -f $wait.Elapsed.TotalSeconds)
    }
    Start-Sleep -Milliseconds 500

    do {
        $sequence++
        $name = '{0}_{1}_{2:D4}.csv' -f $Prefix, (Get-Date -Format 'yyyyMMdd_HHmmss_fff'), $sequence
        $output = Join-Path $directory $name
    } while (Test-Path -LiteralPath $output)

    Write-CaptureEvent 'START' "port=$Port file=$name"
    $endReason = 'interrupted'
    try {
        & $captureScript -Port $Port -Output $output
        $endReason = 'completed'
    }
    catch [System.Management.Automation.PipelineStoppedException] {
        throw
    }
    catch {
        $endReason = "error=$($_.Exception.Message)"
        Write-Warning "Capture failed; retrying: $($_.Exception.Message)"
    }
    finally {
        Write-CaptureEvent 'END' "port=$Port file=$name $endReason"
    }
    $wait.Restart()
    Start-Sleep -Seconds 1
}
