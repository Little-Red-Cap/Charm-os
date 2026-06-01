param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [int]$TimeoutSeconds = 120,
    [string]$Log = "",
    [switch]$ResourceSmoke,
    [switch]$InputSmoke,
    [switch]$PlaybackSmoke
)

$ErrorActionPreference = "Stop"

if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_player_md3_smoke.log"
}

$LogDir = Split-Path -Parent $Log
if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
}

$PyOcd = Get-Command pyocd -ErrorAction Stop
$RequiredTokens = @(
    "real_md3=1",
    "mock=0",
    "smoke=1/11111",
    "exec_fail=0",
    "co=0",
    "to=0"
)
$ResourceTokens = @(
    "fs=",
    "font=",
    "cover=",
    "media="
)
$InputSmokeTokens = @(
    "input_smoke="
)
$PlaybackSmokeTokens = @(
    "playback=",
    "playback_smoke="
)
$StrictFields = @(
    "delta",
    "display",
    "sdram1",
    "rt_store",
    "boot",
    "render",
    "frames",
    "present",
    "content",
    "input",
    "t",
    "e",
    "p_src",
    "p_dst",
    "front",
    "back",
    "lfb",
    "lpf",
    "bytes"
)

$LogWriter = $null
$Serial = $null
$MatchedLine = $null
$ResourceMatchedLine = $null
$InputMatchedLine = $null
$PlaybackMatchedLine = $null
$PlaybackFailedLine = $null
$FailureReason = $null
$LastSmokeFailure = $null
$NeedResourceSmoke = $ResourceSmoke.IsPresent -or $PlaybackSmoke.IsPresent
$NeedInputSmoke = $InputSmoke.IsPresent
$NeedPlaybackSmoke = $PlaybackSmoke.IsPresent
$PlaybackResourceFailure = $null

function Write-CaptureLine {
    param([string]$Line)
    $LogWriter.WriteLine($Line)
    $LogWriter.Flush()
    Write-Host $Line
}

function Write-CaptureText {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) {
        return
    }
    $LogWriter.Write($Text)
    $LogWriter.Flush()
    Write-Host -NoNewline $Text
}

function Send-SerialLineSlow {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [string]$Line,
        [int]$EchoTimeoutMilliseconds = 30000
    )

    $EchoCapture = New-Object System.Text.StringBuilder
    foreach ($Ch in ($Line.ToCharArray())) {
        $SerialPort.Write([string]$Ch)
        $Deadline = [DateTime]::UtcNow.AddMilliseconds($EchoTimeoutMilliseconds)
        $Echoed = $false
        while ([DateTime]::UtcNow -lt $Deadline) {
            $Chunk = $SerialPort.ReadExisting()
            if ($Chunk.Length -gt 0) {
                [void]$EchoCapture.Append($Chunk)
                Write-CaptureText $Chunk
                if ($Chunk.Contains([string]$Ch)) {
                    $Echoed = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 50
        }
        if (-not $Echoed) {
            throw "Timed out waiting for command character echo: '$Ch'"
        }
    }
    $SerialPort.Write("`r")
    $LineDeadline = [DateTime]::UtcNow.AddMilliseconds($EchoTimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $LineDeadline) {
        $Chunk = $SerialPort.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$EchoCapture.Append($Chunk)
            Write-CaptureText $Chunk
            if (($Chunk.Contains("`n")) -or ($Chunk.Contains("`r"))) {
                break
            }
        }
        Start-Sleep -Milliseconds 50
    }
}

function Drain-SerialUntilPrompt {
    param(
        [System.IO.Ports.SerialPort]$SerialPort,
        [DateTime]$Deadline
    )

    $Capture = New-Object System.Text.StringBuilder
    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $SerialPort.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$Capture.Append($Chunk)
            Write-CaptureText $Chunk
            if ($Capture.ToString() -like "*h747-player-md3>*") {
                return $true
            }
        }
        Start-Sleep -Milliseconds 50
    }
    return $false
}

function Test-SmokeLine {
    param([string]$Line)
    return $null -eq (Get-SmokeLineFailureReason -Line $Line)
}

function Test-ResourceLine {
    param([string]$Line)
    foreach ($Token in $ResourceTokens) {
        if ($Line -notlike "*$Token*") {
            return $false
        }
    }
    return $true
}

function Get-PlaybackResourceFailureReason {
    param([string]$Line)
    $Fields = Get-StatusFields -Line $Line
    foreach ($Token in $ResourceTokens) {
        $Key = $Token.TrimEnd("=")
        if (-not $Fields.ContainsKey($Key)) {
            return "missing resource field: $Key"
        }
    }

    $Fs = $Fields["fs"]
    if ($Fs -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "fs shape invalid: $Fs"
    }
    $FsMount = [uint64]$Matches[1]
    $FsTracks = [uint64]$Matches[2]
    $FsHasTracks = [uint64]$Matches[3]
    $FsErr = [int64]$Matches[4]
    if (($FsMount -ne 1) -or ($FsTracks -eq 0) -or ($FsHasTracks -ne 1) -or ($FsErr -ne 0)) {
        return "playback requires fs=1/<n>/1/0 with n>0; got fs=$Fs"
    }

    $Font = $Fields["font"]
    if ($Font -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "font shape invalid: $Font"
    }
    $FontPrimary = [uint64]$Matches[1]
    $FontFallback = [uint64]$Matches[2]
    $FontRuntimeBound = [uint64]$Matches[3]
    $FontErr = [int64]$Matches[4]
    if (($FontPrimary -ne 1) -or ($FontRuntimeBound -ne 1) -or ($FontErr -ne 0)) {
        $FontCfg = if ($Fields.ContainsKey("font_cfg")) { $Fields["font_cfg"] } else { "missing" }
        return "playback requires font primary open and runtime bound; got font=$Font font_cfg=$FontCfg"
    }

    $Media = $Fields["media"]
    if ($Media -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "media shape invalid: $Media"
    }
    $MediaOpen = [uint64]$Matches[1]
    $MediaReady = [uint64]$Matches[3]
    $MediaErr = [int64]$Matches[4]
    if (($MediaOpen -ne 1) -or ($MediaReady -ne 1) -or ($MediaErr -ne 0)) {
        return "playback requires media=1/*/1/0; got media=$Media"
    }

    $Cover = $Fields["cover"]
    if ($Cover -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)x([0-9]+)/(-?[0-9]+)$") {
        return "cover shape invalid: $Cover"
    }

    return $null
}

function Test-InputSmokeLine {
    param([string]$Line)
    foreach ($Token in $InputSmokeTokens) {
        if ($Line -notlike "*$Token*") {
            return $false
        }
    }
    $Fields = Get-StatusFields -Line $Line
    if (-not $Fields.ContainsKey("input_smoke")) {
        return $false
    }
    $Value = $Fields["input_smoke"]
    if ($Value -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)-([0-9]+)/([0-9]+)/([0-9]+)$") {
        return $false
    }
    $Ok = [uint64]$Matches[1]
    $Cmds = [uint64]$Matches[2]
    $Before = [uint64]$Matches[3]
    $After = [uint64]$Matches[4]
    $Frames = [uint64]$Matches[5]
    $ExecFail = [uint64]$Matches[6]
    if (($Ok -ne 1) -or ($Cmds -lt 8) -or ($After -le $Before) -or
        ($Frames -eq 0) -or ($ExecFail -ne 0)) {
        return $false
    }

    if (-not $Fields.ContainsKey("input_route")) {
        return $false
    }
    $Route = $Fields["input_route"]
    if ($Route -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/([0-9]+)@([0-9]+)/([0-9]+)/([0-9]+)$") {
        return $false
    }
    $Console = [uint64]$Matches[1]
    $LastSource = [uint64]$Matches[5]
    $LastKind = [uint64]$Matches[6]
    $LastCode = [uint64]$Matches[7]
    return (($Console -ge $Cmds) -and ($LastSource -eq 1) -and
        ($LastKind -eq 1) -and ($LastCode -eq 4))
}

function Test-PlaybackSmokeLine {
    param([string]$Line)
    foreach ($Token in $PlaybackSmokeTokens) {
        if ($Line -notlike "*$Token*") {
            return $false
        }
    }
    $Fields = Get-StatusFields -Line $Line
    if (-not $Fields.ContainsKey("playback_smoke")) {
        return $false
    }
    $Value = $Fields["playback_smoke"]
    if ($Value -notmatch "^([0-9]+)/([0-9]+)-([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)/(-?[0-9]+)$") {
        return $false
    }
    $Ok = [uint64]$Matches[1]
    $Before = [uint64]$Matches[2]
    $After = [uint64]$Matches[3]
    $Frames = [uint64]$Matches[4]
    $SawPlaying = [uint64]$Matches[5]
    $Stage = [int64]$Matches[6]
    $Err = [int64]$Matches[7]
    return (($Ok -eq 1) -and ($After -gt $Before) -and ($Frames -gt 0) -and
        ($SawPlaying -eq 1) -and ($Stage -eq 0) -and ($Err -eq 0))
}

function Test-HasPlaybackSmokeField {
    param([string]$Line)
    $Fields = Get-StatusFields -Line $Line
    return $Fields.ContainsKey("playback_smoke")
}

function Get-InputSmokeSummary {
    param([string]$Line)
    $Fields = Get-StatusFields -Line $Line
    if (-not $Fields.ContainsKey("input_smoke")) {
        return "missing input_smoke field"
    }
    $Value = $Fields["input_smoke"]
    if ($Value -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)-([0-9]+)/([0-9]+)/([0-9]+)$") {
        return "input_smoke shape invalid: $Value"
    }
    $Ok = [uint64]$Matches[1]
    $Cmds = [uint64]$Matches[2]
    $Before = [uint64]$Matches[3]
    $After = [uint64]$Matches[4]
    $Frames = [uint64]$Matches[5]
    $ExecFail = [uint64]$Matches[6]
    $Delta = $After - $Before
    $RouteSummary = "input_route=missing"
    if ($Fields.ContainsKey("input_route")) {
        $Route = $Fields["input_route"]
        if ($Route -match "^([0-9]+)/([0-9]+)/([0-9]+)/([0-9]+)@([0-9]+)/([0-9]+)/([0-9]+)$") {
            $RouteSummary = "input_route=console$($Matches[1]) touch$($Matches[2]) encoder$($Matches[3]) button$($Matches[4]) last=$($Matches[5])/$($Matches[6])/$($Matches[7])"
        } else {
            $RouteSummary = "input_route=invalid:$Route"
        }
    }
    return "input_smoke=ok$Ok cmds=$Cmds events_delta=$Delta frames=$Frames exec_fail=$ExecFail $RouteSummary"
}

function Get-PlaybackSmokeSummary {
    param([string]$Line)
    $Fields = Get-StatusFields -Line $Line
    if (-not $Fields.ContainsKey("playback_smoke")) {
        return "missing playback_smoke field"
    }
    $Value = $Fields["playback_smoke"]
    if ($Value -notmatch "^([0-9]+)/([0-9]+)-([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)/(-?[0-9]+)$") {
        return "playback_smoke shape invalid: $Value"
    }
    $Ok = [uint64]$Matches[1]
    $Before = [uint64]$Matches[2]
    $After = [uint64]$Matches[3]
    $Frames = [uint64]$Matches[4]
    $SawPlaying = [uint64]$Matches[5]
    $Stage = [int64]$Matches[6]
    $Err = [int64]$Matches[7]
    $Delta = $After - $Before
    return "playback_smoke=ok$Ok callbacks_delta=$Delta frames=$Frames saw_playing=$SawPlaying stage=$Stage err=$Err"
}

function Get-ResourceSummary {
    param([string]$Line)
    $Fields = Get-StatusFields -Line $Line
    foreach ($Token in $ResourceTokens) {
        $Key = $Token.TrimEnd("=")
        if (-not $Fields.ContainsKey($Key)) {
            return "missing resource field: $Key"
        }
    }

    $Fs = $Fields["fs"]
    $Font = $Fields["font"]
    $Cover = $Fields["cover"]
    $Media = $Fields["media"]

    if ($Fs -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "resource fields present; fs shape invalid: $Fs"
    }
    $FsMount = [uint64]$Matches[1]
    $FsTracks = [uint64]$Matches[2]
    $FsHasTracks = [uint64]$Matches[3]
    $FsErr = [int64]$Matches[4]

    if ($Font -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "resource fields present; font shape invalid: $Font"
    }
    $FontPrimary = [uint64]$Matches[1]
    $FontFallback = [uint64]$Matches[2]
    $FontRuntimeBound = [uint64]$Matches[3]
    $FontErr = [int64]$Matches[4]
    $FontCfg = if ($Fields.ContainsKey("font_cfg")) { $Fields["font_cfg"] } else { "missing" }
    $FontCfgSummary = "font_cfg=$FontCfg"

    if ($Cover -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)x([0-9]+)/(-?[0-9]+)$") {
        return "resource fields present; cover shape invalid: $Cover"
    }
    $CoverFound = [uint64]$Matches[1]
    $CoverDecoded = [uint64]$Matches[2]
    $CoverW = [uint64]$Matches[3]
    $CoverH = [uint64]$Matches[4]
    $CoverErr = [int64]$Matches[5]

    if ($Media -notmatch "^([0-9]+)/([0-9]+)/([0-9]+)/(-?[0-9]+)$") {
        return "resource fields present; media shape invalid: $Media"
    }
    $MediaOpen = [uint64]$Matches[1]
    $MediaDuration = [uint64]$Matches[2]
    $MediaReady = [uint64]$Matches[3]
    $MediaErr = [int64]$Matches[4]

    $StorageState = if ($FsMount -eq 1) { "mounted" } else { "not-mounted(err=$FsErr)" }
    $TrackState = if (($FsTracks -gt 0) -and ($FsHasTracks -eq 1)) { "tracks=$FsTracks" } else { "no-tracks" }
    $FontState = if (($FontPrimary -eq 1) -and ($FontRuntimeBound -eq 1) -and ($FontErr -eq 0)) {
        if ($FontFallback -eq 1) { "font-primary+fallback-bound" } else { "font-primary-bound" }
    } elseif ($FontPrimary -eq 1) {
        "font-primary-present-not-bound(err=$FontErr)"
    } else {
        "font-primary-missing(err=$FontErr)"
    }
    $MediaState = if (($MediaOpen -eq 1) -and ($MediaReady -eq 1) -and ($MediaErr -eq 0)) {
        if ($MediaDuration -eq 1) { "media-ready+duration" } else { "media-ready(no-duration)" }
    } else {
        "media-missing(err=$MediaErr)"
    }
    $CoverState = if (($CoverFound -eq 1) -and ($CoverDecoded -eq 1) -and ($CoverW -gt 0) -and ($CoverH -gt 0) -and ($CoverErr -eq 0)) {
        "cover-ready(${CoverW}x${CoverH})"
    } elseif (($CoverFound -eq 1) -and ($CoverErr -eq -95)) {
        "cover-found-decoder-unavailable(err=$CoverErr)"
    } elseif ($CoverFound -eq 1) {
        "cover-found-decode-failed(err=$CoverErr)"
    } else {
        "cover-missing(err=$CoverErr)"
    }

    $Populated = (($FsMount -eq 1) -and ($FsTracks -gt 0) -and ($FsHasTracks -eq 1) -and
        ($FontPrimary -eq 1) -and ($FontRuntimeBound -eq 1) -and
        ($MediaOpen -eq 1) -and ($MediaReady -eq 1))
    $Mode = if ($Populated) { "populated" } else { "empty-or-missing" }
    return "resource=$Mode fs=$StorageState $TrackState font=$FontState $FontCfgSummary media=$MediaState cover=$CoverState"
}

function Get-StatusFields {
    param([string]$Line)
    $Fields = @{}
    foreach ($Part in ($Line -split "\s+")) {
        $Equals = $Part.IndexOf("=")
        if ($Equals -le 0) {
            continue
        }
        $Key = $Part.Substring(0, $Equals)
        $Value = $Part.Substring($Equals + 1)
        $Fields[$Key] = $Value
    }
    return ,$Fields
}

function Test-DecimalPairNonZero {
    param(
        [string]$Value,
        [string]$Name
    )
    if ($Value -notmatch "^([0-9]+)/([0-9]+)$") {
        return "${Name} must have <a>/<b> decimal shape"
    }
    if (([uint64]$Matches[1] -eq 0) -or ([uint64]$Matches[2] -eq 0)) {
        return "${Name} values must both be non-zero"
    }
    return $null
}

function Test-DecimalGreaterThanZero {
    param(
        [string]$Value,
        [string]$Name
    )
    if ($Value -notmatch "^[0-9]+$") {
        return "${Name} must be decimal"
    }
    if ([uint64]$Value -eq 0) {
        return "${Name} must be greater than zero"
    }
    return $null
}

function Test-ContentField {
    param([string]$Value)
    if ($Value -notmatch "^0x[0-9A-Fa-f]+:([0-9]+)@[0-9]+,[0-9]+-[0-9]+,[0-9]+$") {
        return "content must have <bg>:<non_bg>@<min>-<max> shape"
    }
    if ([uint64]$Matches[1] -eq 0) {
        return "content non-background pixel count must be greater than zero"
    }
    return $null
}

function Test-InputFields {
    param([hashtable]$Fields)
    if ($Fields["input"] -notmatch "^[0-9]+/[0-9]+$") {
        return "input must have <polls>/<events> shape"
    }
    if ($Fields["t"] -notmatch "^[0-9]+/[0-9]+/[0-9]+@[0-9]+,[0-9]+$") {
        return "t must have <probe>/<ready>/<down>@<x>,<y> shape"
    }
    if ($Fields["e"] -notmatch "^[0-9]+/[0-9]+/[0-9]+$") {
        return "e must have <touch>/<encoder>/<button> shape"
    }
    if ($Fields.ContainsKey("input_route") -and
        ($Fields["input_route"] -notmatch "^[0-9]+/[0-9]+/[0-9]+/[0-9]+@[0-9]+/[0-9]+/[0-9]+$")) {
        return "input_route must have <console>/<touch>/<encoder>/<button>@<src>/<kind>/<code> shape"
    }
    return $null
}

function Get-SmokeLineFailureReason {
    param([string]$Line)
    if ($Line -notmatch "^player_md3(\.loop)?\s") {
        return "line is not a player_md3 status line"
    }
    foreach ($Token in $RequiredTokens) {
        if ($Line -notlike "*$Token*") {
            return "missing token: $Token"
        }
    }

    $Fields = Get-StatusFields -Line $Line
    foreach ($Field in $StrictFields) {
        if (-not $Fields.ContainsKey($Field)) {
            return "missing field: $Field"
        }
    }

    $Reason = Test-DecimalPairNonZero -Value $Fields["delta"] -Name "delta"
    if ($null -ne $Reason) { return $Reason }
    $Reason = Test-DecimalGreaterThanZero -Value $Fields["frames"] -Name "frames"
    if ($null -ne $Reason) { return $Reason }
    $Reason = Test-DecimalGreaterThanZero -Value $Fields["present"] -Name "present"
    if ($null -ne $Reason) { return $Reason }
    if ($Fields["display"] -ne "1") { return "display must be 1" }
    if ($Fields["sdram1"] -ne "1/1") { return "sdram1 must be 1/1" }
    if ($Fields["rt_store"] -ne "1") { return "rt_store must be 1" }
    if ($Fields["boot"] -ne "1") { return "boot must be 1" }
    if ($Fields["render"] -ne "1") { return "render must be 1" }
    if ($Fields["bytes"] -ine "0x00384000") { return "bytes must be 0x00384000" }

    $Reason = Test-ContentField -Value $Fields["content"]
    if ($null -ne $Reason) { return $Reason }
    $Reason = Test-InputFields -Fields $Fields
    if ($null -ne $Reason) { return $Reason }

    return $null
}

try {
    $Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    $ResolvedLog = [System.IO.Path]::GetFullPath($Log)
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)

    Write-CaptureLine "H747 Lab Player MD3 smoke capture"
    Write-CaptureLine "  port:      $Port"
    Write-CaptureLine "  baud:      $BaudRate"
    Write-CaptureLine "  pyocd:     $($PyOcd.Source)"
    Write-CaptureLine "  probe:     $Probe"
    Write-CaptureLine "  target:    $Target"
    Write-CaptureLine "  frequency: $Frequency"
    Write-CaptureLine "  timeout:   ${TimeoutSeconds}s"
    Write-CaptureLine "  resource:  $NeedResourceSmoke"
    Write-CaptureLine "  input:     $NeedInputSmoke"
    Write-CaptureLine "  playback:  $NeedPlaybackSmoke"
    Write-CaptureLine "  log:       $ResolvedLog"
    Write-CaptureLine ""

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One)
    $Serial.Encoding = [System.Text.Encoding]::ASCII
    $Serial.NewLine = "`n"
    $Serial.ReadTimeout = 250
    $Serial.WriteTimeout = 1000
    $Serial.Open()
    $Serial.DiscardInBuffer()
    $Serial.DiscardOutBuffer()

    Write-CaptureLine "Serial open. Resetting target through pyOCD commander..."
    $PyOcdArgs = @(
        "commander",
        "-u", $Probe,
        "-t", $Target,
        "-f", $Frequency,
        "-c", "reset halt",
        "-c", "go",
        "-c", "exit"
    )
    $PyOcdStdout = [System.IO.Path]::GetTempFileName()
    $PyOcdStderr = [System.IO.Path]::GetTempFileName()
    try {
        $PyOcdProcess = Start-Process `
            -FilePath $PyOcd.Source `
            -ArgumentList $PyOcdArgs `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $PyOcdStdout `
            -RedirectStandardError $PyOcdStderr
        foreach ($Line in (Get-Content -Path $PyOcdStdout -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            Write-CaptureLine "[pyocd:stdout] $Line"
        }
        foreach ($Line in (Get-Content -Path $PyOcdStderr -Encoding UTF8 -ErrorAction SilentlyContinue)) {
            Write-CaptureLine "[pyocd:stderr] $Line"
        }
        $PyOcdExit = $PyOcdProcess.ExitCode
    } finally {
        Remove-Item -LiteralPath $PyOcdStdout -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $PyOcdStderr -Force -ErrorAction SilentlyContinue
    }
    if ($PyOcdExit -ne 0) {
        $FailureReason = "pyocd commander failed with exit code $PyOcdExit"
    } else {
        Write-CaptureLine "Capturing serial until a valid smoke line appears..."
        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while ([DateTime]::UtcNow -lt $Deadline) {
            try {
                $Line = $Serial.ReadLine().TrimEnd("`r", "`n")
            } catch [System.TimeoutException] {
                continue
            }

            if ($Line.Length -eq 0) {
                continue
            }

            Write-CaptureLine $Line
            $SmokeFailure = Get-SmokeLineFailureReason -Line $Line
            if ($null -eq $SmokeFailure) {
                $MatchedLine = $Line
                if (-not $NeedResourceSmoke -and -not $NeedInputSmoke -and -not $NeedPlaybackSmoke) {
                    break
                }
                if (Test-ResourceLine $Line) {
                    $ResourceMatchedLine = $Line
                    if ($NeedPlaybackSmoke) {
                        $PlaybackResourceFailure = Get-PlaybackResourceFailureReason -Line $Line
                    }
                    if (-not $NeedInputSmoke -and -not $NeedPlaybackSmoke) {
                        break
                    }
                }
                if (Test-InputSmokeLine $Line) {
                    $InputMatchedLine = $Line
                    if (-not $NeedPlaybackSmoke) {
                        break
                    }
                }
                if (Test-PlaybackSmokeLine $Line) {
                    $PlaybackMatchedLine = $Line
                    if (-not $NeedInputSmoke -or ($null -ne $InputMatchedLine)) {
                        break
                    }
                }
            } elseif ($Line -like "player_md3*") {
                $LastSmokeFailure = $SmokeFailure
            }

            if ($NeedInputSmoke -and ($null -ne $MatchedLine) -and ($null -eq $InputMatchedLine)) {
                # MD3 rendering is heavy enough that the board-side polled UART RX path can
                # overrun when a full command line is sent at host speed. Wait for the prompt
                # when possible, then pace the command one byte at a time.
                $PromptDeadline = [DateTime]::UtcNow.AddSeconds(5)
                [void](Drain-SerialUntilPrompt -SerialPort $Serial -Deadline $PromptDeadline)
                Write-CaptureLine "Injecting input smoke command..."
                Send-SerialLineSlow -SerialPort $Serial -Line "input smoke"
                $InputDeadline = [DateTime]::UtcNow.AddSeconds([Math]::Max(30, $TimeoutSeconds))
                while ([DateTime]::UtcNow -lt $InputDeadline) {
                    try {
                        $InputLine = $Serial.ReadLine().TrimEnd("`r", "`n")
                    } catch [System.TimeoutException] {
                        continue
                    }
                    if ($InputLine.Length -eq 0) {
                        continue
                    }
                    Write-CaptureLine $InputLine
                    if ($null -ne (Get-SmokeLineFailureReason -Line $InputLine)) {
                        continue
                    }
                    if (Test-InputSmokeLine $InputLine) {
                        $InputMatchedLine = $InputLine
                        break
                    }
                }
                if (($null -ne $InputMatchedLine) -and (-not $NeedPlaybackSmoke)) {
                    break
                }
            }

            if ($NeedPlaybackSmoke -and ($null -ne $MatchedLine) -and
                ($null -ne $ResourceMatchedLine) -and ($null -eq $PlaybackMatchedLine)) {
                $PromptDeadline = [DateTime]::UtcNow.AddSeconds(5)
                [void](Drain-SerialUntilPrompt -SerialPort $Serial -Deadline $PromptDeadline)
                Write-CaptureLine "Injecting playback smoke command..."
                Send-SerialLineSlow -SerialPort $Serial -Line "playback smoke"
                $PlaybackDeadline = [DateTime]::UtcNow.AddSeconds([Math]::Max(60, $TimeoutSeconds))
                while ([DateTime]::UtcNow -lt $PlaybackDeadline) {
                    try {
                        $PlaybackLine = $Serial.ReadLine().TrimEnd("`r", "`n")
                    } catch [System.TimeoutException] {
                        continue
                    }
                    if ($PlaybackLine.Length -eq 0) {
                        continue
                    }
                    Write-CaptureLine $PlaybackLine
                    if ($null -ne (Get-SmokeLineFailureReason -Line $PlaybackLine)) {
                        continue
                    }
                    if (Test-ResourceLine $PlaybackLine) {
                        $ResourceMatchedLine = $PlaybackLine
                        $PlaybackResourceFailure = Get-PlaybackResourceFailureReason -Line $PlaybackLine
                    }
                    if (Test-PlaybackSmokeLine $PlaybackLine) {
                        $PlaybackMatchedLine = $PlaybackLine
                        break
                    }
                    if (Test-HasPlaybackSmokeField $PlaybackLine) {
                        $PlaybackFailedLine = $PlaybackLine
                        break
                    }
                }
                if ($null -ne $PlaybackMatchedLine) {
                    break
                }
                if ($null -ne $PlaybackFailedLine) {
                    if ($null -ne $PlaybackResourceFailure) {
                        $FailureReason = "playback resource precondition failed: $PlaybackResourceFailure; $(Get-PlaybackSmokeSummary -Line $PlaybackFailedLine)"
                    } else {
                        $FailureReason = "playback smoke command failed: $(Get-PlaybackSmokeSummary -Line $PlaybackFailedLine)"
                    }
                    break
                }
            }
        }

        if ($null -eq $MatchedLine) {
            if ($null -ne $LastSmokeFailure) {
                $FailureReason = "timed out before matching a valid smoke line; last failure: $LastSmokeFailure"
            } else {
                $FailureReason = "timed out before matching a valid smoke line"
            }
        } elseif ($null -ne $FailureReason) {
            # Keep the more specific failure captured inside the command/state machine.
        } elseif ($NeedResourceSmoke -and ($null -eq $ResourceMatchedLine)) {
            $FailureReason = "timed out before matching resource smoke fields on a valid strict status line"
        } elseif ($NeedInputSmoke -and ($null -eq $InputMatchedLine)) {
            $FailureReason = "timed out before matching input smoke evidence on a valid strict status line"
        } elseif ($NeedPlaybackSmoke -and ($null -ne $PlaybackResourceFailure)) {
            $FailureReason = "playback resource precondition failed: $PlaybackResourceFailure"
        } elseif ($NeedPlaybackSmoke -and ($null -eq $PlaybackMatchedLine)) {
            $FailureReason = "timed out before matching playback smoke evidence on a valid strict status line"
        }
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

if (($null -ne $MatchedLine) -and
    ((-not $NeedResourceSmoke) -or ($null -ne $ResourceMatchedLine)) -and
    ((-not $NeedInputSmoke) -or ($null -ne $InputMatchedLine)) -and
    ((-not $NeedPlaybackSmoke) -or ($null -ne $PlaybackMatchedLine))) {
    Write-Host ""
    Write-Host "Smoke capture passed."
    Write-Host "Matched line: $MatchedLine"
    if ($NeedResourceSmoke) {
        Write-Host "Resource line: $ResourceMatchedLine"
        Write-Host (Get-ResourceSummary -Line $ResourceMatchedLine)
    }
    if ($NeedInputSmoke) {
        Write-Host "Input line: $InputMatchedLine"
        Write-Host (Get-InputSmokeSummary -Line $InputMatchedLine)
    }
    if ($NeedPlaybackSmoke) {
        Write-Host "Playback line: $PlaybackMatchedLine"
        Write-Host (Get-PlaybackSmokeSummary -Line $PlaybackMatchedLine)
    }
    Write-Host "Log: $Log"
    exit 0
}

Write-Host ""
Write-Host "Smoke capture failed: $FailureReason"
Write-Host "Log: $Log"
exit 1
