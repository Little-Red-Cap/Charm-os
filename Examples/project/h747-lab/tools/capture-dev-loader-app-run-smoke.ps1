param(
    [string]$PacketStream = "",
    [string]$AppName = "received_app",
    [string]$AppArgs = "",
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [int]$TimeoutSeconds = 45,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 0,
    [string]$Log = "",
    [string[]]$Expect = @(),
    [string]$ValidateLog = "",
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
    param([string[]]$ExtraTokens)

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Token in @(
        "dev: stage=launch_ready code=ok",
        "dev: app command=run",
        "dev: app run-region name=ram_d1_app_elf base=0x24070000 size=65536 align=16 linked_elf_base=0x24070000",
        "dev: app read=ok",
        "dev: app stage=ok",
        "dev: app probe=ok",
        "dev: app plan=ok",
        "dev: app prepare=start code=ok",
        "dev: app run stage=exit code=ok",
        "exited=1 exit=0",
        "dev: app caps"
    )) {
        [void]$Tokens.Add($Token)
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

    Write-Host "Dev Loader App run smoke log validation"
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
dev: stage=launch_ready code=ok received=1234 crc=0x00000000/0x00000000
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

function Invoke-SelfTest {
    $Tokens = Get-DefaultTokens -ExtraTokens @("present_count=1")
    $PassingMissing = Get-MissingTokens -Text (Get-SyntheticPassingLog) -Tokens $Tokens
    if ($PassingMissing.Count -ne 0) {
        Write-Host "Self-test failed: synthetic passing log missed tokens."
        foreach ($Token in $PassingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    $FailingLog = (Get-SyntheticPassingLog).Replace("dev: app run stage=exit code=ok", "dev: app run stage=start code=ok")
    $FailingMissing = Get-MissingTokens -Text $FailingLog -Tokens $Tokens
    if ($FailingMissing.Count -ne 1 -or $FailingMissing[0] -ne "dev: app run stage=exit code=ok") {
        Write-Host "Self-test failed: synthetic missing-token log was not classified as expected."
        foreach ($Token in $FailingMissing) {
            Write-Host "  - $Token"
        }
        return 1
    }

    Write-Host "Dev Loader App run capture self-test passed."
    return 0
}

if ($SelfTest) {
    exit (Invoke-SelfTest)
}

$RequiredTokens = Get-DefaultTokens -ExtraTokens $Expect
if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Validate-LogFile -Path $ValidateLog -Tokens $RequiredTokens)
}

if ([string]::IsNullOrWhiteSpace($PacketStream)) {
    throw "PacketStream is required."
}
if (-not (Test-Path -LiteralPath $PacketStream)) {
    throw "PacketStream not found: $PacketStream"
}
if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_dev_loader_app_run_smoke.log"
}

$RawLog = [System.IO.Path]::ChangeExtension($Log, ".raw-download.log")
$Sender = Join-Path $PSScriptRoot "send-dev-loader-raw-packetstream.ps1"
if (-not (Test-Path -LiteralPath $Sender)) {
    throw "Raw packetstream sender not found: $Sender"
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $Sender `
    -PacketStream $PacketStream `
    -Port $Port `
    -BaudRate $BaudRate `
    -TimeoutSeconds $TimeoutSeconds `
    -WriteChunkSize $WriteChunkSize `
    -InterChunkDelayMs $InterChunkDelayMs `
    -Log $RawLog `
    -WaitPrompt
if ($LASTEXITCODE -ne 0) {
    throw "Raw packetstream sender failed with exit code $LASTEXITCODE"
}

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$LogWriter = $null
$Serial = $null
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

function Wait-ForPrompt {
    param(
        [System.IO.Ports.SerialPort]$Serial,
        [DateTime]$Deadline
    )

    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$script:Capture.Append($Chunk)
            Write-CaptureText $Chunk
            if ($script:Capture.ToString().IndexOf("dev-loader>", [System.StringComparison]::Ordinal) -ge 0) {
                return $true
            }
        }
        Start-Sleep -Milliseconds 20
    }
    return $false
}

try {
    $ResolvedLog = [System.IO.Path]::GetFullPath($Log)
    $LogDir = Split-Path -Parent $ResolvedLog
    if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
    }
    $LogWriter = [System.IO.StreamWriter]::new($ResolvedLog, $false, $Utf8NoBom)
    $RawText = Get-Content -LiteralPath $RawLog -Raw -Encoding UTF8
    [void]$Capture.Append($RawText)
    Write-CaptureText "H747 Dev Loader App run smoke`n"
    Write-CaptureText "  port:         $Port`n"
    Write-CaptureText "  baud:         $BaudRate`n"
    Write-CaptureText "  packetstream: $((Resolve-Path -LiteralPath $PacketStream).Path)`n"
    Write-CaptureText "  app:          $AppName`n"
    Write-CaptureText "  args:         $AppArgs`n"
    Write-CaptureText "  raw log:      $RawLog`n"
    Write-CaptureText "  log:          $ResolvedLog`n`n"
    Write-CaptureText $RawText
    Write-CaptureText "`n"

    $Serial = [System.IO.Ports.SerialPort]::new(
        $Port,
        $BaudRate,
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

    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    [void](Wait-ForPrompt -Serial $Serial -Deadline ([DateTime]::UtcNow.AddSeconds(2)))

    $Commands = @(
        "dev app probe $AppName",
        "dev app prepare $AppName $AppArgs".TrimEnd(),
        "dev app run $AppName $AppArgs".TrimEnd(),
        "dev app status"
    )
    foreach ($Command in $Commands) {
        $Serial.WriteLine($Command)
        Write-CaptureText "`n[capture] sent: $Command`n"
        Start-Sleep -Milliseconds 100
    }

    while ([DateTime]::UtcNow -lt $Deadline) {
        $Chunk = $Serial.ReadExisting()
        if ($Chunk.Length -gt 0) {
            [void]$Capture.Append($Chunk)
            Write-CaptureText $Chunk
        }
        $Missing = Get-MissingTokens -Text $Capture.ToString() -Tokens $RequiredTokens
        if ($Missing.Count -eq 0) {
            Write-CaptureText "`nApp run smoke capture passed.`n"
            exit 0
        }
        Start-Sleep -Milliseconds 20
    }

    $Missing = Get-MissingTokens -Text $Capture.ToString() -Tokens $RequiredTokens
    Write-Host ""
    Write-Host "App run smoke capture failed. Missing tokens:"
    foreach ($Token in $Missing) {
        Write-Host "  - $Token"
    }
    Write-Host "Log: $ResolvedLog"
    exit 1
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
