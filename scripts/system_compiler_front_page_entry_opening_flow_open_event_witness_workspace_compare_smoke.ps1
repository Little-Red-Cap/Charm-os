param(
    [string]$OpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke",
    [string]$OpenEventRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-workspace-compare-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "front_page_entry_opening_flow_harness.ps1")

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openEventWitnessRootPath = Resolve-FullPath -Path $OpenEventWitnessRoot
$openEventRootPath = Resolve-FullPath -Path $OpenEventRoot
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

$openEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1"
$workspaceCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1"
foreach ($requiredPath in @($openEventWitnessSmokeScript, $workspaceCompareScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $baselineWitnessPath = Join-Path $openEventWitnessRootPath "default-no-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    $candidateWitnessPath = Join-Path $openEventWitnessRootPath "default-with-drift-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $baselineWitnessPath) -and (Test-Path -LiteralPath $candidateWitnessPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] witness_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openEventWitnessSmokeScript,
                "-OutputRoot",
                $openEventWitnessRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening-flow open-event witness smoke bootstrap failed"
    }

    $fixtureBaselineOpenEventPath = Join-Path $openEventWitnessRootPath "_fixture-open-events\default-no-compare\front-page.entry-opening-flow.open-event.summary.json"
    $fixtureCandidateOpenEventPath = Join-Path $openEventWitnessRootPath "_fixture-open-events\default-with-drift-compare\front-page.entry-opening-flow.open-event.summary.json"
    $smokeBaselineOpenEventPath = Join-Path $openEventRootPath "default-no-compare\front-page.entry-opening-flow.open-event.summary.json"
    $smokeCandidateOpenEventPath = Join-Path $openEventRootPath "default-with-drift-compare\front-page.entry-opening-flow.open-event.summary.json"
    $baselineOpenEventPath = if (Test-Path -LiteralPath $fixtureBaselineOpenEventPath) {
        $fixtureBaselineOpenEventPath
    } else {
        $smokeBaselineOpenEventPath
    }
    $candidateOpenEventPath = if (Test-Path -LiteralPath $fixtureCandidateOpenEventPath) {
        $fixtureCandidateOpenEventPath
    } else {
        $smokeCandidateOpenEventPath
    }
    $usingFixtureOpenEvents = (Test-Path -LiteralPath $fixtureBaselineOpenEventPath) -and (Test-Path -LiteralPath $fixtureCandidateOpenEventPath)
    foreach ($requiredPath in @($baselineOpenEventPath, $candidateOpenEventPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "missing open event summary for witness workspace compare: $requiredPath"
        }
    }

    $cases = @(
        [ordered]@{
            Name = "workspace-witness-summary-self-standing"
            Arguments = @(
                "-BaselineOpenEventWitnessSummaryPath",
                $baselineWitnessPath,
                "-CandidateOpenEventWitnessSummaryPath",
                $baselineWitnessPath
            )
            ExpectedVerdict = "standing"
            ExpectedChangedFields = 0
            ExpectedEventIdentityChanged = $false
            ExpectedCompareContextChanged = $false
            ExpectedEvidenceChanged = $false
            ExpectedExplanationChanged = $false
        },
        [ordered]@{
            Name = "workspace-open-event-summary-to-drift-witness"
            Arguments = @(
                "-BaselineOpenEventSummaryPath",
                $baselineOpenEventPath,
                "-CandidateOpenEventSummaryPath",
                $candidateOpenEventPath
            )
            ExpectedVerdict = "drifted"
            ExpectedEventIdentityChanged = $usingFixtureOpenEvents
            ExpectedCompareContextChanged = $true
            ExpectedEvidenceChanged = $true
            ExpectedExplanationChanged = $true
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
            -FailureMessage ("front page entry opening-flow open-event witness workspace compare failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "compare\front-page.entry-opening-flow.open-event.witness.compare.summary.json"
        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.witness_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.witness_verdict)
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
            -Condition ([bool]$summary.witness_regression_surface.event_identity_changed -eq [bool]$case.ExpectedEventIdentityChanged) `
            -Message ("case '{0}' event identity changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.compare_context_changed -eq [bool]$case.ExpectedCompareContextChanged) `
            -Message ("case '{0}' compare context changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.evidence_refs_changed -eq [bool]$case.ExpectedEvidenceChanged) `
            -Message ("case '{0}' evidence refs changed expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.witness_regression_surface.explanation_changed -eq [bool]$case.ExpectedExplanationChanged) `
            -Message ("case '{0}' explanation changed expectation mismatch" -f $case.Name)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case={0} verdict={1} changed={2} identity_changed={3} compare_changed={4} evidence_changed={5} explanation_changed={6}" -f
            $case.Name,
            [string]$summary.witness_verdict,
            [int]$summary.change_summary.changed_field_count,
            [bool]$summary.witness_regression_surface.event_identity_changed,
            [bool]$summary.witness_regression_surface.compare_context_changed,
            [bool]$summary.witness_regression_surface.evidence_refs_changed,
            [bool]$summary.witness_regression_surface.explanation_changed
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
