param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 90,
    [string]$Command = "app smoke",
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
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_app_lab_smoke.log"
}

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$RequiredTokens = @(
    "status: monitor=ready display_ready=true input_ready=true file_backed=false",
    "[app-smoke] builtin=ok",
    "[app-smoke] install=ok",
    "[app-smoke] qspi_named=ok",
    "[app-smoke] qspi_raw=ok",
    "[app-smoke] generic_stub=ok",
    "[app-smoke] result=ok",
    "status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true",
    "status: store install attempted=true ready=true code=ok",
    "status: last request=/not-supported image=/not-supported source=file-backed stage=file-backed code=not_supported exited=false exit=0 backend=0",
    "status: elf_load=[0x24070000,0x24072000) size=8192"
)

$LogWriter = $null
$Serial = $null
$FailureReason = $null
$SentSmokeCommand = $false
$SentStatusCommand = $false
$RawCapture = New-Object System.Text.StringBuilder

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

function Write-PrefixedBlock {
    param(
        [string]$Prefix,
        [string]$Text
    )

    if ([string]::IsNullOrEmpty($Text)) {
        return
    }

    foreach ($Line in ([System.Text.RegularExpressions.Regex]::Split($Text, "\r?\n"))) {
        if ([string]::IsNullOrEmpty($Line)) {
            continue
        }
        Write-CaptureText "$Prefix$Line`n"
    }
}

function Invoke-LoggedProcess {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$Label
    )

    $StdoutPath = [System.IO.Path]::GetTempFileName()
    $StderrPath = [System.IO.Path]::GetTempFileName()
    try {
        $Process = Start-Process `
            -FilePath $FilePath `
            -ArgumentList $ArgumentList `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $StdoutPath `
            -RedirectStandardError $StderrPath

        return @{
            Label = $Label
            ExitCode = $Process.ExitCode
            Stdout = Get-Content -Path $StdoutPath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            Stderr = Get-Content -Path $StderrPath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            Arguments = @($ArgumentList)
        }
    } finally {
        Remove-Item -LiteralPath $StdoutPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $StderrPath -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-PyOcdReset {
    param(
        [string]$Probe,
        [string]$Target,
        [string]$Frequency
    )

    $PyOcd = Get-Command pyocd -ErrorAction Stop
    $Attempts = New-Object System.Collections.Generic.List[hashtable]
    $Specs = @(
        @{
            Label = "pyocd reset"
            Arguments = @(
                "reset",
                "-u", $Probe,
                "-t", $Target,
                "-f", $Frequency,
                "-M", "halt",
                "-m", "hw"
            )
        },
        @{
            Label = "pyocd commander"
            Arguments = @(
                "commander",
                "-u", $Probe,
                "-t", $Target,
                "-f", $Frequency,
                "-c", "reset halt",
                "-c", "go",
                "-c", "exit"
            )
        }
    )

    foreach ($Spec in $Specs) {
        $Result = Invoke-LoggedProcess -FilePath $PyOcd.Source -ArgumentList $Spec.Arguments -Label $Spec.Label
        [void]$Attempts.Add($Result)
        if ($Result.ExitCode -eq 0) {
            return @{
                ToolPath = $PyOcd.Source
                Succeeded = $true
                Selected = $Result
                Attempts = @($Attempts.ToArray())
            }
        }
    }

    return @{
        ToolPath = $PyOcd.Source
        Succeeded = $false
        Selected = $null
        Attempts = @($Attempts.ToArray())
    }
}

function Write-ResetTranscript {
    param([hashtable[]]$Attempts)

    foreach ($Attempt in $Attempts) {
        Write-CaptureText "[reset] $($Attempt.Label) exit=$($Attempt.ExitCode)`n"
        Write-PrefixedBlock -Prefix "[reset:$($Attempt.Label):stdout] " -Text $Attempt.Stdout
        Write-PrefixedBlock -Prefix "[reset:$($Attempt.Label):stderr] " -Text $Attempt.Stderr
    }
}

function Format-ResetFailureReason {
    param([hashtable[]]$Attempts)

    if ($null -eq $Attempts -or $Attempts.Count -eq 0) {
        return "no reset attempts were executed"
    }

    $Parts = foreach ($Attempt in $Attempts) {
        "$($Attempt.Label) exit=$($Attempt.ExitCode)"
    }
    return ($Parts -join "; ")
}

function Test-AllTokensPresent {
    param([string]$Text)

    return (Get-MissingTokens -Text $Text).Count -eq 0
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

    Write-Host "App Lab smoke log validation"
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
status: monitor=ready display_ready=true input_ready=true file_backed=false
[app-smoke] builtin=ok
[app-smoke] install=ok
[app-smoke] qspi_named=ok
[app-smoke] qspi_raw=ok
[app-smoke] generic_stub=ok
[app-smoke] result=ok
status: source embedded entries=2 readable=true valid=true qspi_ready=true qspi_readable=true qspi_valid=true
status: store install attempted=true ready=true code=ok
status: last request=/not-supported image=/not-supported source=file-backed stage=file-backed code=not_supported exited=false exit=0 backend=0
status: elf_load=[0x24070000,0x24072000) size=8192
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

    $FailingLog = $PassingLog.Replace("[app-smoke] qspi_raw=ok", "[app-smoke] qspi_raw=failed")
    $FailingMissing = Get-MissingTokens -Text $FailingLog
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "[app-smoke] qspi_raw=ok") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "App Lab capture self-test passed."
    return 0
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

    Write-CaptureText "H747 Lab App Lab smoke capture`n"
    Write-CaptureText "  port:      $Port`n"
    Write-CaptureText "  baud:      $BaudRate`n"
    Write-CaptureText "  probe:     $Probe`n"
    Write-CaptureText "  target:    $Target`n"
    Write-CaptureText "  frequency: $Frequency`n"
    Write-CaptureText "  timeout:   ${TimeoutSeconds}s`n"
    Write-CaptureText "  command:   $Command`n"
    Write-CaptureText "  log:       $ResolvedLog`n`n"

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
        Write-CaptureText "Serial open. Resetting target through pyOCD reset fallback chain...`n"
        $Reset = Invoke-PyOcdReset -Probe $Probe -Target $Target -Frequency $Frequency
        Write-ResetTranscript -Attempts $Reset.Attempts
        if (-not $Reset.Succeeded) {
            $FailureReason = "reset failed: $(Format-ResetFailureReason -Attempts $Reset.Attempts)"
        }
    } else {
        Write-CaptureText "Serial open. NoReset set; waiting for an existing prompt/log...`n"
    }

    if ($null -eq $FailureReason) {
        Write-CaptureText "Capturing serial and sending commands at app-lab prompt...`n"
        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $SendAnywayAt = [DateTime]::UtcNow.AddSeconds(8)
        while ([DateTime]::UtcNow -lt $Deadline) {
            $Chunk = $Serial.ReadExisting()
            if ($Chunk.Length -gt 0) {
                [void]$RawCapture.Append($Chunk)
                Write-CaptureText $Chunk
            }

            $Captured = $RawCapture.ToString()
            if ((-not $SentSmokeCommand) -and
                (($Captured -like "*app-lab>*") -or ([DateTime]::UtcNow -gt $SendAnywayAt))) {
                $Serial.WriteLine($Command)
                $SentSmokeCommand = $true
                Write-CaptureText "`n[capture] sent: $Command`n"
            }

            if ($SentSmokeCommand -and (-not $SentStatusCommand) -and
                (Test-ContainsLiteral -Text $Captured -Needle "[app-smoke] result=ok")) {
                $Serial.WriteLine("app status")
                $SentStatusCommand = $true
                Write-CaptureText "`n[capture] sent: app status`n"
            }

            if (Test-AllTokensPresent -Text $Captured) {
                break
            }
            Start-Sleep -Milliseconds 50
        }

        $CapturedFinal = $RawCapture.ToString()
        $Missing = Get-MissingTokens -Text $CapturedFinal
        if ($Missing.Count -eq 0) {
            Write-CaptureText "`nSmoke capture passed.`n"
            exit 0
        }
        $FailureReason = "missing token: $($Missing[0])"
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
