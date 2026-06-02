param(
    [string]$ArtifactManifest = "",
    [string]$ControlPort = "COM16",
    [int]$ControlBaudRate = 115200,
    [int]$TimeoutSeconds = 20,
    [int]$RepeatPerMedia = 1,
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
        [int]$RepeatPerMedia,
        [object[]]$Artifacts
    )

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Name in $MediaList) {
        [void]$Tokens.Add("=== installed-store media $Name ===")
        [void]$Tokens.Add("media $Name passed")
        $ListToken = if ($Name -eq "emmc") { "dev: store emmc entries=3" } else { "dev: store entries=3" }
        [void]$Tokens.Add($ListToken)
        [void]$Tokens.Add("dev: app command=run name=${Name}:hello_app run=enabled")
        [void]$Tokens.Add("dev: app command=run name=${Name}:player_min run=enabled")
        [void]$Tokens.Add("dev: app command=run name=${Name}:modulex_hello_app run=enabled")
        foreach ($Artifact in $Artifacts) {
            [void]$Tokens.Add("name=$($Artifact.Name)")
            [void]$Tokens.Add("format=$($Artifact.Format)")
            [void]$Tokens.Add(("flags=0x{0:x8}" -f $Artifact.StoreFlags))
        }
        for ($Index = 1; $Index -le $RepeatPerMedia; ++$Index) {
            [void]$Tokens.Add("installed-store repeat $Index/$RepeatPerMedia passed")
        }
    }
    [void]$Tokens.Add("Installed Store matrix smoke passed.")
    return ,$Tokens.ToArray()
}

function Get-RequiredCounts {
    param(
        [string[]]$MediaList,
        [int]$RepeatPerMedia
    )

    $Counts = New-Object System.Collections.Generic.List[object]
    $TotalRepeats = $MediaList.Count * $RepeatPerMedia
    [void]$Counts.Add(@{ Token = "hello_app: charm_app_main entered"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "player_min: presented one frame"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "modulex_hello_app: charm_app_main entered"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "dev: app format=modulex modulex=ok"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "relocated=1"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "dev: app run stage=exit code=ok"; Count = 3 * $TotalRepeats })
    [void]$Counts.Add(@{ Token = "exited=1 exit=0"; Count = 3 * $TotalRepeats })
    [void]$Counts.Add(@{ Token = "present_count=1"; Count = $TotalRepeats })
    [void]$Counts.Add(@{ Token = "input_polls=1"; Count = $TotalRepeats })
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

    $ListLine = if ($Name -eq "emmc") { "dev: store emmc entries=3" } else { "dev: store entries=3" }
    return @"
=== installed-store media $Name ===
$ListLine
  [0] name=hello_app format=elf offset=0x000000a0 size=5132 flags=0x00000000 runnable=1
  [1] name=player_min format=elf offset=0x000014b0 size=5168 flags=0x00000000 runnable=1
  [2] name=modulex_hello_app format=modulex offset=0x00002900 size=268 flags=0x00000001 runnable=1
hello_app: charm_app_main entered
dev: app command=run name=${Name}:hello_app run=enabled
dev: app run stage=exit code=ok backend=0 exited=1 exit=0
player_min: presented one frame
dev: app command=run name=${Name}:player_min run=enabled
dev: app run stage=exit code=ok backend=0 exited=1 exit=0
dev: app caps console_bytes=32 present_count=1 present_bytes=1024 sample0=0xff51a851 input_polls=1
modulex_hello_app: charm_app_main entered
dev: app command=run name=${Name}:modulex_hello_app run=enabled
dev: app format=modulex modulex=ok
dev: app modulex diag validate=ok dep=ok dep_index=0 relocated=1 entry_off=0x00000000 span=268
dev: app run stage=exit code=ok backend=0 exited=1 exit=0
installed-store repeat 1/1 passed
media $Name passed
"@
}

function Get-SyntheticPassingLog {
    param([string[]]$MediaList)

    $Text = New-Object System.Text.StringBuilder
    foreach ($Name in $MediaList) {
        [void]$Text.Append((Get-SyntheticMediaLog -Name $Name))
    }
    [void]$Text.AppendLine("Installed Store matrix smoke passed.")
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

    Write-Host "Dev Loader Installed Store matrix log validation"
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
    if (-not (Test-CharmResidentArtifactManifestSelfTest)) {
        Write-Host "Self-test failed: artifact manifest helper checks failed."
        return 1
    }
    $MediaList = Get-MediaList -RawMedia @("qspi,emmc", "qspi")
    if ($MediaList.Count -ne 2 -or $MediaList[0] -ne "qspi" -or $MediaList[1] -ne "emmc") {
        Write-Host "Self-test failed: media parsing did not preserve expected order."
        return 1
    }

    $SyntheticArtifacts = @(
        [pscustomobject]@{ Name = "hello_app"; Format = "elf"; StoreFlags = [uint32]0 },
        [pscustomobject]@{ Name = "player_min"; Format = "elf"; StoreFlags = [uint32]0 },
        [pscustomobject]@{ Name = "modulex_hello_app"; Format = "modulex"; StoreFlags = [uint32]1 }
    )
    $Tokens = Get-RequiredTokens -MediaList $MediaList -RepeatPerMedia 1 -Artifacts $SyntheticArtifacts
    $Counts = Get-RequiredCounts -MediaList $MediaList -RepeatPerMedia 1
    $Passing = Get-SyntheticPassingLog -MediaList $MediaList
    $Missing = Get-MissingEvidence -Text $Passing -Tokens $Tokens -Counts $Counts
    if ($Missing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing log missed tokens."
        foreach ($Token in $Missing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $Failing = $Passing.Replace("media emmc passed", "media emmc failed")
    $FailingMissing = Get-MissingEvidence -Text $Failing -Tokens $Tokens -Counts $Counts
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "media emmc passed") {
        Write-Host "Self-test failed: synthetic failing log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Dev Loader Installed Store matrix smoke self-test passed."
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

function Invoke-InstalledStoreRun {
    param(
        [string]$Name,
        [int]$Index
    )

    $Serial = $null
    try {
        $Serial = Open-ControlSerial
        Start-Sleep -Milliseconds 300
        [void]$Serial.ReadExisting()
        [void](Send-ControlCommand -Serial $Serial -Command "dev store status" -Phase "store_status" -Timeout $TimeoutSeconds)
        [void](Send-ControlCommand -Serial $Serial -Command "dev store list $Name" -Phase "store_list" -Timeout $TimeoutSeconds)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run ${Name}:hello_app alpha beta" -Phase "hello_run" -Timeout $TimeoutSeconds)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run ${Name}:player_min" -Phase "player_min_run" -Timeout $TimeoutSeconds)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app run ${Name}:modulex_hello_app alpha beta" -Phase "modulex_run" -Timeout $TimeoutSeconds)
        [void](Send-ControlCommand -Serial $Serial -Command "dev app status" -Phase "app_status" -Timeout $TimeoutSeconds)
        Write-CaptureText "`ninstalled-store repeat $Index/$RepeatPerMedia passed`n"
    } finally {
        if (($null -ne $Serial) -and $Serial.IsOpen) {
            $Serial.Close()
        }
        if ($null -ne $Serial) {
            $Serial.Dispose()
        }
    }
}

if ($RepeatPerMedia -le 0) {
    throw "RepeatPerMedia must be greater than zero."
}
if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}
if ($ControlBaudRate -le 0) {
    throw "ControlBaudRate must be greater than zero."
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$Manifest = Read-CharmResidentArtifactManifest -Path $ArtifactManifest
$MediaList = Get-MediaList -RawMedia $Media
$RequiredTokens = Get-RequiredTokens -MediaList $MediaList -RepeatPerMedia $RepeatPerMedia -Artifacts $Manifest.Artifacts
$RequiredCounts = Get-RequiredCounts -MediaList $MediaList -RepeatPerMedia $RepeatPerMedia

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens -Counts $RequiredCounts)
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_installed_store_matrix_smoke.log"
}
$ResolvedLog = [System.IO.Path]::GetFullPath($Log)

if ($DryRun) {
    Write-Host "H747 Dev Loader Installed Store matrix smoke dry run"
    Write-Host "  media:        $($MediaList -join ', ')"
    Write-Host "  repeat/media: $RepeatPerMedia"
    Write-Host "  control port: $ControlPort"
    Write-Host "  manifest:     $($Manifest.Path)"
    Write-Host "  commands:     dev store status; dev store list <media>; dev app run <media>:hello_app alpha beta; dev app run <media>:player_min; dev app run <media>:modulex_hello_app alpha beta; dev app status"
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
    Write-CaptureText "H747 Dev Loader Installed Store matrix smoke`n"
    Write-CaptureText "  media:        $($MediaList -join ', ')`n"
    Write-CaptureText "  repeat/media: $RepeatPerMedia`n"
    Write-CaptureText "  control port: $ControlPort`n"
    Write-CaptureText "  manifest:     $($Manifest.Path)`n"
    Write-CaptureText "  log:          $ResolvedLog`n`n"

    foreach ($Name in $MediaList) {
        Write-CaptureText "`n=== installed-store media $Name ===`n"
        for ($Index = 1; $Index -le $RepeatPerMedia; ++$Index) {
            Invoke-InstalledStoreRun -Name $Name -Index $Index
        }
        Write-CaptureText "`nmedia $Name passed`n"
    }

    Write-CaptureText "`nInstalled Store matrix smoke passed.`n"
    $Missing = Get-MissingEvidence -Text $script:Capture.ToString() -Tokens $RequiredTokens -Counts $RequiredCounts
    if ($Missing.Count -ne 0) {
        Write-CaptureText "`nmissing_token: $($Missing -join '; ')`n"
        throw "missing_token: installed store matrix missed required evidence"
    }
    exit 0
} catch {
    $Failed = $true
    Write-CaptureText "`nInstalled Store matrix smoke failed: $($_.Exception.Message)`n"
    throw
} finally {
    if ($null -ne $script:LogWriter) {
        $script:LogWriter.Dispose()
    }
    if ($Failed) {
        Write-Host "Log: $ResolvedLog"
    }
}
