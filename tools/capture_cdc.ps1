param(
    [Parameter(Mandatory = $true)]
    [string]$Port,
    [Parameter(Mandatory = $true)]
    [string]$Output
)

$ErrorActionPreference = 'Stop'

# Source common build utilities (for path resolution)
$commonBuild = Join-Path $PSScriptRoot "common-build.ps1"
if (-not (Test-Path -LiteralPath $commonBuild -PathType Leaf)) {
    throw "Common build library not found: $commonBuild"
}
. $commonBuild

$serial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$serial.DtrEnable = $true
$serial.ReadTimeout = 250
$serial.WriteTimeout = 2000
$file = $null

try {
    $serial.Open()
    $outputPath = Resolve-PathSafe $Output
    $file = [System.IO.File]::Open($outputPath, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    $unixUs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() * 1000
    $serial.Write("TIME,$unixUs`n")
    $syncTimer = [System.Diagnostics.Stopwatch]::StartNew()
    $syncText = ''
    $synced = $false
    $buffer = [byte[]]::new(8192)
    Write-Host "Recording $Port to $Output. Press Ctrl+C to stop."

    while ($true) {
        try {
            $count = $serial.Read($buffer, 0, $buffer.Length)
        }
        catch [System.TimeoutException] {
            $count = 0
        }
        if ($count -gt 0) {
            $file.Write($buffer, 0, $count)
            $file.Flush()
            if (-not $synced) {
                $syncText += [System.Text.Encoding]::ASCII.GetString($buffer, 0, $count)
                if ($syncText.Contains('#TIME_ERROR,')) {
                    throw 'The device rejected the time synchronization command.'
                }
                if ($syncText.Contains("#TIME_SYNC,$unixUs,")) {
                    $synced = $true
                    Write-Host 'Device clock synchronized.'
                }
                elseif ($syncText.Length -gt 512) {
                    $syncText = $syncText.Substring($syncText.Length - 512)
                }
            }
        }
        if (-not $synced -and $syncTimer.Elapsed.TotalSeconds -ge 10) {
            throw 'No time synchronization acknowledgement received within 10 seconds.'
        }
    }
}
finally {
    if ($null -ne $file) {
        $file.Dispose()
    }
    $serial.Dispose()
}
