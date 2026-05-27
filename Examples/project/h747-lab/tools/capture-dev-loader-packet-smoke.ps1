param(
    [string]$Commands = "",
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 120,
    [string]$Log = "",
    [string]$ValidateLog = "",
    [switch]$NoReset,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ([string]::IsNullOrWhiteSpace($Commands)) {
    throw "Commands file is required."
}
if (-not (Test-Path -LiteralPath $Commands)) {
    throw "Commands file not found: $Commands"
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_packet_smoke.log"
}

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$CommandLines = Get-Content -LiteralPath $Commands -Encoding UTF8 |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
if ($CommandLines.Count -eq 0) {
    throw "Commands file is empty: $Commands"
}

$RequiredTokens = @(
    "dev: packet transport=ok",
    "dev: packet last=ok kind=launch_dry_run",
    "dev: stage=launch_ready code=ok received=64"
)

$LogWriter = $null
$Serial = $null
$RawCapture = New-Object System.Text.StringBuilder
$LineIndex = 0
$StatusSent = $false
$LastSendAt = [DateTime]::MinValue
$FailureReason = $null

function Test-ContainsLiteral {
    param(
        [string]$Text,
        [string]$Needle
    )

    if ($null -eq $Text -or $null -eq $Needle) {
        return $false
    }
    return $Text.IndexOf($Needle, [System.StringComparison]::Ordinal) -ge 0
}

function Get-MissingTokens {
    param([string]$Text)

    $Missing = New-Object System.Collections.Generic.List[string]
    foreach ($Token in $RequiredTokens) {
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }
    return ,$Missing.ToArray()
}

function Validate-LogFile {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "ValidateLog path must not be empty."
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Log file not found: $Path"
    }

    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Text = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    $Missing = Get-MissingTokens -Text $Text

    Write-Host "Dev Loader packet smoke log validation"
    Write-Host "  log: $ResolvedPath"
    if ($Missing.Count -eq 0) {
        Write-Host ""
        Write-Host "Validation passed."
        return 0
    }

    Write-Host ""
    Write-Host "Validation failed. Missing tokens:"
    foreach ($Token in $Missing) {
        Write-Host "  - $Token"
    }
    return 1
}

function Get-SyntheticPassingLog {
    return @"
dev: packet transport=ok buffered=0 consumed=28 dispatched=1
dev: packet last=ok kind=launch_dry_run next_seq=7 cursor=64 active=1
dev: stage=launch_ready code=ok received=64 crc=0x100ece8c/0x100ece8c
"@
}

function Invoke-SelfTest {
    $PassingLog = Get-SyntheticPassingLog
    $PassingMissing = Get-MissingTokens -Text $PassingLog
    if ($PassingMissing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing log missed tokens."
        foreach ($Token in $PassingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $FailingLog = $PassingLog.Replace("dev: stage=launch_ready code=ok received=64", "dev: stage=verified code=ok received=64")
    $FailingMissing = Get-MissingTokens -Text $FailingLog
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "dev: stage=launch_ready code=ok received=64") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Dev Loader packet capture self-test passed."
    return 0
}

function Write-CaptureText {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) {
        return
    }
    if ($null -ne $script:LogWriter) {
        $script:LogWriter.Write($Text)
        $script:LogWriter.Flush()
    }
    Write-Host -NoNewline $Text
}

function Invoke-PyOcdReset {
    param(
        [string]$Probe,
        [string]$Target,
        [string]$Frequency
    )

    $PyOcd = Get-Command pyocd -ErrorAction Stop
    $PyOcdArgs = @(
        "commander",
        "-u", $Probe,
        "-t", $Target,
        "-f", $Frequency,
        "-c", "reset",
        "-c", "go",
        "-c", "exit"
    )
    $Stdout = [System.IO.Path]::GetTempFileName()
    $Stderr = [System.IO.Path]::GetTempFileName()
    try {
        $Process = Start-Process `
            -FilePath $PyOcd.Source `
            -ArgumentList $PyOcdArgs `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $Stdout `
            -RedirectStandardError $Stderr
        return @{
            ExitCode = $Process.ExitCode
            Stdout = Get-Content -Path $Stdout -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            Stderr = Get-Content -Path $Stderr -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        }
    } finally {
        Remove-Item -LiteralPath $Stdout -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $Stderr -Force -ErrorAction SilentlyContinue
    }
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog)
}

try {
    $Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $ResolvedLog = [System.IO.Path]::GetFullPath($Log)
    $ResolvedCommands = (Resolve-Path -LiteralPath $Commands).Path
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)

    Write-CaptureText "H747 Lab Dev Loader packet smoke capture`n"
    Write-CaptureText "  port:     $Port`n"
    Write-CaptureText "  baud:     $BaudRate`n"
    Write-CaptureText "  timeout:  ${TimeoutSeconds}s`n"
    Write-CaptureText "  commands: $ResolvedCommands`n"
    Write-CaptureText "  lines:    $($CommandLines.Count)`n"
    Write-CaptureText "  log:      $ResolvedLog`n`n"

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.NewLine = "`n"
    $Serial.ReadTimeout = 200
    $Serial.WriteTimeout = 1000
    $Serial.Open()
    $Serial.DiscardInBuffer()
    $Serial.DiscardOutBuffer()

    if (-not $NoReset) {
        Write-CaptureText "Serial open. Resetting target through pyOCD commander...`n"
        $Reset = Invoke-PyOcdReset -Probe $Probe -Target $Target -Frequency $Frequency
        if (-not [string]::IsNullOrEmpty($Reset.Stdout)) {
            Write-CaptureText "[pyocd:stdout] $($Reset.Stdout)`n"
        }
        if (-not [string]::IsNullOrEmpty($Reset.Stderr)) {
            Write-CaptureText "[pyocd:stderr] $($Reset.Stderr)`n"
        }
        if ($Reset.ExitCode -ne 0) {
            $FailureReason = "pyocd commander failed with exit code $($Reset.ExitCode)"
        }
    }

    if ($null -eq $FailureReason) {
        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while ([DateTime]::UtcNow -lt $Deadline) {
            $Chunk = $Serial.ReadExisting()
            if ($Chunk.Length -gt 0) {
                [void]$RawCapture.Append($Chunk)
                Write-CaptureText $Chunk
            }

            $Captured = $RawCapture.ToString()
            $PromptSeen = $Captured -like "*dev-loader>*"
            $SpacingOk = ([DateTime]::UtcNow - $LastSendAt).TotalMilliseconds -gt 80
            if ($PromptSeen -and $SpacingOk) {
                if ($LineIndex -lt $CommandLines.Count) {
                    $Line = $CommandLines[$LineIndex]
                    $Serial.WriteLine($Line)
                    ++$LineIndex
                    $LastSendAt = [DateTime]::UtcNow
                    Write-CaptureText "`n[capture] sent packet line $LineIndex/$($CommandLines.Count)`n"
                } elseif (-not $StatusSent) {
                    $Serial.WriteLine("dev packet status")
                    $StatusSent = $true
                    $LastSendAt = [DateTime]::UtcNow
                    Write-CaptureText "`n[capture] sent: dev packet status`n"
                }
            }

            if ((Get-MissingTokens -Text $Captured).Count -eq 0) {
                Write-CaptureText "`nPacket smoke capture passed.`n"
                exit 0
            }
            Start-Sleep -Milliseconds 20
        }

        $Missing = Get-MissingTokens -Text $RawCapture.ToString()
        if ($Missing.Count -gt 0) {
            $FailureReason = "missing token: $($Missing[0])"
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

Write-Host ""
Write-Host "Packet smoke capture failed: $FailureReason"
Write-Host "Log: $Log"
exit 1
