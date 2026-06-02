param(
    [string]$PacketStream = "",
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [int]$TimeoutSeconds = 30,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 0,
    [string]$Log = "",
    [switch]$DryRun,
    [switch]$NoBegin,
    [switch]$WaitPrompt,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ($BaudRate -le 0) {
    throw "BaudRate must be greater than zero."
}
if ($WriteChunkSize -le 0) {
    throw "WriteChunkSize must be greater than zero."
}
if ($InterChunkDelayMs -lt 0) {
    throw "InterChunkDelayMs must not be negative."
}

function Get-TransferEstimate {
    param(
        [int]$ByteCount,
        [int]$BaudRate,
        [int]$WriteChunkSize,
        [int]$InterChunkDelayMs
    )

    $ChunkCount = [int][Math]::Ceiling($ByteCount / [double]$WriteChunkSize)
    $WireSeconds = ($ByteCount * 10.0) / [double]$BaudRate
    $DelaySeconds = [Math]::Max($ChunkCount - 1, 0) * ($InterChunkDelayMs / 1000.0)
    $TotalSeconds = $WireSeconds + $DelaySeconds
    $KiBPerSecond = ($ByteCount / 1024.0) / [Math]::Max($TotalSeconds, 0.001)
    return @{
        ChunkCount = $ChunkCount
        WireSeconds = $WireSeconds
        DelaySeconds = $DelaySeconds
        TotalSeconds = $TotalSeconds
        KiBPerSecond = $KiBPerSecond
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

if ($SelfTest) {
    $Estimate = Get-TransferEstimate -ByteCount 1024 -BaudRate 115200 -WriteChunkSize 128 -InterChunkDelayMs 1
    if ($Estimate.ChunkCount -ne 8) {
        throw "Self-test failed: chunk count was $($Estimate.ChunkCount), expected 8."
    }
    if ($Estimate.WireSeconds -le 0 -or $Estimate.TotalSeconds -le $Estimate.WireSeconds) {
        throw "Self-test failed: transfer estimate was invalid."
    }
    [byte[]]$Synthetic = @(
        0x43, 0x56, 0x4c, 0x50,
        0x01, 0x00,
        0x1c, 0x00,
        0x01, 0x00,
        0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00,
        0x78, 0x56, 0x34, 0x12
    )
    $Info = Get-PacketStreamBeginInfo -Bytes $Synthetic
    if ($Info.PayloadSize -ne 64 -or $Info.Crc32 -ne 0x12345678 -or -not $Info.CheckCrc) {
        throw "Self-test failed: begin header parse was invalid."
    }
    Write-Host "Raw packetstream sender self-test passed."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    throw "PacketStream is required."
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_raw_smoke.log"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$PacketBytes = [System.IO.File]::ReadAllBytes($ResolvedPacketStream)
if ($PacketBytes.Length -eq 0) {
    throw "PacketStream is empty: $ResolvedPacketStream"
}
$PacketInfo = Get-PacketStreamBeginInfo -Bytes $PacketBytes
$TransferEstimate = Get-TransferEstimate `
    -ByteCount $PacketBytes.Length `
    -BaudRate $BaudRate `
    -WriteChunkSize $WriteChunkSize `
    -InterChunkDelayMs $InterChunkDelayMs

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

Write-Host "H747 Dev Loader raw packetstream sender"
Write-Host "  port:         $Port"
Write-Host "  baud:         $BaudRate"
Write-Host "  packetstream: $ResolvedPacketStream"
Write-Host "  bytes:        $($PacketBytes.Length)"
Write-Host "  payload:      $($PacketInfo.PayloadSize)"
Write-Host ("  expected crc: 0x{0:x8} ({1})" -f $PacketInfo.Crc32, $(if ($PacketInfo.CheckCrc) { "checked" } else { "not checked" }))
Write-Host "  write chunk:  $WriteChunkSize"
Write-Host "  chunk delay:  ${InterChunkDelayMs}ms"
Write-Host "  chunks:       $($TransferEstimate.ChunkCount)"
Write-Host ("  estimate:     {0:n3}s min, {1:n2} KiB/s effective" -f `
    $TransferEstimate.TotalSeconds, `
    $TransferEstimate.KiBPerSecond)
Write-Host "  timeout:      ${TimeoutSeconds}s"
Write-Host "  log:          $([System.IO.Path]::GetFullPath($Log))"
if ($DryRun) {
    Write-Host ""
    Write-Host "Dry run: serial port was not opened."
    exit 0
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$LogWriter = $null
$Serial = $null
$RawCapture = New-Object System.Text.StringBuilder

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

function Wait-ForToken {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [string]$Token,
        [DateTime]$Deadline
    )

    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$script:RawCapture.Append($Chunk)
            Write-CaptureText $Chunk
            if ($script:RawCapture.ToString().IndexOf($Token, [System.StringComparison]::Ordinal) -ge 0) {
                return $true
            }
        }
        Start-Sleep -Milliseconds 20
    }
    return $false
}

function Drain-SerialText {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$QuietMilliseconds = 120,
        [int]$MaxMilliseconds = 1000
    )

    $Deadline = [DateTime]::UtcNow.AddMilliseconds($MaxMilliseconds)
    $QuietDeadline = [DateTime]::UtcNow.AddMilliseconds($QuietMilliseconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$script:RawCapture.Append($Chunk)
            Write-CaptureText $Chunk
            $QuietDeadline = [DateTime]::UtcNow.AddMilliseconds($QuietMilliseconds)
        } elseif ([DateTime]::UtcNow -ge $QuietDeadline) {
            return
        }
        Start-Sleep -Milliseconds 20
    }
}

try {
    $LogWriter = [System.IO.StreamWriter]::new([System.IO.Path]::GetFullPath($Log), $false, $Utf8NoBom)
    Write-CaptureText "H747 Dev Loader raw packetstream sender`n"
    Write-CaptureText "  port:         $Port`n"
    Write-CaptureText "  baud:         $BaudRate`n"
    Write-CaptureText "  packetstream: $ResolvedPacketStream`n"
    Write-CaptureText "  bytes:        $($PacketBytes.Length)`n`n"
    Write-CaptureText "  payload:      $($PacketInfo.PayloadSize)`n"
    Write-CaptureText ("  expected crc: 0x{0:x8} ({1})`n" -f $PacketInfo.Crc32, $(if ($PacketInfo.CheckCrc) { "checked" } else { "not checked" }))
    Write-CaptureText "  write chunk:  $WriteChunkSize`n"
    Write-CaptureText "  chunk delay:  ${InterChunkDelayMs}ms`n"
    Write-CaptureText "  chunks:       $($TransferEstimate.ChunkCount)`n"
    Write-CaptureText ("  estimate:     {0:n3}s min, {1:n2} KiB/s effective`n`n" -f `
        $TransferEstimate.TotalSeconds, `
        $TransferEstimate.KiBPerSecond)

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.NewLine = "`n"
    $Serial.ReadTimeout = 200
    $Serial.WriteTimeout = 10000
    $Serial.Open()
    $Serial.DiscardInBuffer()
    $Serial.DiscardOutBuffer()

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    if (-not $NoBegin) {
        $Serial.WriteLine("dev raw begin")
        Write-CaptureText "[sender] sent: dev raw begin`n"
        if (-not (Wait-ForToken -Serial $Serial -Token "dev: raw ready" -Deadline $Deadline)) {
            throw "Timed out waiting for raw ready."
        }
    }

    $Start = [DateTime]::UtcNow
    $Offset = 0
    while ($Offset -lt $PacketBytes.Length) {
        $Count = [Math]::Min($WriteChunkSize, $PacketBytes.Length - $Offset)
        $Serial.BaseStream.Write($PacketBytes, $Offset, $Count)
        $Offset += $Count
        if (($InterChunkDelayMs -gt 0) -and ($Offset -lt $PacketBytes.Length)) {
            Start-Sleep -Milliseconds $InterChunkDelayMs
        }
    }
    $Serial.BaseStream.Flush()
    $WriteElapsed = [DateTime]::UtcNow - $Start
    Write-CaptureText "`n[sender] wrote raw bytes=$($PacketBytes.Length) elapsed=$('{0:n3}' -f $WriteElapsed.TotalSeconds)s`n"

    if (-not (Wait-ForToken -Serial $Serial -Token "dev: stage=launch_ready code=ok" -Deadline $Deadline)) {
        $Serial.WriteLine("dev raw status")
        [void](Wait-ForToken -Serial $Serial -Token "dev: raw" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))
        throw "Timed out waiting for launch_ready."
    }
    $LaunchReadyElapsed = [DateTime]::UtcNow - $Start
    [void](Wait-ForToken -Serial $Serial -Token "dev-loader>" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))
    $Serial.WriteLine("dev raw status")
    Write-CaptureText "`n[sender] sent: dev raw status`n"
    [void](Wait-ForToken -Serial $Serial -Token "dev-loader>" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))
    $Captured = $RawCapture.ToString()
    $ExpectedReceived = "received=$($PacketInfo.PayloadSize)"
    if ($Captured.IndexOf($ExpectedReceived, [System.StringComparison]::Ordinal) -lt 0) {
        throw "launch_ready was seen, but expected token was missing: $ExpectedReceived"
    }
    if ($PacketInfo.CheckCrc) {
        $ExpectedCrc = ("0x{0:x8}" -f $PacketInfo.Crc32)
        if ($Captured.IndexOf($ExpectedCrc, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "launch_ready was seen, but expected CRC token was missing: $ExpectedCrc"
        }
    }
    if ($WaitPrompt) {
        [void](Wait-ForToken -Serial $Serial -Token "dev-loader>" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))
    }
    Drain-SerialText -Serial $Serial

    $TotalElapsed = [DateTime]::UtcNow - $Start
    $KibPerSecond = ($PacketBytes.Length / 1024.0) / [Math]::Max($LaunchReadyElapsed.TotalSeconds, 0.001)
    Write-CaptureText "`nRaw packetstream transfer passed.`n"
    Write-CaptureText ("throughput: {0:n2} KiB/s to launch_ready`n" -f $KibPerSecond)
    Write-CaptureText ("settled elapsed: {0:n3}s`n" -f $TotalElapsed.TotalSeconds)
    exit 0
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
