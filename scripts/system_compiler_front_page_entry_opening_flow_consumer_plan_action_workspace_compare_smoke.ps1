param(
    [string]$ActionWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-compare-smoke",
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

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Message
    )

    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace($Path)) `
        -Message ("{0}: missing path" -f $Message)
    Assert-Condition `
        -Condition (Test-Path -LiteralPath $Path) `
        -Message ("{0}: path not found: {1}" -f $Message, $Path)
}

function Assert-SurfacePaths {
    param(
        $Surface,
        [string]$Message
    )

    Assert-PathExists -Path ([string]$Surface.summary_path) -Message ("{0}.summary_path" -f $Message)
    Assert-PathExists -Path ([string]$Surface.report_markdown_path) -Message ("{0}.report_markdown_path" -f $Message)
    Assert-PathExists -Path ([string]$Surface.check_text_path) -Message ("{0}.check_text_path" -f $Message)
}

function Assert-CompareWitnessShape {
    param(
        $CompareSummary,
        [string]$CompareSummaryPath,
        [string]$CaseName
    )

    Assert-SurfacePaths -Surface $CompareSummary.front_page -Message ("case '{0}' front_page" -f $CaseName)
    Assert-Condition `
        -Condition ([string]$CompareSummary.front_page.summary_path -eq [string]$CompareSummary.artifact_context.compare_summary_path) `
        -Message ("case '{0}' front_page summary path does not match artifact context" -f $CaseName)
    Assert-Condition `
        -Condition ([string]$CompareSummary.front_page.summary_path -eq [string]$CompareSummaryPath) `
        -Message ("case '{0}' front_page summary path does not match smoke output" -f $CaseName)

    $surfaces = @($CompareSummary.front_page.supporting_surfaces)
    Assert-Condition `
        -Condition ($surfaces.Count -eq 2) `
        -Message ("case '{0}' expected 2 supporting surfaces but got {1}" -f $CaseName, $surfaces.Count)

    $surfaceById = @{}
    foreach ($surface in $surfaces) {
        $surfaceById[[string]$surface.id] = $surface
        Assert-Condition `
            -Condition ([string]$surface.summary_schema -eq "system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0") `
            -Message ("case '{0}' supporting surface '{1}' has unexpected schema" -f $CaseName, [string]$surface.id)
        Assert-SurfacePaths -Surface $surface -Message ("case '{0}' supporting surface '{1}'" -f $CaseName, [string]$surface.id)
    }

    foreach ($expectedId in @("baseline_plan_action", "candidate_plan_action")) {
        Assert-Condition `
            -Condition ($surfaceById.ContainsKey($expectedId)) `
            -Message ("case '{0}' missing supporting surface '{1}'" -f $CaseName, $expectedId)
    }

    $provenance = @($CompareSummary.action_provenance)
    Assert-Condition `
        -Condition ($provenance.Count -eq 2) `
        -Message ("case '{0}' expected 2 action provenance entries but got {1}" -f $CaseName, $provenance.Count)

    $provenanceById = @{}
    foreach ($entry in $provenance) {
        $provenanceById[[string]$entry.id] = $entry
        Assert-PathExists -Path ([string]$entry.source_summary_path) -Message ("case '{0}' provenance '{1}'.source_summary_path" -f $CaseName, [string]$entry.id)
        Assert-PathExists -Path ([string]$entry.source_report_markdown_path) -Message ("case '{0}' provenance '{1}'.source_report_markdown_path" -f $CaseName, [string]$entry.id)
        Assert-PathExists -Path ([string]$entry.source_check_text_path) -Message ("case '{0}' provenance '{1}'.source_check_text_path" -f $CaseName, [string]$entry.id)
    }

    foreach ($expectedId in @("baseline_plan_action", "candidate_plan_action")) {
        Assert-Condition `
            -Condition ($provenanceById.ContainsKey($expectedId)) `
            -Message ("case '{0}' missing provenance '{1}'" -f $CaseName, $expectedId)
        Assert-Condition `
            -Condition ([string]$surfaceById[$expectedId].summary_path -eq [string]$provenanceById[$expectedId].source_summary_path) `
            -Message ("case '{0}' supporting surface and provenance summary path diverged for '{1}'" -f $CaseName, $expectedId)
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
$compareWorkspaceScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_consumer_plan_action_workspace.ps1"
foreach ($requiredPath in @($actionWorkspaceSmokeScript, $compareWorkspaceScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineActionWorkspaceRoot = Join-Path $actionWorkspaceRootPath "cold-default"
    $baselineActionPath = Join-Path $baselineActionWorkspaceRoot "action\front-page.entry-opening-flow.consumer.plan-action.summary.json"
    $baselinePlanWorkspaceRoot = Join-Path $baselineActionWorkspaceRoot "plan-ws"
    if ((-not $Clean) -and (Test-Path -LiteralPath $baselineActionPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE-SMOKE] bootstrap=reuse-existing"
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

    $cases = @(
        [ordered]@{
            Name = "action-workspace-self-standing"
            BaselineActionWorkspaceRoot = $baselineActionWorkspaceRoot
            CandidateActionWorkspaceRoot = $baselineActionWorkspaceRoot
            CandidatePlanWorkspaceRoot = ""
            CandidateActionKind = ""
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedActionChanged = $false
            ExpectedOpenerChanged = $false
            ExpectedTargetChanged = $false
            ExpectedReasonChanged = $false
        },
        [ordered]@{
            Name = "action-workspace-default-to-compare-neighbor"
            BaselineActionWorkspaceRoot = $baselineActionWorkspaceRoot
            CandidateActionWorkspaceRoot = ""
            CandidatePlanWorkspaceRoot = $baselinePlanWorkspaceRoot
            CandidateActionKind = "compare-neighbor"
            ExpectedVerdict = "drifted"
            ExpectedChangedFields = 30
            ExpectedActionChanged = $true
            ExpectedOpenerChanged = $true
            ExpectedTargetChanged = $true
            ExpectedReasonChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $compareWorkspaceScript,
            "-BaselineActionWorkspaceRoot",
            $case.BaselineActionWorkspaceRoot,
            "-OutputRoot",
            $caseOutputRoot,
            "-PythonExe",
            $resolvedPythonExe
        )
        if (-not [string]::IsNullOrWhiteSpace([string]$case.CandidateActionWorkspaceRoot)) {
            $arguments += @("-CandidateActionWorkspaceRoot", [string]$case.CandidateActionWorkspaceRoot)
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.CandidatePlanWorkspaceRoot)) {
            $arguments += @("-CandidatePlanWorkspaceRoot", [string]$case.CandidatePlanWorkspaceRoot)
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.CandidateActionKind)) {
            $arguments += @("-CandidateActionKind", [string]$case.CandidateActionKind)
        }

        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opening-flow consumer plan action workspace compare failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opening-flow.consumer.plan-action.compare.summary.json"
        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-CompareWitnessShape -CompareSummary $compareSummary -CompareSummaryPath $compareSummaryPath -CaseName $case.Name
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
            -Condition ([bool]$compareSummary.action_regression_surface.opener_changed -eq [bool]$case.ExpectedOpenerChanged) `
            -Message ("case '{0}' opener changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.target_changed -eq [bool]$case.ExpectedTargetChanged) `
            -Message ("case '{0}' target changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.reason_changed -eq [bool]$case.ExpectedReasonChanged) `
            -Message ("case '{0}' reason changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.projection_summary_changed -eq [bool]$case.ExpectedReasonChanged) `
            -Message ("case '{0}' projection summary changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$compareSummary.action_regression_surface.projection_questions_changed -eq [bool]$case.ExpectedReasonChanged) `
            -Message ("case '{0}' projection questions changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} action_changed={3} opener_changed={4} target_changed={5} projection_summary_changed={6} projection_questions_changed={7} reason_changed={8}" -f
            $case.Name,
            [string]$compareSummary.action_verdict,
            [int]$compareSummary.change_summary.changed_field_count,
            [bool]$compareSummary.action_regression_surface.action_id_changed,
            [bool]$compareSummary.action_regression_surface.opener_changed,
            [bool]$compareSummary.action_regression_surface.target_changed,
            [bool]$compareSummary.action_regression_surface.projection_summary_changed,
            [bool]$compareSummary.action_regression_surface.projection_questions_changed,
            [bool]$compareSummary.action_regression_surface.reason_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-PLAN-ACTION-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
