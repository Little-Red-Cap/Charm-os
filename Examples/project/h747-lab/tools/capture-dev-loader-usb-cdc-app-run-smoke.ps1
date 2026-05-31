param(
    [string]$PacketStream = "",
    [string]$AppName = "received_app",
    [string]$AppArgs = "",
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [int]$ControlBaudRate = 115200,
    [int]$UsbBaudRate = 115200,
    [int]$TimeoutSeconds = 45,
    [int]$UsbEnumerateTimeoutSeconds = 10,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 1,
    [ValidateSet("elf", "modulex")]
    [string]$AppFormat = "elf",
    [string]$Log = "",
    [string[]]$Expect = @(),
    [string]$ValidateLog = "",
    [switch]$NoResetSession,
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

function Get-DefaultTokens {
    param(
        [string]$Format,
        [string[]]$ExtraTokens
    )

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Token in @(
        "dev: usb ready",
        "USB CDC packetstream transfer passed.",
        "dev: stage=launch_ready code=ok",
        "dev: app command=run",
        "dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000",
        "dev: app read=ok",
        "dev: app stage=ok",
        "dev: app plan=ok",
        "dev: app prepare=start code=ok",
        "dev: app run stage=exit code=ok",
        "exited=1 exit=0",
        "dev: app caps"
    )) {
        [void]$Tokens.Add($Token)
    }
    if ($Format -eq "modulex") {
        [void]$Tokens.Add("dev: app format=modulex modulex=ok")
    } else {
        [void]$Tokens.Add("dev: app probe=ok")
    }
    foreach ($Token in $ExtraTokens) {
        foreach ($SplitToken in ([string]$Token).Split(",")) {
            if (-not [string]::IsNullOrWhiteSpace($SplitToken)) {
                [void]$Tokens.Add($SplitToken.Trim())
            }
        }
    }
    return ,$Tokens.ToArray()
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
        [string[]]$Tokens
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

    Write-Host "Dev Loader USB CDC App run smoke log validation"
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
    param([string]$Format = "elf")

    if ($Format -eq "modulex") {
        return @"
dev: usb ready
dev: stage=launch_ready code=ok received=212 crc=0x77012eee/0x77012eee
USB CDC packetstream transfer passed.
dev: app command=run name=modulex_hello_app run=enabled
dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000
dev: app read=ok bytes=212
dev: app stage=ok bytes=212
dev: app format=modulex modulex=ok
dev: app probe=invalid_argument entry_off=0x00000000 span=0 segments=0 runnable=0
dev: app plan=ok backend=0 load=0x24070000 entry=0x24070001 span=0 segments=0 runnable=0 run=disabled
dev: app prepare=start code=ok backend=0 argc=3 ready=1 entry=0x24070001 run=disabled
modulex_hello_app: charm_app_main entered
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070001 span=0 segments=0 exited=1 exit=0 app_exit=1 app_exit_code=0
dev: app caps console_bytes=80 present_count=0 present_bytes=0 sample0=0x00000000 input_polls=0
"@
    }

    return @"
dev: usb ready
dev: stage=launch_ready code=ok received=1234 crc=0x00000000/0x00000000
USB CDC packetstream transfer passed.
dev: app command=run name=hello_app run=enabled
dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000
dev: app read=ok bytes=1234
dev: app stage=ok bytes=1234
dev: app probe=ok entry_off=0x00000001 span=2048 segments=3 runnable=1
dev: app plan=ok backend=0 load=0x24070000 entry=0x24070001 span=2048 segments=3 runnable=1 run=disabled
dev: app prepare=start code=ok backend=0 argc=3 ready=1 entry=0x24070001 run=disabled
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070001 span=2048 segments=3 exited=1 exit=0 app_exit=0 app_exit_code=0
dev: app caps console_bytes=80 present_count=1 present_bytes=1024 sample0=0xff000000 input_polls=1
"@
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

function Invoke-SelfTest {
    foreach ($Format in @("elf", "modulex")) {
        $Extra = if ($Format -eq "modulex") { @("modulex_hello_app: charm_app_main entered") } else { @("present_count=1") }
        $Tokens = Get-DefaultTokens -Format $Format -ExtraTokens $Extra
        $PassingMissing = Get-MissingTokens -Text (Get-SyntheticPassingLog -Format $Format) -Tokens $Tokens
        if ($PassingMissing.Count -ne 0) {
            Write-Host "Self-test failed: synthetic passing log missed tokens for format=$Format."
            foreach ($Token in $PassingMissing) {
                Write-Host "  - $Token"
            }
            return 1
        }

        $FailingLog = (Get-SyntheticPassingLog -Format $Format).Replace("USB CDC packetstream transfer passed.", "USB CDC packetstream transfer failed.")
        $FailingMissing = Get-MissingTokens -Text $FailingLog -Tokens $Tokens
        if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "USB CDC packetstream transfer passed.") {
            Write-Host "Self-test failed: synthetic missing-token log was not classified as expected for format=$Format."
            foreach ($Token in $FailingMissing) {
                Write-Host "  - $Token"
            }
            return 1
        }
    }

    $Selected = Select-UsbPortFromSnapshots -Before @("COM16") -After @("COM16", "COM27") -ControlPort "COM16"
    if ($Selected -ne "COM27") {
        Write-Host "Self-test failed: USB CDC port selection returned $Selected."
        return 1
    }

    Write-Host "Dev Loader USB CDC App run capture self-test passed."
    return 0
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

$RequiredTokens = Get-DefaultTokens -Format $AppFormat -ExtraTokens $Expect
if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens)
}

if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    throw "PacketStream is required."
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}
if ([string]::IsNullOrWhiteSpace($AppName)) {
    throw "AppName must not be empty."
}
if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ($UsbEnumerateTimeoutSeconds -le 0) {
    throw "UsbEnumerateTimeoutSeconds must be greater than zero."
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

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_app_run_smoke.log"
}

$Sender = Join-Path $PSScriptRoot "send-dev-loader-usb-cdc-packetstream.ps1"
if (-not (Test-Path -LiteralPath $Sender)) {
    throw "USB CDC packetstream sender not found: $Sender"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)
$DownloadLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".usb-download.log")

if ($DryRun) {
    Write-Host "H747 Dev Loader USB CDC App run smoke dry run"
    Write-Host "  control port: $ControlPort"
    Write-Host "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto-discover after dev usb begin' } else { $UsbPort })"
    Write-Host "  packetstream: $ResolvedPacketStream"
    Write-Host "  app:          $AppName"
    Write-Host "  format:       $AppFormat"
    Write-Host "  args:         $AppArgs"
    Write-Host "  log:          $ResolvedLog"
    Write-Host "  reset session before begin: $(-not $NoResetSession)"
    exit 0
}

$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$LogWriter = $null
$ControlSerial = $null
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

function Read-UntilQuiet {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [int]$QuietMilliseconds = 120,
        [int]$MaxMilliseconds = 1000
    )

    $LastRead = [DateTime]::UtcNow
    $Deadline = [DateTime]::UtcNow.AddMilliseconds($MaxMilliseconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$script:Capture.Append($Chunk)
            Write-CaptureText $Chunk
            $LastRead = [DateTime]::UtcNow
        }
        if ((([DateTime]::UtcNow - $LastRead).TotalMilliseconds -ge $QuietMilliseconds)) {
            return
        }
        Start-Sleep -Milliseconds 20
    }
}

try {
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    Write-CaptureText "H747 Dev Loader USB CDC App run smoke`n"
    Write-CaptureText "  control port: $ControlPort`n"
    Write-CaptureText "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })`n"
    Write-CaptureText "  packetstream: $ResolvedPacketStream`n"
    Write-CaptureText "  app:          $AppName`n"
    Write-CaptureText "  format:       $AppFormat`n"
    Write-CaptureText "  args:         $AppArgs`n"
    Write-CaptureText "  log:          $ResolvedLog`n"
    Write-CaptureText "  download log: $DownloadLog`n`n"

    $PortsBefore = Get-SerialPortNames
    Write-CaptureText "serial ports before usb begin: $($PortsBefore -join ', ')`n"

    $ControlSerial = Open-ControlSerial
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    if (-not $NoResetSession) {
        $ControlSerial.WriteLine("dev packet reset-session")
        Write-CaptureText "[capture] sent: dev packet reset-session`n"
        Read-UntilQuiet -Serial $ControlSerial
    }

    $ControlSerial.WriteLine("dev usb begin")
    Write-CaptureText "[capture] sent: dev usb begin`n"
    if (-not (Wait-ForToken -Serial $ControlSerial -Token "dev: usb ready" -Deadline $Deadline)) {
        throw "Timed out waiting for usb ready."
    }
    Read-UntilQuiet -Serial $ControlSerial
    $ControlSerial.Close()
    $ControlSerial.Dispose()
    $ControlSerial = $null

    $ResolvedUsbPort = Wait-ForUsbPort `
        -Before $PortsBefore `
        -ControlPort $ControlPort `
        -RequestedUsbPort $UsbPort `
        -TimeoutSeconds $UsbEnumerateTimeoutSeconds
    Write-CaptureText "selected usb port: $ResolvedUsbPort`n`n"

    $LogWriter.Dispose()
    $LogWriter = $null

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
        throw "USB CDC packetstream sender failed with exit code $LASTEXITCODE"
    }

    $DownloadText = Get-Content -LiteralPath $DownloadLog -Raw -Encoding UTF8
    [void]$Capture.Append($DownloadText)
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $true, $Utf8NoBom)
    Write-CaptureText "`n--- USB CDC packetstream sender log ---`n"
    Write-CaptureText $DownloadText
    Write-CaptureText "`n--- App run commands ---`n"

    $ControlSerial = Open-ControlSerial
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    [void](Wait-ForToken -Serial $ControlSerial -Token "dev-loader>" -Deadline ([DateTime]::UtcNow.AddSeconds(2)))

    $Commands = @(
        "dev app probe $AppName",
        "dev app prepare $AppName $AppArgs".TrimEnd(),
        "dev app run $AppName $AppArgs".TrimEnd(),
        "dev app status"
    )
    foreach ($Command in $Commands) {
        $ControlSerial.WriteLine($Command)
        Write-CaptureText "`n[capture] sent: $Command`n"
        Start-Sleep -Milliseconds 100
    }

    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $ControlSerial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$Capture.Append($Chunk)
            Write-CaptureText $Chunk
        }
        $Missing = Get-MissingTokens -Text $Capture.ToString() -Tokens $RequiredTokens
        if ($Missing.Count -eq 0) {
            Write-CaptureText "`nUSB CDC App run smoke capture passed.`n"
            exit 0
        }
        Start-Sleep -Milliseconds 20
    }

    $FinalMissing = Get-MissingTokens -Text $Capture.ToString() -Tokens $RequiredTokens
    Write-Host ""
    Write-Host "USB CDC App run smoke capture failed. Missing tokens:"
    foreach ($Token in $FinalMissing) {
        Write-Host "  - $Token"
    }
    Write-Host "Log: $ResolvedLog"
    exit 1
} finally {
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
