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
    [int]$Repeat = 3,
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

function Get-DefaultTokens {
    param(
        [int]$PayloadSize,
        [uint32]$Crc32
    )

    return @(
        "USB CDC packetstream transfer passed.",
        "dev: usb active=0 exit=launch_ready",
        "dev: stage=launch_ready code=ok received=$PayloadSize",
        ("crc=0x{0:x8}/0x{0:x8}" -f $Crc32),
        "dropped=0 overflow=0"
    )
}

function Get-MissingTokens {
    param(
        [string]$Text,
        [string[]]$Tokens
    )

    $Missing = New-Object System.Collections.Generic.List[string]
    foreach ($Token in $Tokens) {
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }
    return ,$Missing.ToArray()
}

function Validate-LogFile {
    param(
        [string]$Path,
        [string[]]$Tokens,
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
    $Missing = Get-MissingTokens -Text $Text -Tokens $Tokens
    for ($Index = 1; $Index -le $Repeat; ++$Index) {
        $Token = "repeat $Index/$Repeat passed"
        if (-not (Test-ContainsLiteral -Text $Text -Needle $Token)) {
            [void]$Missing.Add($Token)
        }
    }

    Write-Host "Dev Loader USB CDC App Store transfer smoke log validation"
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

function Get-SerialPortNames {
    $Ports = @([System.IO.Ports.SerialPort]::GetPortNames())
    [Array]::Sort($Ports, [System.StringComparer]::OrdinalIgnoreCase)
    return ,$Ports
}

function Select-UsbPortFromSnapshots {
    param(
        [string[]]$Before,
        [string[]]$After,
        [string]$ControlPort
    )

    $ControlUpper = $ControlPort.ToUpperInvariant()
    $BeforeUpper = @($Before | ForEach-Object { $_.ToUpperInvariant() })
    $NewPorts = @($After | Where-Object {
        $Upper = $_.ToUpperInvariant()
        ($Upper -ne $ControlUpper) -and ($BeforeUpper -notcontains $Upper)
    })
    if ($NewPorts.Count -eq 1) {
        return $NewPorts[0]
    }
    if ($NewPorts.Count -gt 1) {
        throw "Multiple new serial ports appeared: $($NewPorts -join ', '). Pass -UsbPort explicitly."
    }

    $NonControl = @($After | Where-Object { $_.ToUpperInvariant() -ne $ControlUpper })
    if ($NonControl.Count -eq 1) {
        return $NonControl[0]
    }

    throw "Could not identify USB CDC port. Current ports: $($After -join ', '). Pass -UsbPort explicitly."
}

function Wait-ForUsbPort {
    param(
        [string[]]$Before,
        [string]$ControlPort,
        [string]$RequestedUsbPort,
        [int]$TimeoutSeconds
    )

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $LastError = ""
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Current = Get-SerialPortNames
        if (-not [string]::IsNullOrWhiteSpace($RequestedUsbPort)) {
            if (@($Current | ForEach-Object { $_.ToUpperInvariant() }) -contains $RequestedUsbPort.ToUpperInvariant()) {
                return $RequestedUsbPort
            }
            $LastError = "USB port $RequestedUsbPort is not present yet. Current ports: $($Current -join ', ')"
        } else {
            try {
                return (Select-UsbPortFromSnapshots -Before $Before -After $Current -ControlPort $ControlPort)
            } catch {
                $LastError = $_.Exception.Message
            }
        }
        Start-Sleep -Milliseconds 200
    }

    if ([string]::IsNullOrWhiteSpace($LastError)) {
        $LastError = "Timed out waiting for USB CDC port."
    }
    throw $LastError
}

function Open-ControlSerial {
    $Serial = [System.IO.Ports.SerialPort]::new(
        $ControlPort,
        $ControlBaudRate,
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
    Write-Host -NoNewline $Text
}

function Wait-ForToken {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [string]$Token,
        [DateTime]$Deadline
    )

    $Seen = New-Object System.Text.StringBuilder
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            Write-CaptureText $Chunk
            [void]$Seen.Append($Chunk)
            if ($Seen.ToString().IndexOf($Token, [System.StringComparison]::Ordinal) -ge 0) {
                return $true
            }
        }
        Start-Sleep -Milliseconds 20
    }
    return $false
}

function Read-UntilQuiet {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$QuietMilliseconds = 150,
        [int]$MaxMilliseconds = 1200
    )

    $LastRead = [DateTime]::UtcNow
    $Deadline = [DateTime]::UtcNow.AddMilliseconds($MaxMilliseconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            Write-CaptureText $Chunk
            $LastRead = [DateTime]::UtcNow
        }
        if ((([DateTime]::UtcNow - $LastRead).TotalMilliseconds -ge $QuietMilliseconds)) {
            return
        }
        Start-Sleep -Milliseconds 20
    }
}

function Invoke-SelfTest {
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
    $Tokens = Get-DefaultTokens -PayloadSize $Info.PayloadSize -Crc32 $Info.Crc32
    $Passing = @"
USB CDC packetstream transfer passed.
dev: usb active=0 exit=launch_ready bytes=128
dev: usb rx packets=2 bytes=128 read=128 dropped=0 overflow=0 ctrl=1 last_ctrl=33/7
dev: stage=launch_ready code=ok received=64 crc=0x12345678/0x12345678
repeat 1/1 passed
"@
    $Missing = Get-MissingTokens -Text $Passing -Tokens $Tokens
    if ($Missing.Count -ne 0) {
        throw "Self-test failed: synthetic passing log missed tokens: $($Missing -join ', ')"
    }
    $Selected = Select-UsbPortFromSnapshots -Before @("COM16") -After @("COM16", "COM27") -ControlPort "COM16"
    if ($Selected -ne "COM27") {
        throw "Self-test failed: selected USB port was $Selected."
    }
    Write-Host "Dev Loader USB CDC App Store transfer smoke self-test passed."
    return 0
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
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_appstore_transfer_smoke.log"
}

$Sender = Join-Path $PSScriptRoot "send-dev-loader-usb-cdc-packetstream.ps1"
if (-not (Test-Path -LiteralPath $Sender)) {
    throw "USB CDC packetstream sender not found: $Sender"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$PacketBytes = [System.IO.File]::ReadAllBytes($ResolvedPacketStream)
$PacketInfo = Get-PacketStreamBeginInfo -Bytes $PacketBytes
$RequiredTokens = Get-DefaultTokens -PayloadSize $PacketInfo.PayloadSize -Crc32 $PacketInfo.Crc32

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens -Repeat $Repeat)
}

$ResolvedLog = [System.IO.Path]::GetFullPath($Log)
if ($DryRun) {
    Write-Host "H747 Dev Loader USB CDC App Store transfer smoke dry run"
    Write-Host "  control port: $ControlPort"
    Write-Host "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto-discover after dev usb begin' } else { $UsbPort })"
    Write-Host "  packetstream: $ResolvedPacketStream"
    Write-Host "  bytes:        $($PacketBytes.Length)"
    Write-Host "  payload:      $($PacketInfo.PayloadSize)"
    Write-Host ("  expected crc: 0x{0:x8}" -f $PacketInfo.Crc32)
    Write-Host "  repeat:       $Repeat"
    Write-Host "  write chunk:  $WriteChunkSize"
    Write-Host "  chunk delay:  ${InterChunkDelayMs}ms"
    Write-Host "  usb settle:   ${UsbSettleMilliseconds}ms"
    Write-Host "  log:          $ResolvedLog"
    exit 0
}

$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:LogWriter = $null
$ControlSerial = $null
$Failed = $false

try {
    $script:LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    Write-CaptureText "H747 Dev Loader USB CDC App Store transfer smoke`n"
    Write-CaptureText "  control port: $ControlPort`n"
    Write-CaptureText "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })`n"
    Write-CaptureText "  packetstream: $ResolvedPacketStream`n"
    Write-CaptureText "  bytes:        $($PacketBytes.Length)`n"
    Write-CaptureText "  payload:      $($PacketInfo.PayloadSize)`n"
    Write-CaptureText ("  expected crc: 0x{0:x8}`n" -f $PacketInfo.Crc32)
    Write-CaptureText "  repeat:       $Repeat`n"
    Write-CaptureText "  write chunk:  $WriteChunkSize`n"
    Write-CaptureText "  chunk delay:  ${InterChunkDelayMs}ms`n"
    Write-CaptureText "  usb settle:   ${UsbSettleMilliseconds}ms`n"
    Write-CaptureText "  log:          $ResolvedLog`n`n"

    for ($Index = 1; $Index -le $Repeat; ++$Index) {
        Write-CaptureText "`n=== repeat $Index/$Repeat ===`n"
        $PortsBefore = Get-SerialPortNames
        Write-CaptureText "serial ports before usb begin: $($PortsBefore -join ', ')`n"

        $ControlSerial = Open-ControlSerial
        $ControlSerial.WriteLine("dev packet reset-session")
        Write-CaptureText "[capture] sent: dev packet reset-session`n"
        Read-UntilQuiet -Serial $ControlSerial

        $ControlSerial.WriteLine("dev usb abort")
        Write-CaptureText "[capture] sent: dev usb abort`n"
        Read-UntilQuiet -Serial $ControlSerial
        if ($UsbSettleMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $UsbSettleMilliseconds
        }

        $ControlSerial.WriteLine("dev usb begin")
        Write-CaptureText "[capture] sent: dev usb begin`n"
        if (-not (Wait-ForToken -Serial $ControlSerial -Token "dev: usb ready" -Deadline ([DateTime]::UtcNow.AddSeconds($TimeoutSeconds)))) {
            throw "Timed out waiting for usb ready on repeat $Index."
        }
        Read-UntilQuiet -Serial $ControlSerial
        $ControlSerial.Close()
        $ControlSerial.Dispose()
        $ControlSerial = $null
        if ($UsbSettleMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $UsbSettleMilliseconds
        }

        try {
            $ResolvedUsbPort = Wait-ForUsbPort `
                -Before $PortsBefore `
                -ControlPort $ControlPort `
                -RequestedUsbPort $UsbPort `
                -TimeoutSeconds $UsbEnumerateTimeoutSeconds
        } catch {
            $ControlSerial = Open-ControlSerial
            $ControlSerial.WriteLine("dev usb status")
            Write-CaptureText "[capture] sent: dev usb status after enumerate failure`n"
            Read-UntilQuiet -Serial $ControlSerial -QuietMilliseconds 250 -MaxMilliseconds 3000
            $ControlSerial.Close()
            $ControlSerial.Dispose()
            $ControlSerial = $null
            throw
        }
        Write-CaptureText "selected usb port: $ResolvedUsbPort`n"

        $DownloadLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".repeat$Index.usb-download.log")
        & powershell -NoProfile -ExecutionPolicy Bypass -File $Sender `
            -PacketStream $ResolvedPacketStream `
            -ControlPort $ControlPort `
            -UsbPort $ResolvedUsbPort `
            -ControlBaudRate $ControlBaudRate `
            -UsbBaudRate $UsbBaudRate `
            -TimeoutSeconds $TimeoutSeconds `
            -WriteChunkSize $WriteChunkSize `
            -InterChunkDelayMs $InterChunkDelayMs `
            -Log $DownloadLog `
            -NoBegin
        if ($LASTEXITCODE -ne 0) {
            throw "USB CDC packetstream sender failed on repeat $Index with exit code $LASTEXITCODE"
        }

        $DownloadText = Get-Content -LiteralPath $DownloadLog -Raw -Encoding UTF8
        Write-CaptureText "`n--- repeat $Index sender log ---`n"
        Write-CaptureText $DownloadText

        $Missing = Get-MissingTokens -Text $DownloadText -Tokens $RequiredTokens
        if ($Missing.Count -ne 0) {
            throw "Repeat $Index missed required tokens: $($Missing -join ', ')"
        }
        Write-CaptureText "`nrepeat $Index/$Repeat passed`n"
    }

    Write-CaptureText "`nUSB CDC App Store transfer smoke passed.`n"
    exit 0
} catch {
    $Failed = $true
    Write-CaptureText "`nUSB CDC App Store transfer smoke failed: $($_.Exception.Message)`n"
    throw
} finally {
    if (($null -ne $ControlSerial) -and $ControlSerial.IsOpen) {
        $ControlSerial.Close()
    }
    if ($null -ne $ControlSerial) {
        $ControlSerial.Dispose()
    }
    if ($null -ne $script:LogWriter) {
        $script:LogWriter.Dispose()
    }
    if ($Failed) {
        Write-Host "Log: $ResolvedLog"
    }
}
