param(
    [string]$OpeningFlowCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-opening-flow-compare-smoke",
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

function Write-TextFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }

    Set-Content -LiteralPath $Path -Encoding utf8 -Value $Content
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 100
    Write-TextFile -Path $Path -Content ($json + [Environment]::NewLine)
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

function New-FlowStep {
    param(
        [string]$Id,
        [string]$Label,
        [string]$OutputRoot
    )

    Ensure-Directory -Path $OutputRoot
    return [ordered]@{
        id = $Id
        label = $Label
        script_path = "scripts/system_compiler_front_page_entry_opener_opening_flow_compare_smoke.ps1"
        output_root = (Resolve-FullPath -Path $OutputRoot)
        status = "completed"
        produced_summary_count = 1
    }
}

function New-OpenerCase {
    param(
        [string]$Name,
        [string]$CaseRoot,
        [bool]$CompareContextAvailable,
        [string]$LandingVerdict
    )

    Ensure-Directory -Path $CaseRoot
    $summaryPath = Resolve-FullPath -Path (Join-Path $CaseRoot "front-page.entry-opener.summary.json")
    $reportPath = Resolve-FullPath -Path (Join-Path $CaseRoot "front-page.entry-opener.report.md")
    $checkPath = Resolve-FullPath -Path (Join-Path $CaseRoot "front-page.entry-opener.check.txt")
    $targetSummaryPath = Resolve-FullPath -Path (Join-Path $CaseRoot "target.summary.json")
    Write-JsonFile -Path $summaryPath -Value ([ordered]@{
        schema = "synthetic.front_page_entry_opener_reference/v0"
        name = $Name
    })
    Write-TextFile -Path $reportPath -Content ("# Synthetic opener case {0}`n" -f $Name)
    Write-TextFile -Path $checkPath -Content ("synthetic opener case {0}`n" -f $Name)
    Write-JsonFile -Path $targetSummaryPath -Value ([ordered]@{
        schema = "minimal_kernel.runtime_evidence_bundle.summary/v1"
        kind = "minimal_kernel.runtime_evidence_bundle"
        name = $Name
    })

    $reasonKind = if ($CompareContextAvailable) { "counterfactual_verdict" } else { "supporting_evidence" }
    $reasonSummary = if ($CompareContextAvailable) {
        "Compare-aware opener case stays near the default explain entry."
    } else {
        "Default opener case provides the first supporting evidence entry."
    }

    return [ordered]@{
        name = $Name
        summary_path = $summaryPath
        report_markdown_path = $reportPath
        check_text_path = $checkPath
        open_action_status = "ready"
        selected_tab_id = $Name
        selected_role = "supporting_evidence"
        query_kind = "default_overview"
        query_scope = "report"
        selection_rule = "single_report"
        opening_reason = [ordered]@{
            kind = $reasonKind
            summary = $reasonSummary
            source_summary_path = $summaryPath
            drift_changed = $CompareContextAvailable
            drift_verdict = $LandingVerdict
        }
        target_summary_schema = "minimal_kernel.runtime_evidence_bundle.summary/v1"
        target_summary_kind = "minimal_kernel.runtime_evidence_bundle"
        target_summary_path = $targetSummaryPath
        open_action_blockers = @()
        projection_status = "available"
        projection_kind = "runtime_evidence_bundle_overview"
        projection_headline = ("runtime_evidence case={0}" -f $Name)
        projection_summary_lines = @("synthetic runtime evidence projection")
        projection_question_lines = @("Should this opener case stay in the opening chain?")
        projection_blockers = @()
        compare_context_available = $CompareContextAvailable
        landing_verdict = $LandingVerdict
        inspector_ready = $false
        inspector_mode = "unresolved"
        inspector_blockers = @("synthetic opener target is not an artifact report")
        opener_compare_questions = @("Does this opener case still preserve the expected opening judgment?")
        opener_next_questions = @("Should this opener case become the next explain target?")
    }
}

function New-MinimalOpeningFlowSummary {
    param(
        [string]$FlowPath,
        [bool]$IncludeCompareCase
    )

    $flowRoot = Split-Path -Parent $FlowPath
    Ensure-Directory -Path $flowRoot
    $flowPath = Resolve-FullPath -Path $FlowPath
    $reportPath = Resolve-FullPath -Path (Join-Path $flowRoot "front-page.entry-opening-flow.report.md")
    $checkPath = Resolve-FullPath -Path (Join-Path $flowRoot "front-page.entry-opening-flow.check.txt")
    Write-TextFile -Path $reportPath -Content "# Synthetic Opening Flow`n"
    Write-TextFile -Path $checkPath -Content "synthetic opening flow`n"

    $caseRoot = Join-Path $flowRoot "opener-cases"
    $cases = @(
        New-OpenerCase `
            -Name "root-witness" `
            -CaseRoot (Join-Path $caseRoot "root-witness") `
            -CompareContextAvailable $false `
            -LandingVerdict ""
    )
    if ($IncludeCompareCase) {
        $cases += New-OpenerCase `
            -Name "root-witness-to-root-world-compare" `
            -CaseRoot (Join-Path $caseRoot "root-witness-to-root-world-compare") `
            -CompareContextAvailable $true `
            -LandingVerdict "improved"
    }

    $supportingSurfaces = @()
    foreach ($case in @($cases)) {
        $supportingSurfaces += [ordered]@{
            id = [string]$case.name
            label = ("opener case: {0}" -f [string]$case.name)
            role = "opener_case"
            summary_schema = "system_compiler.front_page_entry_opener/v0"
            summary_path = [string]$case.summary_path
            report_markdown_path = [string]$case.report_markdown_path
            check_text_path = [string]$case.check_text_path
        }
    }

    $actualOpenerCount = @($cases).Count
    $compareContextCount = @($cases | Where-Object { [bool]$_.compare_context_available }).Count
    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_opening_flow/v0"
        kind = "system_compiler.front_page_entry_opening_flow"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_opening_flow_compare_smoke.ps1"
        result = "ok"
        opening_flow = [ordered]@{
            title = "Synthetic Opening Flow"
            summary = "A minimal flow fixture used to prove opener projection for opening-flow compare judgments."
        }
        front_page = [ordered]@{
            summary_path = $flowPath
            report_markdown_path = $reportPath
            check_text_path = $checkPath
            supporting_surfaces = $supportingSurfaces
        }
        artifact_context = [ordered]@{
            route_root = Resolve-FullPath -Path (Join-Path $flowRoot "route")
            capability_root = Resolve-FullPath -Path (Join-Path $flowRoot "capability")
            landing_root = Resolve-FullPath -Path (Join-Path $flowRoot "landing")
            landing_compare_root = Resolve-FullPath -Path (Join-Path $flowRoot "landing-compare")
            opener_root = Resolve-FullPath -Path $caseRoot
            output_root = Resolve-FullPath -Path $flowRoot
            flow_summary_path = $flowPath
            report_markdown_path = $reportPath
            check_text_path = $checkPath
        }
        flow_status = [ordered]@{
            expected_opener_count = 2
            actual_opener_count = $actualOpenerCount
            available_projection_count = $actualOpenerCount
            compare_context_count = $compareContextCount
            inspector_ready_count = 0
            blocked_inspector_count = $actualOpenerCount
            completed_step_count = 3
            result = "ok"
        }
        flow_steps = @(
            New-FlowStep -Id "route" -Label "front page route" -OutputRoot (Join-Path $flowRoot "route")
            New-FlowStep -Id "landing" -Label "front page landing" -OutputRoot (Join-Path $flowRoot "landing")
            New-FlowStep -Id "opener" -Label "front page opener" -OutputRoot $caseRoot
        )
        opener_cases = @($cases)
        questions = [ordered]@{
            compare_questions = @("Should this synthetic opening flow stay as the projection fixture?")
            next_questions = @("Should removed compare-aware opener cases block publication?")
        }
        violations = @()
    }

    Write-JsonFile -Path $flowPath -Value $summary
    return $flowPath
}

function New-EntryRef {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        route_id = "opening-flow-compare-route"
        depth = 0
        surface_id = "opening_flow_compare"
        label = "Front page entry opening flow compare"
        role = "opening_chain_compare"
        summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_compare"
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        revisit = $false
        cycle = $false
        expanded = $true
        route_provenance_count = 1
        supporting_surface_count = 0
    }
}

function New-QueryHint {
    param(
        [string]$TabId
    )

    return [ordered]@{
        tab_id = $TabId
        tab_title = "Opening Flow Compare"
        entry_role = "opening_chain_compare"
        summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $true
        followup_query_kinds = @("flow_evidence_refs", "opener_case_drift")
        rationale = "Open the opening-flow compare as the first explainable opening-chain judgment preview."
    }
}

function New-MinimalLandingSummary {
    param(
        [string]$LandingPath,
        [string]$TargetSummaryPath,
        [string]$TargetReportPath,
        [string]$TargetCheckPath
    )

    $caseRoot = Split-Path -Parent $LandingPath
    Ensure-Directory -Path $caseRoot
    $landingReportPath = Join-Path $caseRoot "front-page.entry-landing.report.md"
    $landingCheckPath = Join-Path $caseRoot "front-page.entry-landing.check.txt"
    Write-TextFile -Path $landingReportPath -Content "# Synthetic Opening Flow Compare Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "synthetic opening flow compare landing`n"

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "opening_flow_compare"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Opening Flow Compare"
        capability_ids = @("system_compiler.front_page_entry_opening_flow_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_opening_flow_compare_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Opening Flow Compare Landing"
            summary = "A targeted landing that proves opener projection for opening-flow compare judgments."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "opening_flow_compare"
                    label = "opening flow compare"
                    role = "opening_chain_compare"
                    summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "opening-flow-compare-route"
                route_kind = "synthetic_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("opening_flow_compare")
            }
        )
        artifact_context = [ordered]@{
            input_capability_summary_path = $targetSummaryPath
            output_root = $outputRoot
            landing_summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
        }
        root_surface = [ordered]@{
            surface_id = "opening_flow_compare"
            label = "Front page entry opening flow compare"
            role = "opening_chain_compare"
            summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
            summary_kind = "system_compiler.front_page_entry_opening_flow_compare"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "compare"
            entry_tier = "compare_ready"
            opening_reason = [ordered]@{
                kind = "counterfactual_verdict"
                summary = "Open the opening-flow compare because it judges whether the whole explainable opening chain still closes."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opening_flow_compare"
            available_tab_ids = @($tabId)
            fallback_tab_ids = @()
            tab_count = 1
            fallback_tab_count = 0
            provenance_root_count = 1
            route_provenance_entry_count = 1
            direct_review_available = $false
            direct_compare_available = $true
            direct_biography_available = $false
            direct_evidence_available = $true
            direct_runtime_session_available = $false
        }
        fallback_mode_order = @("compare", "route")
        primary_landing = $landingTab
        secondary_landings = @()
        landing_tabs = @($landingTab)
        provenance_roots = @(
            [ordered]@{
                root_id = "opening_flow_compare"
                root_kind = "opening_flow_compare"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_compare/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("opening-flow-compare-route")
                owner_surface_ids = @("opening_flow_compare")
                available_supporting_surface_ids = @("opening_flow_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this opening-flow compare be shown before opening individual opener case details?")
            next_questions = @("Should removed opener cases become the next explain surface prompt?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openingFlowCompareRootPath = Resolve-FullPath -Path $OpeningFlowCompareRoot
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

$openingFlowCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow.py"
$validateOpeningFlowCompareScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_compare.py"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($openingFlowCompareScript, $validateOpeningFlowCompareScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $openingFlowCompareCaseRoot = Join-Path $openingFlowCompareRootPath "removed-compare-opener"
    $openingFlowCompareSummaryPath = Join-Path $openingFlowCompareCaseRoot "front-page.entry-opening-flow.compare.summary.json"
    $openingFlowCompareReportPath = Join-Path $openingFlowCompareCaseRoot "front-page.entry-opening-flow.compare.report.md"
    $openingFlowCompareCheckPath = Join-Path $openingFlowCompareCaseRoot "front-page.entry-opening-flow.compare.check.txt"

    if ($Clean -or -not (Test-Path -LiteralPath $openingFlowCompareSummaryPath)) {
        $flowFixtureRoot = Join-Path $openingFlowCompareRootPath "_synthetic_flow"
        $baselineFlowSummaryPath = New-MinimalOpeningFlowSummary `
            -FlowPath (Join-Path $flowFixtureRoot "baseline\front-page.entry-opening-flow.summary.json") `
            -IncludeCompareCase $true
        $candidateFlowSummaryPath = New-MinimalOpeningFlowSummary `
            -FlowPath (Join-Path $flowFixtureRoot "candidate\front-page.entry-opening-flow.summary.json") `
            -IncludeCompareCase $false

        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $openingFlowCompareScript,
                "--baseline",
                $baselineFlowSummaryPath,
                "--candidate",
                $candidateFlowSummaryPath,
                "--output-root",
                $openingFlowCompareCaseRoot
            ) `
            -FailureMessage "front page entry opening flow compare export failed"

        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateOpeningFlowCompareScript, "--summary", $openingFlowCompareSummaryPath) `
            -FailureMessage "front page entry opening flow compare validation failed"
    } else {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-OPENING-FLOW-COMPARE-SMOKE] opening_flow_compare_bootstrap=reuse-existing"
    }

    foreach ($requiredArtifact in @($openingFlowCompareSummaryPath, $openingFlowCompareReportPath, $openingFlowCompareCheckPath)) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing opening flow compare artifact: $requiredArtifact"
        }
    }

    $compareSummary = Load-JsonObject -Path $openingFlowCompareSummaryPath
    Assert-Condition `
        -Condition ([string]$compareSummary.flow_verdict -eq "drifted") `
        -Message ("expected drifted opening flow compare fixture but got '{0}'" -f $compareSummary.flow_verdict)
    Assert-Condition `
        -Condition ([int]$compareSummary.opener_case_summary.removed_case_count -eq 1) `
        -Message ("expected removed opener case count 1 but got '{0}'" -f $compareSummary.opener_case_summary.removed_case_count)

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $openingFlowCompareSummaryPath `
        -TargetReportPath $openingFlowCompareReportPath `
        -TargetCheckPath $openingFlowCompareCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "opening-flow-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "front page entry opener export failed for opening flow compare"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener validation failed for opening flow compare"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.status -eq "ready") `
        -Message ("expected open action ready but got '{0}'" -f $openerSummary.open_action.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "opening_flow_compare") `
        -Message ("expected selected tab opening_flow_compare but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
        -Message "expected inspector invocation to stay blocked for non-artifact-report target"
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected opened projection available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "opening_flow_compare_overview") `
        -Message ("expected opening_flow_compare_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("verdict=drifted")) `
        -Message ("expected drifted verdict in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("removed=1")) `
        -Message ("expected removed=1 in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $summaryLines = @($openerSummary.opened_projection.summary_lines)
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("flow_counts ", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose flow_counts summary line"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("case_counts changed=0 added=0 removed=1", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose removed case count"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("flow_regression ", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose flow_regression narrative"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference baseline and candidate opening flow summaries"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENER-OPENING-FLOW-COMPARE-SMOKE] projection={0}/{1} headline='{2}' evidence_paths={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]@($openerSummary.opened_projection.evidence_paths).Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-OPENING-FLOW-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
