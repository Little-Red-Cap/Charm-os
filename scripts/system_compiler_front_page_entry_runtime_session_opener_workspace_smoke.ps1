param(
    [string]$RuntimeSessionOpenEventWitnessCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-open-event-witness-compare-smoke",
    [string]$RuntimeSessionOpenerCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-opener-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-workspace-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
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

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Assert-CleanPath {
    param(
        [string]$Path,
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $RootPath
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $rootPrefix = $resolvedRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if ($resolvedPath.Equals($resolvedRoot, $comparison)) {
        throw "refusing to clean repo root: $resolvedPath"
    }
    if (-not $resolvedPath.StartsWith($rootPrefix, $comparison)) {
        throw "refusing to clean outside repo root: $resolvedPath"
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
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

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$FailureMessage
    )

    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        & $Executable @ArgumentList
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-NoRawRuntimeSessionLeak {
    param(
        [string[]]$Paths,
        [string]$CaseName
    )

    $leakedPaths = @(
        $Paths |
            Where-Object {
                $pathText = [string]$_
                -not [string]::IsNullOrWhiteSpace($pathText) -and
                $pathText.EndsWith("kernel_runtime_session.summary.json", [System.StringComparison]::OrdinalIgnoreCase)
            }
    )

    Assert-Condition `
        -Condition ($leakedPaths.Count -eq 0) `
        -Message ("case '{0}' should not reopen raw kernel_runtime_session summaries" -f $CaseName)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionOpenEventWitnessCompareRootPath = Resolve-FullPath -Path $RuntimeSessionOpenEventWitnessCompareRoot
$runtimeSessionOpenerCompareRootPath = Resolve-FullPath -Path $RuntimeSessionOpenerCompareRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Assert-CleanPath -Path $outputRootPath -RootPath $repoRoot
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$runtimeSessionOpenEventWitnessCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opener_open_event_witness_compare_smoke.ps1"
$runtimeSessionOpenerCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opener_opener_compare_smoke.ps1"
$workspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener_workspace.ps1"
foreach ($requiredPath in @(
    $runtimeSessionOpenEventWitnessCompareSmokeScript,
    $runtimeSessionOpenerCompareSmokeScript,
    $workspaceExportScript
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $neutralLandingRoot = Join-Path $runtimeSessionOpenEventWitnessCompareRootPath "_runtime-session-neutral-drift-open-event-witness-compare_landing"
    $collapsedLandingRoot = Join-Path $runtimeSessionOpenEventWitnessCompareRootPath "_runtime-session-collapsed-open-event-witness-compare_landing"
    $openerCompareLandingRoot = Join-Path $runtimeSessionOpenerCompareRootPath "_synthetic_landing"

    $neutralLandingSummaryPath = Join-Path $neutralLandingRoot "front-page.entry-landing.summary.json"
    $collapsedLandingSummaryPath = Join-Path $collapsedLandingRoot "front-page.entry-landing.summary.json"
    if ($Clean -or -not ((Test-Path -LiteralPath $neutralLandingSummaryPath) -and (Test-Path -LiteralPath $collapsedLandingSummaryPath))) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $runtimeSessionOpenEventWitnessCompareSmokeScript,
                "-OutputRoot",
                $runtimeSessionOpenEventWitnessCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session open-event witness compare smoke bootstrap failed"
    } else {
        Write-Host "[RUNTIME-SESSION-OPENER-WORKSPACE-SMOKE] witness_compare_bootstrap=reuse-existing"
    }

    $openerCompareLandingSummaryPath = Join-Path $openerCompareLandingRoot "front-page.entry-landing.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $openerCompareLandingSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $runtimeSessionOpenerCompareSmokeScript,
                "-OutputRoot",
                $runtimeSessionOpenerCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session opener compare smoke bootstrap failed"
    } else {
        Write-Host "[RUNTIME-SESSION-OPENER-WORKSPACE-SMOKE] opener_compare_bootstrap=reuse-existing"
    }

    foreach ($requiredSummaryPath in @(
        $neutralLandingSummaryPath,
        $collapsedLandingSummaryPath,
        $openerCompareLandingSummaryPath
    )) {
        if (-not (Test-Path -LiteralPath $requiredSummaryPath)) {
            throw "missing landing fixture: $requiredSummaryPath"
        }
    }

    $cases = @(
        [ordered]@{
            Name = "runtime-session-neutral-drift-open-event-witness-compare-workspace"
            LandingWorkspaceRoot = $neutralLandingRoot
            ExpectedTabId = "open_event_witness_compare"
            ExpectedProjectionKind = "open_event_witness_compare_overview"
            ExpectedHeadlinePattern = "*verdict=drifted*"
        },
        [ordered]@{
            Name = "runtime-session-collapsed-open-event-witness-compare-workspace"
            LandingWorkspaceRoot = $collapsedLandingRoot
            ExpectedTabId = "open_event_witness_compare"
            ExpectedProjectionKind = "open_event_witness_compare_overview"
            ExpectedHeadlinePattern = "*verdict=collapsed*"
        },
        [ordered]@{
            Name = "runtime-session-opener-compare-workspace"
            LandingWorkspaceRoot = $openerCompareLandingRoot
            ExpectedTabId = "opener_compare"
            ExpectedProjectionKind = "opener_compare_overview"
            ExpectedHeadlinePattern = "*verdict=drifted*"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $workspaceExportScript,
                "-LandingWorkspaceRoot",
                [string]$case.LandingWorkspaceRoot,
                "-OutputRoot",
                $caseOutputRoot,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage ("runtime-session opener workspace export failed for case '{0}'" -f $case.Name)

        $openerSummaryPath = Join-Path $caseOutputRoot "opener\front-page.entry-opener.summary.json"
        $summary = Load-JsonObject -Path $openerSummaryPath
        $projection = $summary.opened_projection
        $summaryLines = @($projection.summary_lines) | ForEach-Object { [string]$_ }
        $questionLines = @($projection.question_lines) | ForEach-Object { [string]$_ }
        $evidencePaths = @($projection.evidence_paths) | ForEach-Object { [string]$_ }
        $supportingPaths = @($projection.supporting_summary_paths) | ForEach-Object { [string]$_ }
        $allProjectionPaths = @($evidencePaths + $supportingPaths)

        Assert-Condition `
            -Condition ([string]$summary.open_action.status -eq "ready") `
            -Message ("case '{0}' expected open action ready but got '{1}'" -f $case.Name, [string]$summary.open_action.status)
        Assert-Condition `
            -Condition ([string]$summary.open_action.selected_tab_id -eq [string]$case.ExpectedTabId) `
            -Message ("case '{0}' expected selected tab '{1}' but got '{2}'" -f $case.Name, $case.ExpectedTabId, [string]$summary.open_action.selected_tab_id)
        Assert-Condition `
            -Condition ([string]$summary.open_action.query_kind -eq "default_overview") `
            -Message ("case '{0}' expected query kind default_overview but got '{1}'" -f $case.Name, [string]$summary.open_action.query_kind)
        Assert-Condition `
            -Condition ([bool]$summary.compare_context.available -eq $false) `
            -Message ("case '{0}' should not grow compare context at workspace opener layer" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.inspector_invocation.ready -eq $false) `
            -Message ("case '{0}' expected blocked inspector invocation for consumer-side compare target" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$projection.status -eq "available") `
            -Message ("case '{0}' expected available projection but got '{1}'" -f $case.Name, [string]$projection.status)
        Assert-Condition `
            -Condition ([string]$projection.projection_kind -eq [string]$case.ExpectedProjectionKind) `
            -Message ("case '{0}' expected projection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionKind, [string]$projection.projection_kind)
        Assert-Condition `
            -Condition (([string]$projection.headline) -like [string]$case.ExpectedHeadlinePattern) `
            -Message ("case '{0}' expected headline pattern '{1}' but got '{2}'" -f $case.Name, $case.ExpectedHeadlinePattern, [string]$projection.headline)
        Assert-Condition `
            -Condition ($summaryLines.Count -gt 0) `
            -Message ("case '{0}' expected projection summary lines" -f $case.Name)
        Assert-Condition `
            -Condition ($questionLines.Count -gt 0) `
            -Message ("case '{0}' expected projection question lines" -f $case.Name)
        Assert-Condition `
            -Condition ($evidencePaths.Count -ge 2) `
            -Message ("case '{0}' expected at least two evidence paths" -f $case.Name)
        Assert-Condition `
            -Condition ($supportingPaths.Count -ge 2) `
            -Message ("case '{0}' expected at least two supporting summary paths" -f $case.Name)
        Assert-NoRawRuntimeSessionLeak -Paths $allProjectionPaths -CaseName $case.Name

        Write-Host (
            "[RUNTIME-SESSION-OPENER-WORKSPACE-SMOKE] case={0} tab={1} projection={2}/{3} headline='{4}' evidence_paths={5} supporting_paths={6} inspector_ready={7}" -f
            $case.Name,
            [string]$summary.open_action.selected_tab_id,
            [string]$projection.status,
            [string]$projection.projection_kind,
            [string]$projection.headline,
            [int]$evidencePaths.Count,
            [int]$supportingPaths.Count,
            [bool]$summary.inspector_invocation.ready
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENER-WORKSPACE-SMOKE] output_root={0}" -f $outputRootPath)
