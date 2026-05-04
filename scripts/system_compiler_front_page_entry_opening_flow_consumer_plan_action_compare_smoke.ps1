param(
    [string]$ActionWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare-smoke",
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
$actionWorkspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_plan_action_compare.py"
foreach ($requiredPath in @($actionWorkspaceSmokeScript, $actionWorkspaceExportScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineActionPath = Join-Path $actionWorkspaceRootPath "cold-default\action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $baselinePlanWorkspaceRoot = Join-Path $actionWorkspaceRootPath "cold-default\plan-ws"
    if (Test-Path -LiteralPath $baselineActionPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE-SMOKE] bootstrap=reuse-existing"
    } else {
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

    $candidateActionRoot = Join-Path $outputRootPath "_candidate_compare_action"
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $actionWorkspaceExportScript,
            "-PlanWorkspaceRoot",
            $baselinePlanWorkspaceRoot,
            "-OutputRoot",
            $candidateActionRoot,
            "-ActionKind",
            "compare-neighbor",
            "-PythonExe",
            $resolvedPythonExe
        ) `
        -FailureMessage "front page entry opening-flow consumer plan action candidate export failed"

    $candidateActionPath = Join-Path $candidateActionRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $cases = @(
        [ordered]@{
            Name = "action-self-standing"
            Baseline = $baselineActionPath
            Candidate = $baselineActionPath
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedActionChanged = $false
        },
        [ordered]@{
            Name = "default-to-compare-neighbor"
            Baseline = $baselineActionPath
            Candidate = $candidateActionPath
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = 26
            ExpectedActionChanged = $true
        }
    )

    foreach ($case in $cases) {
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
            -FailureMessage ("front page entry opening-flow consumer plan action compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry opening-flow consumer plan action compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.action_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.action_verdict)
        Assert-Condition `
            -Condition ([int]$compareSummary.change_summary.changed_field_count -eq [int]$case.ExpectedChangedFields) `
            -Message ("case '{0}' expected changed fields '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedFields, $compareSummary.change_summary.changed_field_count)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.action_id_changed -eq [bool]$case.ExpectedActionChanged) `
            -Message ("case '{0}' action changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.opening_reason_changed -eq [bool]$case.ExpectedActionChanged) `
            -Message ("case '{0}' opening reason changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.projection_headline_changed -eq [bool]$case.ExpectedActionChanged) `
            -Message ("case '{0}' projection headline changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE-SMOKE] case={0} verdict={1} changed={2} action_changed={3} opening_reason_changed={4} projection_headline_changed={5} opener_changed={6} target_changed={7}" -f
            $case.Name,
            [string]$compareSummary.action_verdict,
            [int]$compareSummary.change_summary.changed_field_count,
            [bool]$compareSummary.action_regression_surface.action_id_changed,
            [bool]$compareSummary.action_regression_surface.opening_reason_changed,
            [bool]$compareSummary.action_regression_surface.projection_headline_changed,
            [bool]$compareSummary.action_regression_surface.opener_changed,
            [bool]$compareSummary.action_regression_surface.target_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
