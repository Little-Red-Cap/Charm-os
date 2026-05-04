param(
    [string]$PlanRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-compare-smoke",
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

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }

    $json = $Value | ConvertTo-Json -Depth 64
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [System.Text.Encoding]::UTF8)
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

function New-SyntheticPlanDrift {
    param(
        [string]$SourcePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourcePath
    $removedActionId = [string]$summary.execution_plan.compare_action.action_id
    $removedEntryName = [string]$summary.execution_plan.compare_action.entry_name
    $keptActions = @()
    foreach ($action in @($summary.execution_plan.action_entries)) {
        if ([string]$action.action_id -ne $removedActionId) {
            $keptActions += $action
        }
    }

    $keptNextActions = @()
    foreach ($action in @($summary.execution_plan.next_actions)) {
        if ([string]$action.action_id -ne $removedActionId) {
            $keptNextActions += $action
        }
    }

    $rank = 0
    foreach ($action in @($keptActions)) {
        $action.rank = $rank
        $rank += 1
    }

    $summary.execution_plan.compare_action = [ordered]@{}
    $summary.execution_plan.action_entries = @($keptActions)
    $summary.execution_plan.next_actions = @($keptNextActions)
    $summary.planning_surface.planned_action_ids = @($keptActions | ForEach-Object { [string]$_.action_id })
    $summary.planning_surface.planned_entry_names = @($keptActions | ForEach-Object { [string]$_.entry_name })
    $summary.planning_surface.compare_action_id = ""
    $summary.planner_status.planned_action_count = @($keptActions).Count
    $summary.planner_status.compare_action_name = ""
    $summary.planner_status.next_action_count = @($keptNextActions).Count
    $summary.planner_status.omitted_entry_count = [int]$summary.planner_status.omitted_entry_count + 1
    $summary.planning_surface.omitted_entry_names += $removedEntryName
    $summary.artifact_context.output_root = (Split-Path -Parent $OutputPath)
    $summary.artifact_context.consumer_plan_summary_path = $OutputPath
    $summary.front_page.summary_path = $OutputPath

    Write-JsonFile -Path $OutputPath -Value $summary
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$planRootPath = Resolve-FullPath -Path $PlanRoot
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

$planSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_consumer_plan_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_compare.py"
foreach ($requiredPath in @($planSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $planSummaryPath = Join-Path $planRootPath "front-page.entry-opening-flow.consumer.plan.summary.json"
    if (Test-Path -LiteralPath $planSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $planSmokeScript,
                "-OutputRoot",
                $planRootPath
            ) `
            -FailureMessage "front page entry opening-flow consumer plan smoke bootstrap failed"
    }

    $syntheticRoot = Join-Path $outputRootPath "_synthetic"
    Ensure-Directory -Path $syntheticRoot
    $driftedPlanSummaryPath = Join-Path $syntheticRoot "front-page.entry-opening-flow.consumer.plan.drifted.summary.json"
    New-SyntheticPlanDrift -SourcePath $planSummaryPath -OutputPath $driftedPlanSummaryPath

    $cases = @(
        [ordered]@{
            Name = "self-standing"
            Baseline = $planSummaryPath
            Candidate = $planSummaryPath
            ExpectedVerdict = "standing"
            ExpectedChangedActions = 0
            ExpectedRemoved = @()
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $false
        },
        [ordered]@{
            Name = "removed-compare-action"
            Baseline = $planSummaryPath
            Candidate = $driftedPlanSummaryPath
            ExpectedVerdict = "drifted"
            ExpectedChangedActions = 3
            ExpectedRemoved = @("open-compare-neighbor")
            ExpectedDefaultChanged = $false
            ExpectedCompareChanged = $true
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path -LiteralPath $requiredSummary)) {
                throw "plan summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                $case.Baseline,
                "--candidate",
                $case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page entry opening-flow consumer plan compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.consumer.plan.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening-flow consumer plan compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.plan_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.plan_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.plan_action_summary.changed_action_count -eq [int]$case.ExpectedChangedActions) `
            -Message ("case '{0}' expected changed actions '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedActions, $compareSummary.plan_action_summary.changed_action_count)
        Assert-Condition `
            -Condition ([bool]$compareSummary.plan_regression_surface.default_action_changed -eq [bool]$case.ExpectedDefaultChanged) `
            -Message ("case '{0}' default changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.plan_regression_surface.compare_action_changed -eq [bool]$case.ExpectedCompareChanged) `
            -Message ("case '{0}' compare changed expectation mismatch" -f $case.Name)

        $removedActions = @([string[]]$compareSummary.plan_changes.ordered_action_id_changes.removed)
        foreach ($actionId in @($case.ExpectedRemoved)) {
            Assert-Condition `
                -Condition ($removedActions -contains [string]$actionId) `
                -Message ("case '{0}' expected removed plan action '{1}'" -f $case.Name, $actionId)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE-SMOKE] case={0} verdict={1} changed={2} added={3} removed={4} default_changed={5} compare_changed={6}" -f
            $case.Name,
            [string]$compareSummary.plan_verdict,
            [int]$compareSummary.plan_action_summary.changed_action_count,
            [int]$compareSummary.plan_action_summary.added_action_count,
            [int]$compareSummary.plan_action_summary.removed_action_count,
            [bool]$compareSummary.plan_regression_surface.default_action_changed,
            [bool]$compareSummary.plan_regression_surface.compare_action_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
