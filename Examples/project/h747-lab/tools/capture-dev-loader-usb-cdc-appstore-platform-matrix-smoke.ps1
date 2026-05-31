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
    [int]$RepeatPerMedia = 1,
    [ValidateSet("qspi", "emmc")]
    [string[]]$Media = @("qspi", "emmc"),
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
    param(
        [string[]]$MediaList,
        [int]$RepeatPerMedia
    )

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Name in $MediaList) {
        [void]$Tokens.Add("=== matrix media $Name ===")
        [void]$Tokens.Add("media $Name passed")
        [void]$Tokens.Add("dev: store install $Name receive=ok")
        [void]$Tokens.Add("dev: app command=run name=${Name}:hello_app run=enabled")
        [void]$Tokens.Add("dev: app command=run name=${Name}:player_min run=enabled")
        [void]$Tokens.Add("$Name written=10416")
        [void]$Tokens.Add("  ${Name}:")
        for ($Index = 1; $Index -le $RepeatPerMedia; ++$Index) {
            [void]$Tokens.Add("platform repeat $Index/$RepeatPerMedia passed")
        }
    }
    [void]$Tokens.Add("Resident App Store platform matrix smoke passed.")
    return ,$Tokens.ToArray()
}

function Get-RequiredCounts {
    param(
        [string[]]$MediaList,
        [int]$RepeatPerMedia
    )

    $Counts = New-Object System.Collections.Generic.List[object]
    $TotalRepeats = $MediaList.Count * $RepeatPerMedia
    [void]$Counts.Add(@{ Token = "USB CDC packetstream transfer passed."; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "dev: stage=launch_ready code=ok received=10416"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "crc=0x73de4894/0x73de4894"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "store=ok code=ok"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "hello_app: charm_app_main entered"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "player_min: presented one frame"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "hello_app exit=0"; Count = $MediaList.Count })
    [void]$Counts.Add(@{ Token = "player_min exit=0"; Count = $MediaList.Count })
    return ,$Counts.ToArray()
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

function Get-SyntheticMediaLog {
    param([string]$Name)

    $ListLine = if ($Name -eq "emmc") { "dev: store emmc entries=2" } else { "dev: store entries=2" }
    return @"
=== matrix media $Name ===
USB CDC packetstream transfer passed.
dev: stage=launch_ready code=ok received=10416 crc=0x73de4894/0x73de4894
dev: store install $Name receive=ok recv_bytes=10416 store=ok code=ok target=0x00000000 written=10416 erased=10752
$ListLine
  [0] name=hello_app offset=0x00000070 size=5132 flags=0x00000000 runnable=1
  [1] name=player_min offset=0x00001480 size=5168 flags=0x00000000 runnable=1
hello_app: charm_app_main entered
hello_app: argv1=alpha
dev: app command=run name=${Name}:hello_app run=enabled
dev: app run stage=exit code=ok backend=0 exited=1 exit=0
player_min: presented one frame
dev: app command=run name=${Name}:player_min run=enabled
dev: app run stage=exit code=ok backend=0 exited=1 exit=0
Summary:
  usb throughput: 13.20 KiB/s
  $Name written=10416 erased=10752
  hello_app exit=0
  player_min exit=0
platform repeat 1/1 passed
media $Name passed
  ${Name}: log=matrix.$Name.log throughput=13.20 KiB/s written=10416 erased=10752 hello_app=0 player_min=0
"@
}

function Get-SyntheticPassingLog {
    param(
        [string[]]$MediaList,
        [int]$RepeatPerMedia
    )

    $Text = New-Object System.Text.StringBuilder
    foreach ($Name in $MediaList) {
        for ($Index = 1; $Index -le $RepeatPerMedia; ++$Index) {
            [void]$Text.Append((Get-SyntheticMediaLog -Name $Name))
        }
    }
    [void]$Text.AppendLine("Resident App Store platform matrix smoke passed.")
    return $Text.ToString()
}

function Validate-LogFile {
    param(
        [string]$Path,
        [string[]]$Tokens,
        [object[]]$Counts
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "ValidateLog path must not be empty."
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Log file not found: $Path"
    }

    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Text = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    $Missing = Get-MissingEvidence -Text $Text -Tokens $Tokens -Counts $Counts

    Write-Host "Dev Loader USB CDC App Store platform matrix log validation"
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

function Invoke-SelfTest {
    $MediaList = @("qspi", "emmc")
    $Tokens = Get-RequiredTokens -MediaList $MediaList -RepeatPerMedia 1
    $Counts = Get-RequiredCounts -MediaList $MediaList -RepeatPerMedia 1
    $Passing = Get-SyntheticPassingLog -MediaList $MediaList -RepeatPerMedia 1
    $Missing = Get-MissingEvidence -Text $Passing -Tokens $Tokens -Counts $Counts
    if ($Missing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing matrix missed tokens."
        foreach ($Token in $Missing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $Failing = $Passing.Replace("media emmc passed", "media emmc failed")
    $FailingMissing = Get-MissingEvidence -Text $Failing -Tokens $Tokens -Counts $Counts
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "media emmc passed") {
        Write-Host "Self-test failed: synthetic failing matrix was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $Parsed = Get-MediaList -RawMedia @("qspi,emmc", "qspi")
    if ($Parsed.Count -ne 2 -or $Parsed[0] -ne "qspi" -or $Parsed[1] -ne "emmc") {
        Write-Host "Self-test failed: media parsing did not preserve expected order."
        return 1
    }

    Write-Host "Dev Loader USB CDC App Store platform matrix smoke self-test passed."
    return 0
}

if ($RepeatPerMedia -le 0) {
    throw "RepeatPerMedia must be greater than zero."
}

$MediaList = Get-MediaList -RawMedia $Media
$RequiredTokens = Get-RequiredTokens -MediaList $MediaList -RepeatPerMedia $RepeatPerMedia
$RequiredCounts = Get-RequiredCounts -MediaList $MediaList -RepeatPerMedia $RepeatPerMedia

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens -Counts $RequiredCounts)
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    $PacketStream = Join-Path $ProjectRoot "..\..\app_abi\elf_samples\out\appstore.bin.packetstream"
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_usb_cdc_appstore_platform_matrix_smoke.log"
}

$PlatformSmoke = Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-platform-smoke.ps1"
if (-not (Test-Path -LiteralPath $PlatformSmoke)) {
    throw "Platform smoke not found: $PlatformSmoke"
}

$ResolvedPacketStream = (Resolve-Path -LiteralPath $PacketStream).Path
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)

if ($DryRun) {
    Write-Host "H747 Dev Loader USB CDC App Store platform matrix smoke dry run"
    Write-Host "  media:        $($MediaList -join ', ')"
    Write-Host "  repeat/media: $RepeatPerMedia"
    Write-Host "  control port: $ControlPort"
    Write-Host "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto-discover per media' } else { $UsbPort })"
    Write-Host "  packetstream: $ResolvedPacketStream"
    Write-Host "  write chunk:  $WriteChunkSize"
    Write-Host "  chunk delay:  ${InterChunkDelayMs}ms"
    Write-Host "  log:          $ResolvedLog"
    foreach ($Name in $MediaList) {
        Write-Host "  command[$Name]: platform-smoke -Media $Name -Repeat $RepeatPerMedia"
    }
    exit 0
}

$LogDir = Split-Path -Parent $ResolvedLog
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$Writer = $null
$Failed = $false
$Capture = New-Object System.Text.StringBuilder

function Write-MatrixText {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) {
        return
    }
    if ($null -ne $script:Writer) {
        $script:Writer.Write($Text)
        $script:Writer.Flush()
    }
    [void]$script:Capture.Append($Text)
    Write-Host -NoNewline $Text
}

try {
    $script:Writer = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    Write-MatrixText "H747 Dev Loader USB CDC App Store platform matrix smoke`n"
    Write-MatrixText "  media:        $($MediaList -join ', ')`n"
    Write-MatrixText "  repeat/media: $RepeatPerMedia`n"
    Write-MatrixText "  control port: $ControlPort`n"
    Write-MatrixText "  usb port:     $(if ([string]::IsNullOrWhiteSpace($UsbPort)) { 'auto' } else { $UsbPort })`n"
    Write-MatrixText "  packetstream: $ResolvedPacketStream`n"
    Write-MatrixText "  write chunk:  $WriteChunkSize`n"
    Write-MatrixText "  chunk delay:  ${InterChunkDelayMs}ms`n"
    Write-MatrixText "  log:          $ResolvedLog`n`n"

    $Summary = New-Object System.Collections.Generic.List[string]
    foreach ($Name in $MediaList) {
        Write-MatrixText "`n=== matrix media $Name ===`n"
        $MediaLog = [System.IO.Path]::ChangeExtension($ResolvedLog, ".$Name.log")
        $Args = @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $PlatformSmoke,
            "-PacketStream", $ResolvedPacketStream,
            "-ControlPort", $ControlPort,
            "-ControlBaudRate", $ControlBaudRate,
            "-UsbBaudRate", $UsbBaudRate,
            "-TimeoutSeconds", $TimeoutSeconds,
            "-UsbEnumerateTimeoutSeconds", $UsbEnumerateTimeoutSeconds,
            "-UsbSettleMilliseconds", $UsbSettleMilliseconds,
            "-WriteChunkSize", $WriteChunkSize,
            "-InterChunkDelayMs", $InterChunkDelayMs,
            "-Repeat", $RepeatPerMedia,
            "-Media", $Name,
            "-Log", $MediaLog
        )
        if (-not [string]::IsNullOrWhiteSpace($UsbPort)) {
            $Args += @("-UsbPort", $UsbPort)
        }

        & powershell @Args
        if ($LASTEXITCODE -ne 0) {
            throw "media_failed: $Name platform smoke failed with exit code $LASTEXITCODE"
        }

        $MediaText = Get-Content -LiteralPath $MediaLog -Raw -Encoding UTF8
        Write-MatrixText "`n--- media $Name log ---`n"
        Write-MatrixText $MediaText

        $ThroughputMatches = [regex]::Matches($MediaText, "throughput: ([0-9.]+ KiB/s) to launch_ready")
        $Throughputs = @($ThroughputMatches | ForEach-Object { $_.Groups[1].Value })
        $Install = [regex]::Match($MediaText, "$Name written=(\d+) erased=(\d+)")
        $ThroughputText = if ($Throughputs.Count -gt 0) { $Throughputs -join "," } else { "n/a" }
        $WrittenText = if ($Install.Success) { $Install.Groups[1].Value } else { "n/a" }
        $ErasedText = if ($Install.Success) { $Install.Groups[2].Value } else { "n/a" }
        [void]$Summary.Add("  ${Name}: log=$MediaLog throughput=$ThroughputText written=$WrittenText erased=$ErasedText hello_app=0 player_min=0")
        Write-MatrixText "`nmedia $Name passed`n"
    }

    Write-MatrixText "`nMatrix summary:`n"
    foreach ($Line in $Summary) {
        Write-MatrixText "$Line`n"
    }
    Write-MatrixText "`nResident App Store platform matrix smoke passed.`n"

    $Text = $Capture.ToString()
    $Missing = Get-MissingEvidence -Text $Text -Tokens $RequiredTokens -Counts $RequiredCounts
    if ($Missing.Count -ne 0) {
        Write-MatrixText "`nmissing_token: $($Missing -join '; ')`n"
        throw "missing_token: platform matrix missed required evidence"
    }
    exit 0
} catch {
    $Failed = $true
    Write-MatrixText "`nResident App Store platform matrix smoke failed: $($_.Exception.Message)`n"
    throw
} finally {
    if ($null -ne $script:Writer) {
        $script:Writer.Dispose()
    }
    if ($Failed) {
        Write-Host "Log: $ResolvedLog"
    }
}
