param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 60,
    [string]$Log = "",
    [string]$ValidateLog = "",
    [switch]$NoReset,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_smoke.log"
}

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Commands = @(
    "dev status",
    "dev begin 16",
    "dev fill 0xaa 16",
    "dev verify",
    "dev launch dry-run"
)

$RequiredTokens = @(
    "dev_loader: resident RAM dev-loader skeleton ready",
    "dev: receive-arena name=",
    "dev: sdram2 ready=",
    "dev: stage=receiving code=ok received=0",
    "dev: stage=complete code=ok received=16",
    "dev: stage=verified code=ok received=16",
    "dev: stage=launch_ready code=ok received=16"
)

$LogWriter = $null
$Serial = $null
$FailureReason = $null
$RawCapture = New-Object System.Text.StringBuilder
$CommandIndex = 0
$LastCommandAt = [DateTime]::MinValue

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

    Write-Host "Dev Loader smoke log validation"
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
dev_loader: resident RAM dev-loader skeleton ready
dev: ram base=0xd0000000 capacity=262144 cursor=0
dev: receive-arena name=sdram2_receive_buffer addr=0xd0000000 expected=0xd0000000 size=262144 align=32
dev: sdram2 ready=1 init=1 smoke=1 base=0xd0000000 size=33554432
dev: stage=receiving code=ok received=0 crc=0x00000000/0x00000000
dev: stage=complete code=ok received=16 crc=0xc79b40e0/0x00000000
dev: stage=verified code=ok received=16 crc=0xc79b40e0/0x00000000
dev: stage=launch_ready code=ok received=16 crc=0xc79b40e0/0x00000000
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

    $FailingLog = $PassingLog.Replace("dev: stage=launch_ready code=ok received=16", "dev: stage=verified code=ok received=16")
    $FailingMissing = Get-MissingTokens -Text $FailingLog
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "dev: stage=launch_ready code=ok received=16") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Dev Loader capture self-test passed."
    return 0
}

function Write-CaptureText {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) {
        return
    }
    $LogWriter.Write($Text)
    $LogWriter.Flush()
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
        return @{
            ExitCode = $PyOcdProcess.ExitCode
            Stdout = Get-Content -Path $PyOcdStdout -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            Stderr = Get-Content -Path $PyOcdStderr -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        }
    } finally {
        Remove-Item -LiteralPath $PyOcdStdout -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $PyOcdStderr -Force -ErrorAction SilentlyContinue
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
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)

    Write-CaptureText "H747 Lab Dev Loader smoke capture`n"
    Write-CaptureText "  port:    $Port`n"
    Write-CaptureText "  baud:    $BaudRate`n"
    Write-CaptureText "  timeout: ${TimeoutSeconds}s`n"
    Write-CaptureText "  log:     $ResolvedLog`n`n"

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
            $CanSend = ($Captured -like "*dev-loader>*") -or
                (($CommandIndex -eq 0) -and ([DateTime]::UtcNow -gt $Deadline.AddSeconds(-($TimeoutSeconds - 5))))
            $SpacingOk = ([DateTime]::UtcNow - $LastCommandAt).TotalMilliseconds -gt 750
            if (($CommandIndex -lt $Commands.Count) -and $CanSend -and $SpacingOk) {
                $Command = $Commands[$CommandIndex]
                $Serial.WriteLine($Command)
                $LastCommandAt = [DateTime]::UtcNow
                $CommandIndex += 1
                Write-CaptureText "`n[capture] sent: $Command`n"
            }

            if ((Get-MissingTokens -Text $Captured).Count -eq 0) {
                Write-CaptureText "`nSmoke capture passed.`n"
                exit 0
            }
            Start-Sleep -Milliseconds 50
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
Write-Host "Smoke capture failed: $FailureReason"
Write-Host "Log: $Log"
exit 1
