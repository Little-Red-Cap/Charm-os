param(
    [string]$ActionWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
    [string]$ActionCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare-smoke",
    [string]$OpenEventWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-compare-smoke",
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

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
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
$actionWorkspaceRootPath = Resolve-FullPath -Path $ActionWorkspaceRoot
$actionCompareRootPath = Resolve-FullPath -Path $ActionCompareRoot
$openEventWorkspaceRootPath = Resolve-FullPath -Path $OpenEventWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$actionWorkspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace_smoke.ps1"
$actionCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare_smoke.ps1"
$openEventWorkspaceSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_workspace_smoke.ps1"
$workspaceCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1"
foreach ($requiredPath in @($actionWorkspaceSmokeScript, $actionCompareSmokeScript, $openEventWorkspaceSmokeScript, $workspaceCompareScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineEventWorkspaceRoot = Join-Path $openEventWorkspaceRootPath "from-action-summary"
    $baselineEventPath = Join-Path $baselineEventWorkspaceRoot "open-event\front-page.entry-opening-flow.open-event.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $baselineEventPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE-SMOKE] open_event_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventWorkspaceSmokeScript,
                "-ActionWorkspaceRoot",
                $actionWorkspaceRootPath,
                "-OutputRoot",
                $openEventWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event workspace smoke bootstrap failed"
    }

    $baselineActionPath = Join-Path $actionWorkspaceRootPath "cold-default\action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    if (-not (Test-Path -LiteralPath $baselineActionPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionWorkspaceSmokeScript,
                "-OutputRoot",
                $actionWorkspaceRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action workspace smoke bootstrap failed"
    }

    $driftComparePath = Join-Path $actionCompareRootPath "default-to-compare-neighbor\front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
    if (-not (Test-Path -LiteralPath $driftComparePath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $actionCompareSmokeScript,
                "-ActionWorkspaceRoot",
                $actionWorkspaceRootPath,
                "-OutputRoot",
                $actionCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow consumer plan action compare smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "workspace-self-standing"
            Arguments = @(
                "-BaselineOpenEventWorkspaceRoot",
                $baselineEventWorkspaceRoot,
                "-CandidateOpenEventWorkspaceRoot",
                $baselineEventWorkspaceRoot
            )
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedStatusChanged = $false
            ExpectedCompareContextChanged = $false
            ExpectedWitnessChanged = $false
        },
        [ordered]@{
            Name = "workspace-action-summary-to-drift-context"
            Arguments = @(
                "-BaselineOpenEventWorkspaceRoot",
                $baselineEventWorkspaceRoot,
                "-CandidateActionSummaryPath",
                $baselineActionPath,
                "-CandidateActionCompareSummaryPath",
                $driftComparePath
            )
            ExpectedVerdict = "drifted"
            ExpectedStatusChanged = $true
            ExpectedCompareContextChanged = $true
            ExpectedWitnessChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceCompareScript,
            "-OutputRoot",
            $caseOutputRoot,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) + @($case.Arguments)

        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opening-flow open-event workspace compare failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opening-flow.open-event.compare.summary.json"
        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.event_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.event_verdict)
        if ($case.Contains("ExpectedChangedFields")) {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
                -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $summary.change_summary.changed_field_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.change_summary.changed_field_count -gt 0) `
                -Message ("case '{0}' expected positive changed field count" -f $case.Name)
        }
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.event_status_changed -eq [bool]$case.ExpectedStatusChanged) `
            -Message ("case '{0}' event status changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.compare_context_changed -eq [bool]$case.ExpectedCompareContextChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.event_regression_surface.witness_set_changed -eq [bool]$case.ExpectedWitnessChanged) `
            -Message ("case '{0}' witness set changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} status_changed={3} compare_changed={4} witness_changed={5}" -f
            $case.Name,
            [string]$summary.event_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.event_regression_surface.event_status_changed,
            [bool]$summary.event_regression_surface.compare_context_changed,
            [bool]$summary.event_regression_surface.witness_set_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
