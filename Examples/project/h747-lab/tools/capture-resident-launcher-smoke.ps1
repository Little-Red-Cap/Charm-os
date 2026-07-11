param(
    [string]$Port = "COM16",
    [int]$BaudRate = 115200,
    [int]$TimeoutSeconds = 60,
    [string]$Log = "",
    [string]$ValidateLog = "",
    [string[]]$Expect = @(),
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if ($TimeoutSeconds -le 0) {
    throw "invalid_argument: TimeoutSeconds must be greater than zero"
}

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\resident_launcher_smoke.log"
}

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

function Get-ResidentLauncherDefaultTokens {
    param([string[]]$ExtraTokens)

    $Tokens = New-Object System.Collections.Generic.List[string]
    foreach ($Token in @(
        "resident_launcher: init",
        "resident_launcher: catalog code=ok count=",
        "resident_launcher: run ",
        "resident_launcher: app record source=emmc_fat format=elf name=",
        " stage=exit code=ok exit=0 ",
        " load=0x",
        " entry=0x",
        " span=",
        " segments="
    )) {
        [void]$Tokens.Add($Token)
    }
    foreach ($Token in $ExtraTokens) {
        foreach ($Part in ([string]$Token -split ",")) {
            if (-not [string]::IsNullOrWhiteSpace($Part)) {
                [void]$Tokens.Add($Part.Trim())
            }
        }
    }
    return ,$Tokens.ToArray()
}

function Get-ResidentLauncherMissingTokens {
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

function Invoke-ResidentLauncherLogValidation {
    param(
        [string]$Path,
        [string[]]$Tokens
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "invalid_argument: ValidateLog path must not be empty"
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "log_missing: $Path"
    }

    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $Text = Get-Content -LiteralPath $ResolvedPath -Raw -Encoding UTF8
    $Missing = Get-ResidentLauncherMissingTokens -Text $Text -Tokens $Tokens

    Write-Host "Resident Launcher smoke log validation"
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

function Get-ResidentLauncherSyntheticPassingLog {
    return @"
resident_launcher: init
resident_launcher: catalog code=ok count=2
resident_launcher: run HELLO_APP.ELF
hello_app: charm_app_main entered
resident_launcher: app record source=emmc_fat format=elf name=HELLO_APP.ELF stage=exit code=ok exit=0 load=0x24070000 entry=0x24070021 span=512 segments=2
"@
}

function Get-ResidentLauncherSyntheticFailureLog {
    return @"
resident_launcher: init
resident_launcher: catalog code=directory_missing count=0
"@
}

function Invoke-ResidentLauncherCapture {
    param(
        [string]$Port,
        [int]$BaudRate,
        [int]$TimeoutSeconds,
        [string]$Log
    )

    $LogDir = Split-Path -Parent $Log
    if (-not [string]::IsNullOrWhiteSpace($LogDir)) {
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
    }

    $Serial = $null
    $Writer = $null
    $Text = New-Object System.Text.StringBuilder
    try {
        $Serial = [System.IO.Ports.SerialPort]::new($Port, $BaudRate, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
        $Serial.Encoding = [System.Text.Encoding]::UTF8
        $Serial.ReadTimeout = 200
        $Serial.WriteTimeout = 200
        $Serial.Open()
        $Writer = [System.IO.StreamWriter]::new($Log, $false, [System.Text.Encoding]::UTF8)

        $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while ([DateTime]::UtcNow -lt $Deadline) {
            try {
                $Chunk = $Serial.ReadExisting()
                if (-not [string]::IsNullOrEmpty($Chunk)) {
                    [void]$Text.Append($Chunk)
                    $Writer.Write($Chunk)
                    $Writer.Flush()
                    Write-Host -NoNewline $Chunk
                }
            } catch [System.TimeoutException] {
            }
            Start-Sleep -Milliseconds 50
        }
    } finally {
        if ($null -ne $Writer) {
            $Writer.Dispose()
        }
        if ($null -ne $Serial) {
            if ($Serial.IsOpen) {
                $Serial.Close()
            }
            $Serial.Dispose()
        }
    }
    return $Text.ToString()
}

function Invoke-ResidentLauncherSmokeSelfTest {
    $TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("charm_launcher_capture_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $TempRoot | Out-Null
    try {
        $PassLog = Join-Path $TempRoot "pass.log"
        $FailLog = Join-Path $TempRoot "fail.log"
        Get-ResidentLauncherSyntheticPassingLog | Set-Content -Path $PassLog -Encoding UTF8
        Get-ResidentLauncherSyntheticFailureLog | Set-Content -Path $FailLog -Encoding UTF8
        $Tokens = Get-ResidentLauncherDefaultTokens -ExtraTokens @()
        if ((Invoke-ResidentLauncherLogValidation -Path $PassLog -Tokens $Tokens) -ne 0) {
            return $false
        }
        if ((Invoke-ResidentLauncherLogValidation -Path $FailLog -Tokens $Tokens) -eq 0) {
            return $false
        }
        return $true
    } finally {
        if (Test-Path -LiteralPath $TempRoot) {
            Remove-Item -LiteralPath $TempRoot -Recurse -Force
        }
    }
}

$Tokens = Get-ResidentLauncherDefaultTokens -ExtraTokens $Expect

if ($SelfTest) {
    if (Invoke-ResidentLauncherSmokeSelfTest) {
        Write-Host "SelfTest passed."
        exit 0
    }
    Write-Host "SelfTest failed."
    exit 1
}

if (-not [string]::IsNullOrWhiteSpace($ValidateLog)) {
    exit (Invoke-ResidentLauncherLogValidation -Path $ValidateLog -Tokens $Tokens)
}

if ($DryRun) {
    Write-Host "Resident Launcher capture smoke dry run"
    Write-Host "  port: $Port"
    Write-Host "  baud: $BaudRate"
    Write-Host "  timeout_seconds: $TimeoutSeconds"
    Write-Host "  log: $Log"
    Write-Host "  required tokens:"
    foreach ($Token in $Tokens) {
        Write-Host "    - $Token"
    }
    exit 0
}

$Text = Invoke-ResidentLauncherCapture -Port $Port -BaudRate $BaudRate -TimeoutSeconds $TimeoutSeconds -Log $Log
$Missing = Get-ResidentLauncherMissingTokens -Text $Text -Tokens $Tokens
if ($Missing.Count -ne 0) {
    Write-Host ""
    Write-Host "Resident Launcher smoke failed. Missing tokens:"
    foreach ($Token in $Missing) {
        Write-Host "  - $Token"
    }
    exit 1
}

Write-Host ""
Write-Host "Resident Launcher smoke passed."
Write-Host "  log: $Log"
exit 0
