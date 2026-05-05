param(
    [string]$RuntimeSessionOpenerWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-workspace-compare-smoke",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionOpenerWorkspaceRootPath = Resolve-FullPath -Path $RuntimeSessionOpenerWorkspaceRoot
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

$workspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opener_workspace_smoke.ps1"
$workspaceCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opener_workspace.ps1"
foreach ($requiredPath in @($workspaceSmokeScript, $workspaceCompareScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $neutralWorkspaceRoot = Join-Path $runtimeSessionOpenerWorkspaceRootPath "runtime-session-neutral-drift-open-event-witness-compare-workspace"
    $collapsedWorkspaceRoot = Join-Path $runtimeSessionOpenerWorkspaceRootPath "runtime-session-collapsed-open-event-witness-compare-workspace"
    $openerCompareWorkspaceRoot = Join-Path $runtimeSessionOpenerWorkspaceRootPath "runtime-session-opener-compare-workspace"

    $neutralWorkspaceSummaryPath = Join-Path $neutralWorkspaceRoot "opener\front-page.entry-opener.summary.json"
    $collapsedWorkspaceSummaryPath = Join-Path $collapsedWorkspaceRoot "opener\front-page.entry-opener.summary.json"
    $openerCompareWorkspaceSummaryPath = Join-Path $openerCompareWorkspaceRoot "opener\front-page.entry-opener.summary.json"
    if ($Clean -or -not ((Test-Path -LiteralPath $neutralWorkspaceSummaryPath) -and (Test-Path -LiteralPath $collapsedWorkspaceSummaryPath) -and (Test-Path -LiteralPath $openerCompareWorkspaceSummaryPath))) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $workspaceSmokeScript,
                "-OutputRoot",
                $runtimeSessionOpenerWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session opener workspace smoke bootstrap failed"
    } else {
        Write-Host "[RUNTIME-SESSION-OPENER-WORKSPACE-COMPARE-SMOKE] workspace_bootstrap=reuse-existing"
    }

    foreach ($requiredSummaryPath in @(
        $neutralWorkspaceSummaryPath,
        $collapsedWorkspaceSummaryPath,
        $openerCompareWorkspaceSummaryPath
    )) {
        if (-not (Test-Path -LiteralPath $requiredSummaryPath)) {
            throw "missing opener workspace summary fixture: $requiredSummaryPath"
        }
    }

    $cases = @(
        [ordered]@{
            Name = "workspace-open-event-witness-compare-self-standing"
            BaselineWorkspaceRoot = $neutralWorkspaceRoot
            CandidateWorkspaceRoot = $neutralWorkspaceRoot
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedCompareChanged = $false
            ExpectedProjectionChanged = $false
            ExpectedImprovement = $false
            ExpectedRegression = $false
            ExpectedProjectionKind = "open_event_witness_compare_overview"
            ExpectedProjectionHeadline = ""
        },
        [ordered]@{
            Name = "workspace-open-event-witness-compare-neutral-to-collapsed"
            BaselineWorkspaceRoot = $neutralWorkspaceRoot
            CandidateWorkspaceRoot = $collapsedWorkspaceRoot
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = -1
            ExpectedCompareChanged = $false
            ExpectedProjectionChanged = $true
            ExpectedImprovement = $false
            ExpectedRegression = $false
            ExpectedProjectionKind = "open_event_witness_compare_overview"
            ExpectedProjectionHeadline = "open_event_witness_compare verdict=collapsed changed=20"
        },
        [ordered]@{
            Name = "workspace-opener-compare-self-standing"
            BaselineWorkspaceRoot = $openerCompareWorkspaceRoot
            CandidateWorkspaceRoot = $openerCompareWorkspaceRoot
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedCompareChanged = $false
            ExpectedProjectionChanged = $false
            ExpectedImprovement = $false
            ExpectedRegression = $false
            ExpectedProjectionKind = "opener_compare_overview"
            ExpectedProjectionHeadline = ""
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
                $workspaceCompareScript,
                "-BaselineOpenerWorkspaceRoot",
                [string]$case.BaselineWorkspaceRoot,
                "-CandidateOpenerWorkspaceRoot",
                [string]$case.CandidateWorkspaceRoot,
                "-OutputRoot",
                $caseOutputRoot,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage ("runtime-session opener workspace compare failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opener.compare.summary.json"
        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.opener_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, [string]$summary.opener_verdict)
        if ([int]$case.ExpectedChangedFields -ge 0) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, [int]$summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.opener_changes.compare_context_changed -eq [bool]$case.ExpectedCompareChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_changes.projection_changed -eq [bool]$case.ExpectedProjectionChanged) `
            -Message ("case '{0}' projection changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_improvement_surface.changed -eq [bool]$case.ExpectedImprovement) `
            -Message ("case '{0}' improvement surface expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.opener_regression_surface.changed -eq [bool]$case.ExpectedRegression) `
            -Message ("case '{0}' regression surface expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.opener_status.candidate_projection_kind -eq [string]$case.ExpectedProjectionKind) `
            -Message ("case '{0}' expected candidate projection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionKind, [string]$summary.opener_status.candidate_projection_kind)

        $projectionHeadlineChange = @($summary.field_changes | Where-Object { [string]$_.field -eq "opened_projection.headline" }) | Select-Object -First 1
        if ([string]::IsNullOrWhiteSpace([string]$case.ExpectedProjectionHeadline)) {
            Assert-Condition `
                -Condition ($null -eq $projectionHeadlineChange) `
                -Message ("case '{0}' should not drift projection headline" -f $case.Name)
        } else {
            Assert-Condition `
                -Condition ($null -ne $projectionHeadlineChange) `
                -Message ("case '{0}' expected opened_projection.headline to drift" -f $case.Name)
            Assert-Condition `
                -Condition ([string]$projectionHeadlineChange.candidate_value -eq [string]$case.ExpectedProjectionHeadline) `
                -Message ("case '{0}' expected candidate projection headline '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionHeadline, [string]$projectionHeadlineChange.candidate_value)
        }

        Write-Host (
            "[RUNTIME-SESSION-OPENER-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} compare_changed={3} projection_changed={4} improved={5} regressed={6} projection_kind={7}" -f
            $case.Name,
            [string]$summary.opener_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.opener_changes.compare_context_changed,
            [bool]$summary.opener_changes.projection_changed,
            [bool]$summary.opener_improvement_surface.changed,
            [bool]$summary.opener_regression_surface.changed,
            [string]$summary.opener_status.candidate_projection_kind
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENER-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
