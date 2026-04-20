param(
    [string]$OutputRoot = "out/minimal-kernel-runtime-host-smoke-ci",
    [string]$SummaryPath = "",
    [string]$SmokeLogPath = "",
    [string]$InspectTextPath = "",
    [string]$InspectJsonPath = "",
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

    $Arguments.Add($Name) | Out-Null
    foreach ($value in @($Values)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$value)) {
            $Arguments.Add([string]$value) | Out-Null
        }
    }
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
        [string]$FailureMessage
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
    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$smokeLogPathResolved = Get-OutputPath -ExplicitPath $SmokeLogPath -OutputRootPath $outputRootPath -DefaultFileName "smoke.log"
$inspectTextPathResolved = Get-OutputPath -ExplicitPath $InspectTextPath -OutputRootPath $outputRootPath -DefaultFileName "inspect.txt"
$inspectJsonPathResolved = Get-OutputPath -ExplicitPath $InspectJsonPath -OutputRootPath $outputRootPath -DefaultFileName "inspect.json"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$baselineSummaryPathResolved = Resolve-FullPath -Path $BaselineSummary

$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$smokeScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_host_smoke_ci.ps1"
$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_host_smoke.ps1"
$checkScript = Join-Path $PSScriptRoot "check_minimal_kernel_runtime_host_smoke_summary.ps1"
foreach ($scriptPath in @($smokeScript, $inspectScript, $checkScript)) {
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
$checkArgs.Add("-RequireConfigureExecuted") | Out-Null
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
        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $smokeScript `
            -ArgumentList $smokeArgs.ToArray() `
            -LogPath $smokeLogPathResolved `
            -FailureMessage "minimal kernel host smoke CI run failed"

        if (-not (Test-Path $summaryPathResolved)) {
            throw "host smoke summary not found: $summaryPathResolved"
        }

        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $inspectScript `
            -ArgumentList $inspectArgs.ToArray() `
            -LogPath $inspectTextPathResolved `
            -FailureMessage "host smoke inspect text generation failed"

        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $inspectScript `
            -ArgumentList $inspectJsonArgs.ToArray() `
            -LogPath $inspectJsonPathResolved `
            -FailureMessage "host smoke inspect json generation failed"

        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $checkScript `
            -ArgumentList $checkArgs.ToArray() `
            -LogPath $checkTextPathResolved `
            -FailureMessage "host smoke summary gate failed"
    } finally {
        Write-Host "==> bundle"
        Write-Host ("output_root={0}" -f $outputRootPath)
        Write-Host ("summary={0}" -f $summaryPathResolved)
        Write-Host ("smoke_log={0}" -f $smokeLogPathResolved)
        Write-Host ("inspect_text={0}" -f $inspectTextPathResolved)
        Write-Host ("inspect_json={0}" -f $inspectJsonPathResolved)
        Write-Host ("check_text={0}" -f $checkTextPathResolved)
    }
} finally {
    Pop-Location
}
