param(
    [ValidateSet("ci", "daily")]
    [string]$Profile = "ci",
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$SmokeLogPath = "",
    [string]$InspectTextPath = "",
    [string]$InspectJsonPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$ReportTitle = "",
    [string]$CheckTextPath = "",
    [string]$BaselineSummary = "",
    [string]$CMakeExe = "cmake",
    [string]$Generator = "Ninja",
    [int]$Jobs = 0,
    [int]$Top = 10,
    [switch]$Clean,
    [switch]$StopOnFailure,
    [string[]]$Examples,
    [int]$MaxFailures = 0,
    [int]$MaxOtherResults = 0,
    [int64]$MaxTotalElapsedMs = -1,
    [int64]$MaxAverageElapsedMs = -1,
    [int64]$MaxMaxElapsedMs = -1,
    [int]$MaxRegressionCount = -1,
    [int64]$MaxRegressionMs = -1,
    [double]$MaxRegressionPct = -1,
    [switch]$AllowAddedExamples,
    [switch]$AllowRemovedExamples
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

function Get-DefaultOutputRoot {
    param(
        [string]$BundleProfile
    )

    switch ($BundleProfile) {
        "daily" { return "out/minimal-kernel-runtime-host-smoke-daily" }
        default { return "out/minimal-kernel-runtime-host-smoke-ci" }
    }
}

function Get-DefaultReportTitle {
    param(
        [string]$BundleProfile
    )

    switch ($BundleProfile) {
        "daily" { return "Minimal Kernel Host Smoke Daily Report" }
        default { return "Minimal Kernel Host Smoke CI Report" }
    }
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

function Add-StringArrayScriptArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string[]]$Values
    )

    if (@($Values).Count -eq 0) {
        return
    }

    $filteredValues = @(
        @($Values) |
            Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [string]$_ }
    )

    if ($filteredValues.Count -eq 0) {
        return
    }

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add(($filteredValues -join ",")) | Out-Null
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

    & $PowerShellExe @commandArgs 2>&1 | Tee-Object -FilePath $LogPath
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }

    return $exitCode
}

function Resolve-SmokeScriptPath {
    param(
        [string]$BundleProfile
    )

    switch ($BundleProfile) {
        "daily" { return (Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_daily.ps1") }
        default { return (Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_ci.ps1") }
    }
}

function Add-ProfileCheckModeArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$BundleProfile
    )

    switch ($BundleProfile) {
        "daily" { $Arguments.Add("-RequireConfigureReused") | Out-Null }
        default { $Arguments.Add("-RequireConfigureExecuted") | Out-Null }
    }
}

function Add-ProfileSmokeModeArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$BundleProfile
    )

    switch ($BundleProfile) {
        "ci" { $Arguments.Add("-KeepBuildDirs") | Out-Null }
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { Get-DefaultOutputRoot -BundleProfile $Profile } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot
$resolvedReportTitle = if ([string]::IsNullOrWhiteSpace($ReportTitle)) { Get-DefaultReportTitle -BundleProfile $Profile } else { $ReportTitle }

if ($Profile -eq "daily") {
    Write-Host "==> profile-note: daily bundle expects warmed cmake-build-verify-* directories"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$smokeLogPathResolved = Get-OutputPath -ExplicitPath $SmokeLogPath -OutputRootPath $outputRootPath -DefaultFileName "smoke.log"
$inspectTextPathResolved = Get-OutputPath -ExplicitPath $InspectTextPath -OutputRootPath $outputRootPath -DefaultFileName "inspect.txt"
$inspectJsonPathResolved = Get-OutputPath -ExplicitPath $InspectJsonPath -OutputRootPath $outputRootPath -DefaultFileName "inspect.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$reportLogPathResolved = Get-OutputPath -ExplicitPath "" -OutputRootPath $outputRootPath -DefaultFileName "report.log"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$baselineSummaryPathResolved = Resolve-FullPath -Path $BaselineSummary

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$smokeScript = Resolve-SmokeScriptPath -BundleProfile $Profile
$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_host_smoke.ps1"
$reportScript = Join-Path $PSScriptRoot "report_minimal_kernel_runtime_host_smoke.ps1"
$checkScript = Join-Path $PSScriptRoot "check_minimal_kernel_runtime_host_smoke_summary.ps1"
foreach ($scriptPath in @($smokeScript, $inspectScript, $reportScript, $checkScript)) {
    if (-not (Test-Path $scriptPath)) {
        throw "missing script: $scriptPath"
    }
}

$smokeArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $smokeArgs -Name "-CMakeExe" -Value $CMakeExe
Add-ScriptArgument -Arguments $smokeArgs -Name "-Generator" -Value $Generator
Add-ScriptArgument -Arguments $smokeArgs -Name "-SummaryPath" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $smokeArgs -Name "-Jobs" -Value (Format-Number -Value $Jobs)
if ($StopOnFailure) {
    $smokeArgs.Add("-StopOnFailure") | Out-Null
}
Add-StringArrayScriptArgument -Arguments $smokeArgs -Name "-Examples" -Values $Examples
Add-ProfileSmokeModeArgument -Arguments $smokeArgs -BundleProfile $Profile

$inspectArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $inspectArgs -Name "-Summary" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $inspectArgs -Name "-Top" -Value (Format-Number -Value $Top)
if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathResolved)) {
    Add-ScriptArgument -Arguments $inspectArgs -Name "-BaselineSummary" -Value $baselineSummaryPathResolved
}

$inspectJsonArgs = [System.Collections.Generic.List[string]]::new()
foreach ($entry in @($inspectArgs)) {
    $inspectJsonArgs.Add([string]$entry) | Out-Null
}
$inspectJsonArgs.Add("-AsJson") | Out-Null

$reportArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $reportArgs -Name "-Summary" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $reportArgs -Name "-InspectJson" -Value $inspectJsonPathResolved
Add-ScriptArgument -Arguments $reportArgs -Name "-OutputPath" -Value $reportMarkdownPathResolved
Add-ScriptArgument -Arguments $reportArgs -Name "-Title" -Value $resolvedReportTitle
Add-ScriptArgument -Arguments $reportArgs -Name "-Top" -Value (Format-Number -Value $Top)
if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathResolved)) {
    Add-ScriptArgument -Arguments $reportArgs -Name "-BaselineSummary" -Value $baselineSummaryPathResolved
}

$checkArgs = [System.Collections.Generic.List[string]]::new()
Add-ScriptArgument -Arguments $checkArgs -Name "-Summary" -Value $summaryPathResolved
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxFailures" -Value (Format-Number -Value $MaxFailures)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxOtherResults" -Value (Format-Number -Value $MaxOtherResults)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxTotalElapsedMs" -Value (Format-Number -Value $MaxTotalElapsedMs)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxAverageElapsedMs" -Value (Format-Number -Value $MaxAverageElapsedMs)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxMaxElapsedMs" -Value (Format-Number -Value $MaxMaxElapsedMs)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxRegressionCount" -Value (Format-Number -Value $MaxRegressionCount)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxRegressionMs" -Value (Format-Number -Value $MaxRegressionMs)
Add-ScriptArgument -Arguments $checkArgs -Name "-MaxRegressionPct" -Value (Format-Number -Value $MaxRegressionPct)
Add-ProfileCheckModeArgument -Arguments $checkArgs -BundleProfile $Profile
if (-not [string]::IsNullOrWhiteSpace($baselineSummaryPathResolved)) {
    Add-ScriptArgument -Arguments $checkArgs -Name "-BaselineSummary" -Value $baselineSummaryPathResolved
}
if ($AllowAddedExamples) {
    $checkArgs.Add("-AllowAddedExamples") | Out-Null
}
if ($AllowRemovedExamples) {
    $checkArgs.Add("-AllowRemovedExamples") | Out-Null
}

Push-Location $repoRoot
try {
    try {
        $smokeExitCode = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $smokeScript `
            -ArgumentList $smokeArgs.ToArray() `
            -LogPath $smokeLogPathResolved `
            -FailureMessage ("minimal kernel host smoke {0} run failed" -f $Profile) `
            -AllowFailure

        if (-not (Test-Path $summaryPathResolved)) {
            if ($smokeExitCode -ne 0) {
                throw ("host smoke exited {0} and did not produce summary: {1}" -f $smokeExitCode, $summaryPathResolved)
            }

            throw "host smoke summary not found: $summaryPathResolved"
        }

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $inspectScript `
            -ArgumentList $inspectArgs.ToArray() `
            -LogPath $inspectTextPathResolved `
            -FailureMessage "host smoke inspect text generation failed"

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $inspectScript `
            -ArgumentList $inspectJsonArgs.ToArray() `
            -LogPath $inspectJsonPathResolved `
            -FailureMessage "host smoke inspect json generation failed"

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $reportScript `
            -ArgumentList $reportArgs.ToArray() `
            -LogPath $reportLogPathResolved `
            -FailureMessage "host smoke markdown report generation failed"

        $null = Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $checkScript `
            -ArgumentList $checkArgs.ToArray() `
            -LogPath $checkTextPathResolved `
            -FailureMessage "host smoke summary gate failed"
    } finally {
        Write-Host "==> bundle"
        Write-Host ("profile={0}" -f $Profile)
        Write-Host ("output_root={0}" -f $outputRootPath)
        Write-Host ("summary={0}" -f $summaryPathResolved)
        Write-Host ("smoke_log={0}" -f $smokeLogPathResolved)
        Write-Host ("inspect_text={0}" -f $inspectTextPathResolved)
        Write-Host ("inspect_json={0}" -f $inspectJsonPathResolved)
        Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
        Write-Host ("check_text={0}" -f $checkTextPathResolved)
    }
} finally {
    Pop-Location
}
