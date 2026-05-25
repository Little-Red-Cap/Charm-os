param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 120,
    [string]$Log = ""
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_player_md3_smoke.log"
}

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$RequiredTokens = @(
    "real_md3=1",
    "mock=0",
    "smoke=1/11111",
    "exec_fail=0",
    "co=0",
    "to=0"
)

$LogWriter = $null
$Serial = $null
$MatchedLine = $null
$FailureReason = $null

function Write-CaptureLine {
    param([string]$Line)
    $LogWriter.WriteLine($Line)
    $LogWriter.Flush()
    Write-Host $Line
}

function Test-SmokeLine {
    param([string]$Line)
    foreach ($Token in $RequiredTokens) {
        if ($Line -notlike "*$Token*") {
            return $false
        }
    }
    return $true
}

try {
    $Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $ResolvedLog = [System.IO.Path]::GetFullPath($Log)
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)

    Write-CaptureLine "H747 Lab Player MD3 smoke capture"
    Write-CaptureLine "  port:      $Port"
    Write-CaptureLine "  baud:      $BaudRate"
    Write-CaptureLine "  pyocd:     $($PyOcd.Source)"
    Write-CaptureLine "  probe:     $Probe"
    Write-CaptureLine "  target:    $Target"
    Write-CaptureLine "  frequency: $Frequency"
    Write-CaptureLine "  timeout:   ${TimeoutSeconds}s"
    Write-CaptureLine "  log:       $ResolvedLog"
    Write-CaptureLine ""

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.NewLine = "`n"
    $Serial.ReadTimeout = 250
    $Serial.WriteTimeout = 1000
    $Serial.Open()
    $Serial.DiscardInBuffer()
    $Serial.DiscardOutBuffer()

    Write-CaptureLine "Serial open. Resetting target through pyOCD commander..."
    $PyOcdArgs = @(
        "commander",
        "-u", $Probe,
        "-t", $Target,
        "-f", $Frequency,
        "-c", "reset halt",
        "-c", "go",
        "-c", "exit"
    )
    $PyOcdStdout = [System.IO.Path]::GetTempFileName()
    $PyOcdStderr = [System.IO.Path]::GetTempFileName()
    try {
        $PyOcdProcess = Start-Process `
            -FilePath $PyOcd.Source `
            -ArgumentList $PyOcdArgs `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $PyOcdStdout `
            -RedirectStandardError $PyOcdStderr
        foreach ($Line in (Get-Content -Path $PyOcdStdout -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            Write-CaptureLine "[pyocd:stdout] $Line"
        }
        foreach ($Line in (Get-Content -Path $PyOcdStderr -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            Write-CaptureLine "[pyocd:stderr] $Line"
        }
        $PyOcdExit = $PyOcdProcess.ExitCode
    } finally {
        Remove-Item -LiteralPath $PyOcdStdout -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $PyOcdStderr -Force -ErrorAction SilentlyContinue
    }
    if ($PyOcdExit -ne 0) {
        $FailureReason = "pyocd commander failed with exit code $PyOcdExit"
    } else {
        Write-CaptureLine "Capturing serial until a valid smoke line appears..."
        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while ([DateTime]::UtcNow -lt $Deadline) {
            try {
                $Line = $Serial.ReadLine().TrimEnd("`r", "`n")
            } catch [System.TimeoutException] {
                continue
            }

            if ($Line.Length -eq 0) {
                continue
            }

            Write-CaptureLine $Line
            if (Test-SmokeLine $Line) {
                $MatchedLine = $Line
                break
            }
        }

        if ($null -eq $MatchedLine) {
            $FailureReason = "timed out before matching a valid smoke line"
        }
    }
} finally {
    if (($null -ne $Serial) -and $Serial.IsOpen) {
        $Serial.Close()
    }
    if ($null -ne $Serial) {
        $Serial.Dispose()
    }
    if ($null -ne $LogWriter) {
        $LogWriter.Dispose()
    }
}

if ($null -ne $MatchedLine) {
    Write-Host ""
    Write-Host "Smoke capture passed."
    Write-Host "Matched line: $MatchedLine"
    Write-Host "Log: $Log"
    exit 0
}

Write-Host ""
Write-Host "Smoke capture failed: $FailureReason"
Write-Host "Log: $Log"
exit 1
