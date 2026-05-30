param(
    [string]$PacketStream = "",
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [int]$ControlBaudRate = 115200,
    [int]$UsbBaudRate = 115200,
    [int]$TimeoutSeconds = 30,
    [int]$WriteChunkSize = 4096,
    [int]$InterChunkDelayMs = 0,
    [string]$Log = "",
    [switch]$DryRun,
    [switch]$NoBegin,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
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
        [int]$WriteChunkSize,
        [int]$InterChunkDelayMs
    )

    $ChunkCount = [int][Math]::Ceiling($ByteCount / [double]$WriteChunkSize)
    $DelaySeconds = [Math]::Max($ChunkCount - 1, 0) * ($InterChunkDelayMs / 1000.0)
    return @{
        ChunkCount = $ChunkCount
        DelaySeconds = $DelaySeconds
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
    $Estimate = Get-TransferEstimate -ByteCount 8192 -WriteChunkSize 4096 -InterChunkDelayMs 1
    if ($Estimate.ChunkCount -ne 2 -or $Estimate.DelaySeconds -le 0) {
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
    Write-Host "USB CDC packetstream sender self-test passed."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    throw "PacketStream is required."
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}
if ([string]::IsNullOrWhiteSpace($UsbPort)) {
    throw "UsbPort is required. Use Device Manager or Get-PnpDevice to find the CDC COM port after 'dev usb begin'."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_smoke.log"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$PacketBytes = [System.IO.File]::ReadAllBytes($ResolvedPacketStream)
if ($PacketBytes.Length -eq 0) {
    throw "PacketStream is empty: $ResolvedPacketStream"
}
$PacketInfo = Get-PacketStreamBeginInfo -Bytes $PacketBytes
$TransferEstimate = Get-TransferEstimate `
    -ByteCount $PacketBytes.Length `
    -WriteChunkSize $WriteChunkSize `
    -InterChunkDelayMs $InterChunkDelayMs

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

Write-Host "H747 Dev Loader USB CDC packetstream sender"
Write-Host "  control port: $ControlPort"
Write-Host "  usb port:     $UsbPort"
Write-Host "  packetstream: $ResolvedPacketStream"
Write-Host "  bytes:        $($PacketBytes.Length)"
Write-Host "  payload:      $($PacketInfo.PayloadSize)"
Write-Host ("  expected crc: 0x{0:x8} ({1})" -f $PacketInfo.Crc32, $(if ($PacketInfo.CheckCrc) { "checked" } else { "not checked" }))
Write-Host "  write chunk:  $WriteChunkSize"
Write-Host "  chunk delay:  ${InterChunkDelayMs}ms"
Write-Host "  chunks:       $($TransferEstimate.ChunkCount)"
Write-Host "  timeout:      ${TimeoutSeconds}s"
Write-Host "  log:          $([System.IO.Path]::GetFullPath($Log))"
if ($DryRun) {
    Write-Host ""
    Write-Host "Dry run: serial ports were not opened."
    exit 0
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$LogWriter = $null
$ControlSerial = $null
$UsbSerial = $null
$Capture = New-Object System.Text.StringBuilder

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
            [void]$script:Capture.Append($Chunk)
            Write-CaptureText $Chunk
            if ($script:Capture.ToString().IndexOf($Token, [System.StringComparison]::Ordinal) -ge 0) {
                return $true
            }
        }
        Start-Sleep -Milliseconds 20
    }
    return $false
}

try {
    $LogWriter = [System.IO.StreamWriter]::new([System.IO.Path]::GetFullPath($Log), $false, $Utf8NoBom)
    Write-CaptureText "H747 Dev Loader USB CDC packetstream sender`n"
    Write-CaptureText "  control port: $ControlPort`n"
    Write-CaptureText "  usb port:     $UsbPort`n"
    Write-CaptureText "  packetstream: $ResolvedPacketStream`n"
    Write-CaptureText "  bytes:        $($PacketBytes.Length)`n`n"

    $ControlSerial = [System.IO.Ports.SerialPort]::new(
        $ControlPort,
        $ControlBaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $ControlSerial.Encoding = [System.Text.Encoding]::ASCII
    $ControlSerial.NewLine = "`n"
    $ControlSerial.ReadTimeout = 200
    $ControlSerial.WriteTimeout = 1000
    $ControlSerial.Open()
    $ControlSerial.DiscardInBuffer()
    $ControlSerial.DiscardOutBuffer()

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    if (-not $NoBegin) {
        $ControlSerial.WriteLine("dev usb begin")
        Write-CaptureText "[sender] sent: dev usb begin`n"
        if (-not (Wait-ForToken -Serial $ControlSerial -Token "dev: usb ready" -Deadline $Deadline)) {
            throw "Timed out waiting for usb ready."
        }
    }

    $UsbSerial = [System.IO.Ports.SerialPort]::new(
        $UsbPort,
        $UsbBaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $UsbSerial.Encoding = [System.Text.Encoding]::ASCII
    $UsbSerial.ReadTimeout = 200
    $UsbSerial.WriteTimeout = 10000
    $UsbSerial.Open()
    $UsbSerial.DiscardInBuffer()
    $UsbSerial.DiscardOutBuffer()

    $Start = [DateTime]::UtcNow
    $Offset = 0
    while ($Offset -lt $PacketBytes.Length) {
        $Count = [Math]::Min($WriteChunkSize, $PacketBytes.Length - $Offset)
        $UsbSerial.BaseStream.Write($PacketBytes, $Offset, $Count)
        $Offset += $Count
        if (($InterChunkDelayMs -gt 0) -and ($Offset -lt $PacketBytes.Length)) {
            Start-Sleep -Milliseconds $InterChunkDelayMs
        }
    }
    $UsbSerial.BaseStream.Flush()
    Write-CaptureText "`n[sender] wrote usb bytes=$($PacketBytes.Length)`n"

    if (-not (Wait-ForToken -Serial $ControlSerial -Token "dev: stage=launch_ready code=ok" -Deadline $Deadline)) {
        $ControlSerial.WriteLine("dev usb status")
        [void](Wait-ForToken -Serial $ControlSerial -Token "dev: usb" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))
        throw "Timed out waiting for launch_ready."
    }
    $LaunchReadyElapsed = [DateTime]::UtcNow - $Start
    $Captured = $Capture.ToString()
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

    $KibPerSecond = ($PacketBytes.Length / 1024.0) / [Math]::Max($LaunchReadyElapsed.TotalSeconds, 0.001)
    Write-CaptureText "`nUSB CDC packetstream transfer passed.`n"
    Write-CaptureText ("throughput: {0:n2} KiB/s to launch_ready`n" -f $KibPerSecond)
    exit 0
} finally {
    if (($null -ne $UsbSerial) -and $UsbSerial.IsOpen) {
        $UsbSerial.Close()
    }
    if ($null -ne $UsbSerial) {
        $UsbSerial.Dispose()
    }
    if (($null -ne $ControlSerial) -and $ControlSerial.IsOpen) {
        $ControlSerial.Close()
    }
    if ($null -ne $ControlSerial) {
        $ControlSerial.Dispose()
    }
    if ($null -ne $LogWriter) {
        $LogWriter.Dispose()
    }
}
