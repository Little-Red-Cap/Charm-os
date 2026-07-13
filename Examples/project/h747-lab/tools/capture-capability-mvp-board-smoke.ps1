[CmdletBinding()]
param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 20,
    [string]$Bin = "",
    [string]$Log = "",
    [string]$ValidateLog = "",
    [switch]$SkipFlash,
    [switch]$NoReset,
    [switch]$DryRun,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

if ($TimeoutSeconds -le 0) {
    throw "invalid_timeout: TimeoutSeconds must be greater than zero"
}

$RequiredTokens = @(
    "charm-mvp: ok",
    "[charm-capability-mvp-h747] positive=ok timestamp=424242 checksum=0x49b880f0",
    "[charm-capability-mvp-h747] missing=missing_binding start_count=0",
    "[charm-capability-mvp-h747] ok"
)
$ForbiddenTokens = @(
    "[charm-capability-mvp-h747] failed",
    "[charm-capability-mvp-h747] error=",
    "fault: HardFault",
    "fault: MemManage",
    "fault: BusFault",
    "fault: UsageFault"
)

function Test-ContainsLiteral {
    param(
        [AllowNull()][AllowEmptyString()][string]$Text,
        [string]$Needle
    )

    if ($null -eq $Text) {
        $Text = ""
    }
    return $Text.IndexOf($Needle, [System.StringComparison]::Ordinal) -ge 0
}

function Test-BoardEvidence {
    param([AllowNull()][AllowEmptyString()][string]$Text)

    $Missing = [System.Collections.Generic.List[string]]::new()
    foreach ($Token in $RequiredTokens) {
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            $Missing.Add($Token)
        }
    }

    $Forbidden = [System.Collections.Generic.List[string]]::new()
    foreach ($Token in $ForbiddenTokens) {
        if (Test-ContainsLiteral -Text $Text -Needle $Token) {
            $Forbidden.Add($Token)
        }
    }

    return [pscustomobject]@{
        Ok = ($Missing.Count -eq 0 -and $Forbidden.Count -eq 0)
        Missing = @($Missing)
        Forbidden = @($Forbidden)
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
            -WindowStyle Hidden `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $StdoutPath `
            -RedirectStandardError $StderrPath
        return [pscustomobject]@{
            Label = $Label
            ExitCode = $Process.ExitCode
            Stdout = Get-Content -LiteralPath $StdoutPath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
            Stderr = Get-Content -LiteralPath $StderrPath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        }
    } finally {
        Remove-Item -LiteralPath $StdoutPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $StderrPath -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-PyOcdReset {
    param(
        [string]$PyOcd,
        [string]$ProbeId,
        [string]$TargetName,
        [string]$ClockFrequency
    )

    $Attempts = [System.Collections.Generic.List[object]]::new()
    $Specs = @(
        [pscustomobject]@{
            Label = "pyocd commander"
            Arguments = @(
                "commander", "-u", $ProbeId, "-t", $TargetName,
                "-f", $ClockFrequency,
                "-c", "reset halt", "-c", "go", "-c", "exit"
            )
        },
        [pscustomobject]@{
            Label = "pyocd reset"
            Arguments = @(
                "reset", "-u", $ProbeId, "-t", $TargetName,
                "-f", $ClockFrequency, "-M", "halt", "-m", "hw"
            )
        }
    )

    foreach ($Spec in $Specs) {
        $Result = Invoke-LoggedProcess `
            -FilePath $PyOcd `
            -ArgumentList $Spec.Arguments `
            -Label $Spec.Label
        $Attempts.Add($Result)
        if ($Result.ExitCode -eq 0) {
            return [pscustomobject]@{ Ok = $true; Attempts = @($Attempts) }
        }
    }
    return [pscustomobject]@{ Ok = $false; Attempts = @($Attempts) }
}

function Invoke-SelfTest {
    $Passing = @"
charm-mvp: ok
[charm-capability-mvp-h747] positive=ok timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-h747] missing=missing_binding start_count=0
[charm-capability-mvp-h747] ok
"@
    if (-not (Test-BoardEvidence -Text $Passing).Ok) {
        throw "self_test_failed: passing evidence rejected"
    }
    foreach ($Token in $RequiredTokens) {
        if ((Test-BoardEvidence -Text ($Passing.Replace($Token, ""))).Ok) {
            throw "self_test_failed: missing token accepted: $Token"
        }
    }
    foreach ($Token in $ForbiddenTokens) {
        if ((Test-BoardEvidence -Text ($Passing + "`n$Token")).Ok) {
            throw "self_test_failed: forbidden token accepted: $Token"
        }
    }
    Write-Output "[charm-capability-mvp-h747-capture-self-test] ok"
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    if (-not (Test-Path -LiteralPath $ValidateLog -PathType Leaf)) {
        throw "validate_log_missing: $ValidateLog"
    }
    $Validation = Test-BoardEvidence -Text (Get-Content -LiteralPath $ValidateLog -Raw -Encoding UTF8)
    if (-not $Validation.Ok) {
        throw "validate_log_failed: missing=$($Validation.Missing -join ',') forbidden=$($Validation.Forbidden -join ',')"
    }
    Write-Output "[charm-capability-mvp-h747-log] ok path=$((Resolve-Path -LiteralPath $ValidateLog).Path)"
    exit 0
}

$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Bin)) {
    $Bin = Join-Path $ProjectRoot "cmake-build-h747-lab-capability-mvp-debug\h747_lab_capability_mvp.bin"
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-capability-mvp-debug\h747_lab_capability_mvp_board.log"
}

$ResolvedBin = [System.IO.Path]::GetFullPath($Bin)
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)

if ($DryRun) {
    Write-Output "[charm-capability-mvp-h747-capture-dry-run]"
    Write-Output "  bin=$ResolvedBin"
    Write-Output "  log=$ResolvedLog"
    Write-Output "  port=$Port baud=$BaudRate"
    Write-Output "  probe=$Probe target=$Target frequency=$Frequency"
    Write-Output "  flash=$(-not $SkipFlash) reset=$(-not $NoReset)"
    exit 0
}

if (-not (Test-Path -LiteralPath $ResolvedBin -PathType Leaf)) {
    throw "firmware_missing: $ResolvedBin"
}

$LogDirectory = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDirectory)) {
    New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
}

$PyOcd = (Get-Command pyocd -ErrorAction Stop).Source
$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$Writer = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
$Serial = $null
$Capture = [System.Text.StringBuilder]::new()

function Write-EvidenceText {
    param([AllowEmptyString()][string]$Text)
    if ([string]::IsNullOrEmpty($Text)) {
        return
    }
    $Writer.Write($Text)
    $Writer.Flush()
    Write-Host -NoNewline $Text
}

try {
    Write-EvidenceText "H747 Capability MVP board capture`n"
    Write-EvidenceText "  bin:       $ResolvedBin`n"
    Write-EvidenceText "  port:      $Port`n"
    Write-EvidenceText "  probe:     $Probe`n"
    Write-EvidenceText "  target:    $Target`n"
    Write-EvidenceText "  frequency: $Frequency`n`n"

    if (-not $SkipFlash) {
        $Load = Invoke-LoggedProcess `
            -FilePath $PyOcd `
            -Label "pyocd load" `
            -ArgumentList @(
                "load", "-u", $Probe, "-t", $Target, "-f", $Frequency,
                "-M", "under-reset",
                "-O", "keep_unwritten=false",
                "-O", "smart_flash=false",
                "-O", "fast_program=false",
                "--format", "bin", "-e", "sector", "-a", "0x08000000",
                $ResolvedBin
            )
        Write-EvidenceText "[flash] exit=$($Load.ExitCode)`n$($Load.Stdout)$($Load.Stderr)"
        if ($Load.ExitCode -ne 0) {
            throw "flash_failed: pyocd exit=$($Load.ExitCode)"
        }
    }

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.ReadTimeout = 200
    $Serial.Open()
    $Serial.DiscardInBuffer()

    if (-not $NoReset) {
        $Reset = Invoke-PyOcdReset `
            -PyOcd $PyOcd `
            -ProbeId $Probe `
            -TargetName $Target `
            -ClockFrequency $Frequency
        foreach ($Attempt in $Reset.Attempts) {
            Write-EvidenceText "[reset] $($Attempt.Label) exit=$($Attempt.ExitCode)`n$($Attempt.Stdout)$($Attempt.Stderr)"
        }
        if (-not $Reset.Ok) {
            throw "reset_failed: all pyocd reset methods failed"
        }
    }

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            $Capture.Append($Chunk) | Out-Null
            Write-EvidenceText $Chunk
        }
        $Current = Test-BoardEvidence -Text $Capture.ToString()
        if ($Current.Ok) {
            break
        }
        Start-Sleep -Milliseconds 50
    }

    $Validation = Test-BoardEvidence -Text $Capture.ToString()
    if (-not $Validation.Ok) {
        throw "board_evidence_failed: missing=$($Validation.Missing -join ',') forbidden=$($Validation.Forbidden -join ',')"
    }
    Write-EvidenceText "`n[charm-capability-mvp-h747-capture] ok log=$ResolvedLog`n"
} finally {
    if ($null -ne $Serial -and $Serial.IsOpen) {
        $Serial.Close()
    }
    if ($null -ne $Serial) {
        $Serial.Dispose()
    }
    $Writer.Dispose()
}
