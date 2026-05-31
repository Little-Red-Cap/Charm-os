param(
    [string]$PacketStream = "",
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [int]$ControlBaudRate = 115200,
    [int]$UsbBaudRate = 115200,
    [int]$TimeoutSeconds = 45,
    [int]$UsbEnumerateTimeoutSeconds = 10,
    [int]$UsbSettleMilliseconds = 1000,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 1,
    [int]$Repeat = 1,
    [string]$Log = "",
    [string]$ValidateLog = "",
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

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

function Get-TokenCount {
    param(
        [string]$Text,
        [string]$Needle
    )

    if ([string]::IsNullOrEmpty($Text) -or [string]::IsNullOrEmpty($Needle)) {
        return 0
    }

    $Count = 0
    $Index = 0
    while ($true) {
        $Found = $Text.IndexOf($Needle, $Index, [System.StringComparison]::Ordinal)
        if ($Found -lt 0) {
            return $Count
        }
        ++$Count
        $Index = $Found + $Needle.Length
    }
}

function Get-PacketStreamBeginInfo {
    param([byte[]]$Bytes)

    $HeaderSize = 28
    if ($Bytes.Length -lt $HeaderSize) {
        throw "PacketStream is too small to contain a packet header."
    }

    $Magic = [BitConverter]::ToUInt32($Bytes, 0)
    $Version = [BitConverter]::ToUInt16($Bytes, 4)
    $HeaderFieldSize = [BitConverter]::ToUInt16($Bytes, 6)
    $Kind = [BitConverter]::ToUInt16($Bytes, 8)
    $Flags = [BitConverter]::ToUInt16($Bytes, 10)
    $Sequence = [BitConverter]::ToUInt32($Bytes, 12)
    $Size = [BitConverter]::ToUInt32($Bytes, 20)
    $Crc32 = [BitConverter]::ToUInt32($Bytes, 24)

    if ($Magic -ne 0x504C5643) {
        throw ("PacketStream begin packet has bad magic: 0x{0:x8}" -f $Magic)
    }
    if ($Version -ne 1) {
        throw "PacketStream begin packet has unsupported version: $Version"
    }
    if ($HeaderFieldSize -ne $HeaderSize) {
        throw "PacketStream begin packet has unsupported header size: $HeaderFieldSize"
    }
    if ($Kind -ne 1) {
        throw "PacketStream first packet is not begin: kind=$Kind"
    }
    if ($Sequence -ne 0) {
        throw "PacketStream begin packet sequence must be 0, got $Sequence"
    }
    if ($Size -eq 0) {
        throw "PacketStream begin packet payload size must be non-zero."
    }

    return @{
        PayloadSize = $Size
        Crc32 = $Crc32
        CheckCrc = (($Flags -band 1) -ne 0)
    }
}

function Get-RequiredTokens {
    param(
        [int]$PayloadSize,
        [uint32]$Crc32
    )

    return @(
        "USB CDC packetstream transfer passed.",
        "dev: usb active=0 exit=launch_ready",
        "dev: stage=launch_ready code=ok received=$PayloadSize",
        ("crc=0x{0:x8}/0x{0:x8}" -f $Crc32),
        "dropped=0 overflow=0",
        "dev: store install qspi receive=ok",
        "store=ok code=ok",
        "dev: store entries=2",
        "name=hello_app",
        "name=player_min",
        "hello_app: charm_app_main entered",
        "hello_app: argv1=alpha",
        "dev: app command=run name=qspi:hello_app run=enabled",
        "player_min: presented one frame",
        "dev: app command=run name=qspi:player_min run=enabled",
        "dev: app stage-arena name=sdram2_stage_cache addr=0xd0040000 expected=0xd0040000",
        "dev: app sdram2 ready=1 init=1 smoke=1 base=0xd0000000",
        "present_count=1",
        "input_polls=1"
    )
}

function Get-RequiredCounts {
    param([int]$Repeat = 1)

    return @(
        @{ Token = "USB CDC packetstream transfer passed."; Count = $Repeat },
        @{ Token = "dev: store install qspi receive=ok"; Count = $Repeat },
        @{ Token = "dev: store entries=2"; Count = $Repeat },
        @{ Token = "dev: app command=run name=qspi:hello_app run=enabled"; Count = $Repeat },
        @{ Token = "dev: app command=run name=qspi:player_min run=enabled"; Count = $Repeat },
        @{ Token = "dev: app run stage=exit code=ok"; Count = 2 * $Repeat },
        @{ Token = "exited=1 exit=0"; Count = 2 * $Repeat },
        @{ Token = "present_count=1"; Count = $Repeat },
        @{ Token = "input_polls=1"; Count = $Repeat }
    )
}

function Get-MissingEvidence {
    param(
        [string]$Text,
        [string[]]$Tokens,
        [object[]]$Counts
    )

    $Missing = New-Object System.Collections.Generic.List[string]
    foreach ($Token in $Tokens) {
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }
    foreach ($Requirement in $Counts) {
        $Actual = Get-TokenCount -Text $Text -Needle $Requirement.Token
        if ($Actual -lt $Requirement.Count) {
            [void]$Missing.Add("$($Requirement.Token) x$($Requirement.Count) (actual $Actual)")
        }
    }
    return ,$Missing.ToArray()
}

function Validate-LogFile {
    param(
        [string]$Path,
        [string[]]$Tokens,
        [object[]]$Counts,
        [int]$Repeat
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "ValidateLog path must not be empty."
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Log file not found: $Path"
    }

    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Text = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    $Missing = New-Object System.Collections.Generic.List[string]
    foreach ($Token in (Get-MissingEvidence -Text $Text -Tokens $Tokens -Counts $Counts)) {
        [void]$Missing.Add($Token)
    }
    for ($Index = 1; $Index -le $Repeat; ++$Index) {
        $Token = "platform repeat $Index/$Repeat passed"
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }

    Write-Host "Dev Loader USB CDC App Store platform smoke log validation"
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
USB CDC packetstream transfer passed.
dev: usb active=0 exit=launch_ready bytes=11648
dev: usb rx packets=182 bytes=11648 read=11648 dropped=0 overflow=0 ctrl=28 last_ctrl=33/7
dev: stage=launch_ready code=ok received=10416 crc=0x73de4894/0x73de4894
dev: store install qspi receive=ok recv_bytes=10416 store=ok code=ok target=0x00000000 written=10416 erased=12288
dev: store entries=2
  [0] name=hello_app offset=0x00000070 size=5132 flags=0x00000000 runnable=1
  [1] name=player_min offset=0x00001480 size=5168 flags=0x00000000 runnable=1
hello_app: charm_app_main entered
hello_app: argv1=alpha
dev: app command=run name=qspi:hello_app run=enabled
dev: app stage-arena name=sdram2_stage_cache addr=0xd0040000 expected=0xd0040000 size=131072 align=32
dev: app sdram2 ready=1 init=1 smoke=1 base=0xd0000000 size=33554432
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070021 span=270 segments=2 exited=1 exit=0 app_exit=0 app_exit_code=0
player_min: presented one frame
dev: app command=run name=qspi:player_min run=enabled
dev: app stage-arena name=sdram2_stage_cache addr=0xd0040000 expected=0xd0040000 size=131072 align=32
dev: app sdram2 ready=1 init=1 smoke=1 base=0xd0000000 size=33554432
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070001 span=1280 segments=3 exited=1 exit=0 app_exit=0 app_exit_code=0
dev: app caps console_bytes=32 present_count=1 present_bytes=1024 sample0=0xff51a851 input_polls=1
platform repeat 1/1 passed
"@
}

function Invoke-SelfTest {
    $Tokens = Get-RequiredTokens -PayloadSize 10416 -Crc32 0x73de4894
    $Counts = Get-RequiredCounts
    $Missing = Get-MissingEvidence -Text (Get-SyntheticPassingLog) -Tokens $Tokens -Counts $Counts
    if ($Missing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing log missed tokens."
        foreach ($Token in $Missing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $FailingLog = (Get-SyntheticPassingLog).Replace("player_min: presented one frame", "player_min: failed")
    $FailingMissing = Get-MissingEvidence -Text $FailingLog -Tokens $Tokens -Counts $Counts
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "player_min: presented one frame") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Dev Loader USB CDC App Store platform smoke self-test passed."
    return 0
}

function Open-ControlSerial {
    $Serial = [System.IO.Ports.SerialPort]::new(
        $ControlPort,
        $ControlBaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.NewLine = "`r"
    $Serial.ReadTimeout = 200
    $Serial.WriteTimeout = 2000
    $Serial.Open()
    $Serial.DiscardInBuffer()
    $Serial.DiscardOutBuffer()
    return $Serial
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
    [void]$script:Capture.Append($Text)
    Write-Host -NoNewline $Text
}

function Read-UntilPrompt {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [string]$Phase,
        [int]$Timeout
    )

    $Local = New-Object System.Text.StringBuilder
    $Deadline = [DateTime]::UtcNow.AddSeconds($Timeout)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            Write-CaptureText $Chunk
            [void]$Local.Append($Chunk)
            if ($Local.ToString().IndexOf("dev-loader>", [System.StringComparison]::Ordinal) -ge 0) {
                return $Local.ToString()
            }
        }
        Start-Sleep -Milliseconds 50
    }
    throw "${Phase}_failed: timeout waiting for monitor prompt"
}

function Send-ControlCommand {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [string]$Command,
        [string]$Phase,
        [int]$Timeout
    )

    Write-CaptureText "`n[capture] sent: $Command`n"
    $Serial.Write("$Command`r")
    return (Read-UntilPrompt -Serial $Serial -Phase $Phase -Timeout $Timeout)
}

function Invoke-StoreAndRun {
    param([int]$Index)

    $Serial = $null
    try {
        $Serial = Open-ControlSerial
        Start-Sleep -Milliseconds 300
        [void]$Serial.ReadExisting()
        [void](Send-ControlCommand -Serial $Serial -Command "dev store install qspi" -Phase "store_install" -Timeout 45)
        [void](Send-ControlCommand -Serial $Serial -Command "dev store list qspi" -Phase "store_list" -Timeout 10)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run qspi:hello_app alpha beta" -Phase "hello_run" -Timeout 15)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run qspi:player_min" -Phase "player_min_run" -Timeout 15)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app status" -Phase "app_status" -Timeout 10)
    } finally {
        if (($null -ne $Serial) -and $Serial.IsOpen) {
            $Serial.Close()
        }
        if ($null -ne $Serial) {
            $Serial.Dispose()
        }
    }
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ($UsbEnumerateTimeoutSeconds -le 0) {
    throw "UsbEnumerateTimeoutSeconds must be greater than zero."
}
if ($UsbSettleMilliseconds -lt 0) {
    throw "UsbSettleMilliseconds must not be negative."
}
if ($ControlBaudRate -le 0 -or $UsbBaudRate -le 0) {
    throw "Baud rates must be greater than zero."
}
if ($WriteChunkSize -le 0) {
    throw "WriteChunkSize must be greater than zero."
}
if ($InterChunkDelayMs -lt 0) {
    throw "InterChunkDelayMs must not be negative."
}
if ($Repeat -le 0) {
    throw "Repeat must be greater than zero."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    $PacketStream = Join-Path $ProjectRoot "..\..\app_abi\elf_samples\out\appstore.bin.packetstream"
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_appstore_platform_smoke.log"
}

$TransferSmoke = Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-transfer-smoke.ps1"
if (-not (Test-Path -LiteralPath $TransferSmoke)) {
    throw "USB CDC App Store transfer smoke not found: $TransferSmoke"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$PacketBytes = [System.IO.File]::ReadAllBytes($ResolvedPacketStream)
$PacketInfo = Get-PacketStreamBeginInfo -Bytes $PacketBytes
$RequiredTokens = Get-RequiredTokens -PayloadSize $PacketInfo.PayloadSize -Crc32 $PacketInfo.Crc32
$RequiredCounts = Get-RequiredCounts -Repeat $Repeat
$PerRepeatCounts = Get-RequiredCounts -Repeat 1

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens -Counts $RequiredCounts -Repeat $Repeat)
}

$ResolvedLog = [System.IO.Path]::GetFullPath($Log)
if ($DryRun) {
    Write-Host "H747 Dev Loader USB CDC App Store platform smoke dry run"
    Write-Host "  control port: $ControlPort"
    Write-Host "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto-discover after dev usb begin' } else { $UsbPort })"
    Write-Host "  packetstream: $ResolvedPacketStream"
    Write-Host "  bytes:        $($PacketBytes.Length)"
    Write-Host "  payload:      $($PacketInfo.PayloadSize)"
    Write-Host ("  expected crc: 0x{0:x8}" -f $PacketInfo.Crc32)
    Write-Host "  repeat:       $Repeat"
    Write-Host "  write chunk:  $WriteChunkSize"
    Write-Host "  chunk delay:  ${InterChunkDelayMs}ms"
    Write-Host "  commands:     dev store install qspi; dev store list qspi; dev app run qspi:hello_app alpha beta; dev app run qspi:player_min; dev app status"
    Write-Host "  log:          $ResolvedLog"
    exit 0
}

$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:LogWriter = $null
$script:Capture = New-Object System.Text.StringBuilder
$Failed = $false

try {
    $script:LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    Write-CaptureText "H747 Dev Loader USB CDC App Store platform smoke`n"
    Write-CaptureText "  control port: $ControlPort`n"
    Write-CaptureText "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })`n"
    Write-CaptureText "  packetstream: $ResolvedPacketStream`n"
    Write-CaptureText "  bytes:        $($PacketBytes.Length)`n"
    Write-CaptureText "  payload:      $($PacketInfo.PayloadSize)`n"
    Write-CaptureText ("  expected crc: 0x{0:x8}`n" -f $PacketInfo.Crc32)
    Write-CaptureText "  repeat:       $Repeat`n"
    Write-CaptureText "  write chunk:  $WriteChunkSize`n"
    Write-CaptureText "  chunk delay:  ${InterChunkDelayMs}ms`n"
    Write-CaptureText "  log:          $ResolvedLog`n`n"

    for ($Index = 1; $Index -le $Repeat; ++$Index) {
        Write-CaptureText "`n=== platform repeat $Index/$Repeat ===`n"
        $RepeatStart = $script:Capture.Length
        $TransferLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".repeat$Index.transfer.log")
        $TransferArgs = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $TransferSmoke,
            "-PacketStream", $ResolvedPacketStream,
            "-ControlPort", $ControlPort,
            "-ControlBaudRate", $ControlBaudRate,
            "-UsbBaudRate", $UsbBaudRate,
            "-TimeoutSeconds", $TimeoutSeconds,
            "-UsbEnumerateTimeoutSeconds", $UsbEnumerateTimeoutSeconds,
            "-UsbSettleMilliseconds", $UsbSettleMilliseconds,
            "-WriteChunkSize", $WriteChunkSize,
            "-InterChunkDelayMs", $InterChunkDelayMs,
            "-Repeat", 1,
            "-Log", $TransferLog
        )
        if (-not [string]::IsNullOrWhiteSpace($UsbPort)) {
            $TransferArgs += @("-UsbPort", $UsbPort)
        }

        & powershell @TransferArgs
        if ($LASTEXITCODE -ne 0) {
            throw "transfer_failed: USB CDC App Store transfer failed with exit code $LASTEXITCODE"
        }

        $TransferText = Get-Content -LiteralPath $TransferLog -Raw -Encoding UTF8
        Write-CaptureText "`n--- transfer log repeat $Index ---`n"
        Write-CaptureText $TransferText
        Invoke-StoreAndRun -Index $Index

        $CurrentText = $script:Capture.ToString()
        $RepeatText = $CurrentText.Substring($RepeatStart)
        $Missing = Get-MissingEvidence -Text $RepeatText -Tokens $RequiredTokens -Counts $PerRepeatCounts
        if ($Missing.Count -ne 0) {
            Write-CaptureText "`nmissing_token: $($Missing -join '; ')`n"
            throw "missing_token: platform repeat $Index missed required evidence"
        }
        Write-CaptureText "`nplatform repeat $Index/$Repeat passed`n"
    }

    $Text = $script:Capture.ToString()
    $Throughputs = [regex]::Matches($Text, "throughput: ([0-9.]+ KiB/s) to launch_ready") |
        ForEach-Object { $_.Groups[1].Value }
    $Install = [regex]::Match($Text, "written=(\d+) erased=(\d+)")
    Write-CaptureText "`nSummary:`n"
    if ($Throughputs.Count -gt 0) {
        Write-CaptureText "  usb throughput: $($Throughputs -join ', ')`n"
    }
    if ($Install.Success) {
        Write-CaptureText "  qspi written=$($Install.Groups[1].Value) erased=$($Install.Groups[2].Value)`n"
    }
    Write-CaptureText "  hello_app exit=0`n"
    Write-CaptureText "  player_min exit=0`n"
    Write-CaptureText "`nUSB CDC App Store platform smoke passed.`n"
    exit 0
} catch {
    $Failed = $true
    Write-CaptureText "`nUSB CDC App Store platform smoke failed: $($_.Exception.Message)`n"
    throw
} finally {
    if ($null -ne $script:LogWriter) {
        $script:LogWriter.Dispose()
    }
    if ($Failed) {
        Write-Host "Log: $ResolvedLog"
    }
}
