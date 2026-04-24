param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$SmokeLogPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$ReportTitle = "",
    [string]$CheckTextPath = "",
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 30,
    [int]$TailLines = 40,
    [int]$Top = 10,
    [switch]$Clean,
    [switch]$StopOnFailure,
    [int]$MaxFailures = 0,
    [int]$MaxOtherResults = 0,
    [int]$RequireCaseCount = -1
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -Recurse -Force $Path
    }
}

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    throw "tool not found: $($Candidates -join ', ')"
}

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path $OutputRootPath $DefaultFileName
}

function Add-ScriptArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add($Value) | Out-Null
}

function Format-Number {
    param(
        $Value
    )

    return [Convert]::ToString($Value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Invoke-PowerShellFile {
    param(
        [string]$PowerShellExe,
        [string]$ScriptPath,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage,
        [switch]$AllowFailure
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($ScriptPath))

    $commandArgs = [System.Collections.Generic.List[string]]::new()
    $commandArgs.Add("-NoProfile") | Out-Null
    $commandArgs.Add("-ExecutionPolicy") | Out-Null
    $commandArgs.Add("Bypass") | Out-Null
    $commandArgs.Add("-File") | Out-Null
    $commandArgs.Add($ScriptPath) | Out-Null
    foreach ($entry in @($ArgumentList)) {
        $commandArgs.Add([string]$entry) | Out-Null
    }

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $PowerShellExe @commandArgs 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return $exitCode
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/minimal-kernel-runtime-armv7a-qemu-smoke" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
$resolvedReportTitle = if ([string]::IsNullOrWhiteSpace($ReportTitle)) { "Minimal Kernel ARMv7-A QEMU Lower-Half Smoke Report" } else { $ReportTitle }

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$smokeLogPathResolved = Get-OutputPath -ExplicitPath $SmokeLogPath -OutputRootPath $outputRootPath -DefaultFileName "smoke.log"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$reportLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "report.log"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$caseArtifactRoot = Join-Path $outputRootPath "cases"

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$smokeScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_armv7a_qemu_smoke.ps1"
$reportScript = Join-Path $PSScriptRoot "report_minimal_kernel_runtime_armv7a_qemu_smoke.ps1"
$checkScript = Join-Path $PSScriptRoot "check_minimal_kernel_runtime_armv7a_qemu_smoke_summary.ps1"
foreach ($scriptPath in @($smokeScript, $reportScript, $checkScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "missing script: $scriptPath"
    }
}

$smokeArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $smokeArgs -Name "-CMakeExe" -Value $CMakeExe
Add-ScriptArgument -Arguments $smokeArgs -Name "-QemuExe" -Value $QemuExe
Add-ScriptArgument -Arguments $smokeArgs -Name "-BuildJobs" -Value (Format-Number -Value $BuildJobs)
Add-ScriptArgument -Arguments $smokeArgs -Name "-TimeoutSec" -Value (Format-Number -Value $TimeoutSec)
Add-ScriptArgument -Arguments $smokeArgs -Name "-TailLines" -Value (Format-Number -Value $TailLines)
Add-ScriptArgument -Arguments $smokeArgs -Name "-SummaryPath" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $smokeArgs -Name "-CaseOutputRoot" -Value $caseArtifactRoot
if ($StopOnFailure) {
    $smokeArgs.Add("-StopOnFailure") | Out-Null
}

$reportArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $reportArgs -Name "-Summary" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $reportArgs -Name "-OutputPath" -Value $reportMarkdownPathResolved
Add-ScriptArgument -Arguments $reportArgs -Name "-Title" -Value $resolvedReportTitle
Add-ScriptArgument -Arguments $reportArgs -Name "-Top" -Value (Format-Number -Value $Top)

$checkArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $checkArgs -Name "-Summary" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxFailures" -Value (Format-Number -Value $MaxFailures)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxOtherResults" -Value (Format-Number -Value $MaxOtherResults)
Add-ScriptArgument -Arguments $checkArgs -Name "-RequireCaseCount" -Value (Format-Number -Value $RequireCaseCount)

Push-Location $repoRoot
try {
    try {
        $smokeExitCode = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $smokeScript `
            -ArgumentList $smokeArgs.ToArray() `
            -LogPath $smokeLogPathResolved `
            -FailureMessage "minimal kernel ARMv7-A qemu smoke run failed" `
            -AllowFailure

        if (-not (Test-Path $summaryPathResolved)) {
            if ($smokeExitCode -ne 0) {
                throw ("armv7a qemu smoke exited {0} and did not produce summary: {1}" -f $smokeExitCode, $summaryPathResolved)
            }

            throw "armv7a qemu smoke summary not found: $summaryPathResolved"
        }

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $reportScript `
            -ArgumentList $reportArgs.ToArray() `
            -LogPath $reportLogPathResolved `
            -FailureMessage "armv7a qemu smoke markdown report generation failed"

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $checkScript `
            -ArgumentList $checkArgs.ToArray() `
            -LogPath $checkTextPathResolved `
            -FailureMessage "armv7a qemu smoke summary gate failed"
    } finally {
        Write-Host "==> bundle"
        Write-Host "profile=armv7a-qemu-lower-half"
        Write-Host ("output_root={0}" -f $outputRootPath)
        Write-Host ("summary={0}" -f $summaryPathResolved)
        Write-Host ("smoke_log={0}" -f $smokeLogPathResolved)
        Write-Host ("case_artifacts={0}" -f $caseArtifactRoot)
        Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
        Write-Host ("check_text={0}" -f $checkTextPathResolved)
    }
} finally {
    Pop-Location
}
