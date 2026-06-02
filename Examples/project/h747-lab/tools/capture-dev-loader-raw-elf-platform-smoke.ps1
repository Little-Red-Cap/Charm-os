param(
    [string]$ArtifactManifest = "",
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [int]$TimeoutSeconds = 60,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 0,
    [ValidateSet("qspi", "emmc")]
    [string[]]$Media = @("qspi", "emmc"),
    [string]$Log = "",
    [string]$ValidateLog = "",
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "charm-resident-artifacts.ps1")

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

function Get-MediaList {
    param([string[]]$RawMedia)

    $Seen = New-Object System.Collections.Generic.HashSet[string]
    $Result = New-Object System.Collections.Generic.List[string]
    foreach ($Item in $RawMedia) {
        foreach ($Part in ($Item -split ",")) {
            $Name = $Part.Trim().ToLowerInvariant()
            if ([string]::IsNullOrWhiteSpace($Name)) {
                continue
            }
            if ($Name -ne "qspi" -and $Name -ne "emmc") {
                throw "Unsupported media '$Name'. Expected qspi or emmc."
            }
            if ($Seen.Add($Name)) {
                [void]$Result.Add($Name)
            }
        }
    }
    if ($Result.Count -eq 0) {
        throw "At least one media must be selected."
    }
    return ,$Result.ToArray()
}

function Get-RequiredTokens {
    param([string[]]$MediaList)

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Token in @(
        "=== received elf: hello_app ===",
        "received elf hello_app passed",
        "Raw packetstream transfer passed.",
        "dev: stage=launch_ready code=ok",
        "dev: app record source=received format=elf name=hello_app command=run",
        "dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000",
        "dev: app probe=ok",
        "dev: app plan=ok",
        "dev: app run stage=exit code=ok",
        "exited=1 exit=0",
        "hello_app: charm_app_main entered"
    )) {
        [void]$Tokens.Add($Token)
    }

    foreach ($Name in $MediaList) {
        foreach ($Token in @(
            "=== store elf media $Name ===",
            "store elf media $Name passed",
            "dev: store install $Name receive=ok",
            "store=ok code=ok",
            "name=hello_app format=elf",
            "name=player_min format=elf",
            "dev: app command=run name=${Name}:hello_app run=enabled",
            "dev: app record source=$Name format=elf name=hello_app command=run",
            "dev: app command=run name=${Name}:player_min run=enabled",
            "dev: app record source=$Name format=elf name=player_min command=run",
            "hello_app: charm_app_main entered",
            "player_min: presented one frame",
            "present_count=1",
            "input_polls=1"
        )) {
            [void]$Tokens.Add($Token)
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
    Write-Host "Resident ELF raw platform smoke log validation"
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

function Get-SyntheticMediaLog {
    param([string]$Name)

    return @"
=== store elf media $Name ===
Raw packetstream transfer passed.
dev: stage=launch_ready code=ok received=10732 crc=0x73de4894/0x73de4894
dev: store install $Name receive=ok recv_bytes=10732 store=ok code=ok target=0x00000000 written=10732 erased=12288
  [0] name=hello_app format=elf offset=0x000000a0 size=5132 flags=0x00000000 runnable=1
  [1] name=player_min format=elf offset=0x000014b0 size=5168 flags=0x00000000 runnable=1
hello_app: charm_app_main entered
dev: app command=run name=${Name}:hello_app run=enabled
dev: app record source=$Name format=elf name=hello_app command=run argv=3 load=0x24070000 entry=0x24070021 span=270 segments=2 run_stage=exit run_code=ok exited=1 exit=0 caps_console=32 caps_present=0 caps_input=0
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070021 span=270 segments=2 exited=1 exit=0 app_exit=0 app_exit_code=0
player_min: presented one frame
dev: app command=run name=${Name}:player_min run=enabled
dev: app record source=$Name format=elf name=player_min command=run argv=1 load=0x24070000 entry=0x24070001 span=1280 segments=3 run_stage=exit run_code=ok exited=1 exit=0 caps_console=32 caps_present=1 caps_input=1
dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070001 span=1280 segments=3 exited=1 exit=0 app_exit=0 app_exit_code=0
dev: app caps console_bytes=32 present_count=1 present_bytes=1024 sample0=0xff51a851 input_polls=1
store elf media $Name passed
"@
}

function Get-SyntheticPassingLog {
    param([string[]]$MediaList)

    $Text = New-Object System.Text.StringBuilder
    [void]$Text.AppendLine("=== received elf: hello_app ===")
    [void]$Text.AppendLine("Raw packetstream transfer passed.")
    [void]$Text.AppendLine("dev: stage=launch_ready code=ok received=5132 crc=0x44810c41/0x44810c41")
    [void]$Text.AppendLine("hello_app: charm_app_main entered")
    [void]$Text.AppendLine("dev: app record source=received format=elf name=hello_app command=run argv=3 load=0x24070000 entry=0x24070021 span=270 segments=2 run_stage=exit run_code=ok exited=1 exit=0 caps_console=32 caps_present=0 caps_input=0")
    [void]$Text.AppendLine("dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000")
    [void]$Text.AppendLine("dev: app probe=ok entry_off=0x00000021 span=270 segments=2 runnable=1")
    [void]$Text.AppendLine("dev: app plan=ok backend=0 load=0x24070000 entry=0x24070021 span=270 segments=2 runnable=1 run=disabled")
    [void]$Text.AppendLine("dev: app run stage=exit code=ok backend=0 load=0x24070000 entry=0x24070021 span=270 segments=2 exited=1 exit=0 app_exit=0 app_exit_code=0")
    [void]$Text.AppendLine("received elf hello_app passed")
    foreach ($Name in $MediaList) {
        [void]$Text.Append((Get-SyntheticMediaLog -Name $Name))
    }
    [void]$Text.AppendLine("Resident ELF raw platform smoke passed.")
    return $Text.ToString()
}

function Invoke-SelfTest {
    if (-not (Test-CharmResidentArtifactManifestSelfTest)) {
        Write-Host "Self-test failed: artifact manifest helper checks failed."
        return 1
    }
    $MediaList = Get-MediaList -RawMedia @("qspi,emmc", "qspi")
    if ($MediaList.Count -ne 2 -or $MediaList[0] -ne "qspi" -or $MediaList[1] -ne "emmc") {
        Write-Host "Self-test failed: media parsing did not preserve expected order."
        return 1
    }
    $Tokens = Get-RequiredTokens -MediaList $MediaList
    $PassingMissing = Get-MissingTokens -Text (Get-SyntheticPassingLog -MediaList $MediaList) -Tokens $Tokens
    if ($PassingMissing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing log missed tokens."
        foreach ($Token in $PassingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $FailingLog = (Get-SyntheticPassingLog -MediaList $MediaList).Replace("dev: app record source=emmc format=elf name=player_min command=run", "dev: app record source=emmc format=modulex name=player_min command=run")
    $FailingMissing = Get-MissingTokens -Text $FailingLog -Tokens $Tokens
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "dev: app record source=emmc format=elf name=player_min command=run") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Resident ELF raw platform smoke self-test passed."
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
    [void]$script:Capture.Append($Text)
    Write-Host -NoNewline $Text
}

function Merge-LogFile {
    param(
        [string]$Path,
        [string]$Header
    )

    Write-CaptureText "`n--- $Header ---`n"
    if (Test-Path -LiteralPath $Path) {
        Write-CaptureText (Get-Content -LiteralPath $Path -Raw -Encoding UTF8)
    } else {
        Write-CaptureText "missing log: $Path`n"
    }
}

function Open-ControlSerial {
    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
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

function Invoke-MediaElfRun {
    param([string]$Name)

    Write-CaptureText "`n=== store elf media $Name ===`n"
    $Serial = $null
    try {
        $Serial = Open-ControlSerial
        Start-Sleep -Milliseconds 300
        [void]$Serial.ReadExisting()
        [void](Send-ControlCommand -Serial $Serial -Command "dev store install $Name" -Phase "${Name}_install" -Timeout 45)
        [void](Send-ControlCommand -Serial $Serial -Command "dev store list $Name" -Phase "${Name}_list" -Timeout 10)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run ${Name}:hello_app alpha beta" -Phase "${Name}_hello" -Timeout 15)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app status" -Phase "${Name}_hello_status" -Timeout 10)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run ${Name}:player_min" -Phase "${Name}_player" -Timeout 15)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app status" -Phase "${Name}_player_status" -Timeout 10)
        if ($Name -eq "emmc") {
            [void](Send-ControlCommand -Serial $Serial -Command "dev store status" -Phase "emmc_status" -Timeout 10)
        }
        Write-CaptureText "`nstore elf media $Name passed`n"
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

$MediaList = Get-MediaList -RawMedia $Media
$RequiredTokens = Get-RequiredTokens -MediaList $MediaList
if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens)
}
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

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$Manifest = Read-CharmResidentArtifactManifest -Path $ArtifactManifest
$Hello = Get-CharmResidentArtifact -Manifest $Manifest -Name "hello_app"
$Player = Get-CharmResidentArtifact -Manifest $Manifest -Name "player_min"
if ($Hello.Format -ne "elf" -or $Player.Format -ne "elf") {
    throw "format_mismatch: hello_app and player_min must be ELF artifacts."
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_raw_elf_platform_smoke.log"
}
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)

$AppRunSmoke = Join-Path $PSScriptRoot "capture-dev-loader-app-run-smoke.ps1"
$RawSender = Join-Path $PSScriptRoot "send-dev-loader-raw-packetstream.ps1"
foreach ($Script in @($AppRunSmoke, $RawSender)) {
    if (-not (Test-Path -LiteralPath $Script)) {
        throw "Required script not found: $Script"
    }
}

if ($DryRun) {
    Write-Host "Resident ELF raw platform smoke dry run"
    Write-Host "  manifest: $($Manifest.Path)"
    Write-Host "  port:     $Port"
    Write-Host "  baud:     $BaudRate"
    Write-Host "  media:    $($MediaList -join ', ')"
    Write-Host "  received: $($Hello.PacketStreamPath)"
    Write-Host "  store:    $($Manifest.StorePacketStreamPath)"
    Write-Host "  log:      $ResolvedLog"
    exit 0
}

$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$script:LogWriter = $null
$script:Capture = New-Object System.Text.StringBuilder

try {
    $script:LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    Write-CaptureText "Resident ELF raw platform smoke`n"
    Write-CaptureText "  manifest: $($Manifest.Path)`n"
    Write-CaptureText "  port:     $Port`n"
    Write-CaptureText "  baud:     $BaudRate`n"
    Write-CaptureText "  media:    $($MediaList -join ', ')`n"
    Write-CaptureText "  log:      $ResolvedLog`n`n"

    Write-CaptureText "=== received elf: hello_app ===`n"
    $ReceivedLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".received-hello.log")
    & powershell -NoProfile -ExecutionPolicy Bypass -File $AppRunSmoke `
        -PacketStream $Hello.PacketStreamPath `
        -AppName "hello_app" `
        -AppArgs "alpha beta" `
        -Port $Port `
        -BaudRate $BaudRate `
        -TimeoutSeconds $TimeoutSeconds `
        -WriteChunkSize $WriteChunkSize `
        -InterChunkDelayMs $InterChunkDelayMs `
        -Log $ReceivedLog `
        -Expect "dev: app record source=received format=elf name=hello_app command=run","argv=3","hello_app: charm_app_main entered","hello_app: argv1=alpha"
    if ($LASTEXITCODE -ne 0) {
        throw "received_elf_failed: hello_app app-run smoke exited with $LASTEXITCODE"
    }
    Merge-LogFile -Path $ReceivedLog -Header "received hello_app log"
    Write-CaptureText "`nreceived elf hello_app passed`n"

    foreach ($Name in $MediaList) {
        $TransferLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".$Name-store-download.log")
        & powershell -NoProfile -ExecutionPolicy Bypass -File $RawSender `
            -PacketStream $Manifest.StorePacketStreamPath `
            -Port $Port `
            -BaudRate $BaudRate `
            -TimeoutSeconds $TimeoutSeconds `
            -WriteChunkSize $WriteChunkSize `
            -InterChunkDelayMs $InterChunkDelayMs `
            -Log $TransferLog `
            -WaitPrompt
        if ($LASTEXITCODE -ne 0) {
            throw "store_transfer_failed: $Name raw transfer exited with $LASTEXITCODE"
        }
        Merge-LogFile -Path $TransferLog -Header "$Name store raw transfer log"
        Invoke-MediaElfRun -Name $Name
    }

    $Text = $script:Capture.ToString()
    $Missing = Get-MissingTokens -Text $Text -Tokens $RequiredTokens
    if ($Missing.Count -ne 0) {
        Write-CaptureText "`nResident ELF raw platform smoke failed. Missing tokens:`n"
        foreach ($Token in $Missing) {
            Write-CaptureText "  - $Token`n"
        }
        exit 1
    }

    Write-CaptureText "`nResident ELF raw platform smoke passed.`n"
    exit 0
} finally {
    if ($null -ne $script:LogWriter) {
        $script:LogWriter.Dispose()
    }
}
