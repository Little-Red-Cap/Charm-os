param(
    [string]$NeutralDriftRoot = "cmake-build-system-compiler-witness-open-event-witness-world-compare-neutral-drift-smoke",
    [string]$BlockedWorldCompareRoot = "cmake-build-system-compiler-witness-open-event-witness-world-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-open-event-witness-compare-smoke",
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

function New-EntryRef {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        route_id = "runtime-session-open-event-witness-compare-route"
        depth = 0
        surface_id = "open_event_witness_compare"
        label = "Runtime-session open event witness compare"
        role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
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
        tab_title = "Runtime Session Open Event Witness Compare"
        entry_role = "supporting_testimony"
        summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $false
        followup_query_kinds = @("evidence_refs", "explanation")
        rationale = "Open the runtime-session open-event witness compare as the nearest explainable testimony compare without reopening raw runtime/session evidence."
    }
}

function New-MinimalLandingSummary {
    param(
        [string]$LandingPath,
        [string]$TargetSummaryPath,
        [string]$TargetReportPath,
        [string]$TargetCheckPath,
        [string]$LandingTitle,
        [string]$LandingSummary,
        [string]$GeneratorName
    )

    $caseRoot = Split-Path -Parent $LandingPath
    Ensure-Directory -Path $caseRoot
    $landingReportPath = Join-Path $caseRoot "front-page.entry-landing.report.md"
    $landingCheckPath = Join-Path $caseRoot "front-page.entry-landing.check.txt"
    Write-TextFile -Path $landingReportPath -Content ("# {0}`n" -f $LandingTitle)
    Write-TextFile -Path $landingCheckPath -Content ("{0}`n" -f $LandingTitle.ToLowerInvariant().Replace(" ", "_"))

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "open_event_witness_compare"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Runtime Session Open Event Witness Compare"
        capability_ids = @("system_compiler.open_event_witness_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = $GeneratorName
        result = "ok"
        entry_landing = [ordered]@{
            title = $LandingTitle
            summary = $LandingSummary
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "open_event_witness_compare"
                    label = "runtime session open event witness compare"
                    role = "supporting_testimony"
                    summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "runtime-session-open-event-witness-compare-route"
                route_kind = "runtime_session_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("open_event_witness_compare")
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
            surface_id = "open_event_witness_compare"
            label = "runtime session open event witness compare"
            role = "supporting_testimony"
            summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
            summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "evidence"
            entry_tier = "evidence_only"
            opening_reason = [ordered]@{
                kind = "supporting_evidence"
                summary = "OpenEventWitnessCompare is available as the nearest testimony compare target for runtime-session opening."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare"
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
        fallback_mode_order = @("evidence", "route")
        primary_landing = $landingTab
        secondary_landings = @()
        landing_tabs = @($landingTab)
        provenance_roots = @(
            [ordered]@{
                root_id = "open_event_witness_compare"
                root_kind = "open_event_witness_compare"
                source_summary_schema = "system_compiler.front_page_entry_opening_flow_open_event_witness_compare/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("runtime-session-open-event-witness-compare-route")
                owner_surface_ids = @("open_event_witness_compare")
                available_supporting_surface_ids = @("open_event_witness_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this runtime-session OpenEventWitnessCompare projection be shown before opening deeper witness details?")
            next_questions = @("Should the next consumer follow evidence refs only, rather than reopening runtime/session artifacts?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
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
$neutralDriftRootPath = Resolve-FullPath -Path $NeutralDriftRoot
$blockedWorldCompareRootPath = Resolve-FullPath -Path $BlockedWorldCompareRoot
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

$neutralDriftSmokeScript = Join-Path $PSScriptRoot "system_compiler_witness_open_event_witness_world_compare_neutral_drift_smoke.ps1"
$blockedWorldCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_witness_open_event_witness_world_compare_smoke.ps1"
$compareExportScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$compareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py"
$openerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$openerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @(
    $neutralDriftSmokeScript,
    $blockedWorldCompareSmokeScript,
    $compareExportScript,
    $compareValidateScript,
    $openerExportScript,
    $openerValidateScript
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $neutralCompareSummaryPath = Join-Path $neutralDriftRootPath "open-event-witness-compare\front-page.entry-opening-flow.open-event.witness.compare.summary.json"
    $neutralCompareReportPath = Join-Path $neutralDriftRootPath "open-event-witness-compare\report.md"
    $neutralCompareCheckPath = Join-Path $neutralDriftRootPath "open-event-witness-compare\check.txt"
    if ($Clean -or -not (Test-Path -LiteralPath $neutralCompareSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $neutralDriftSmokeScript,
                "-OutputRoot",
                $neutralDriftRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session neutral drift witness compare bootstrap failed"
    } else {
        Write-Host "[RUNTIME-SESSION-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] neutral_drift_bootstrap=reuse-existing"
    }

    $blockedCandidateWitnessPath = Join-Path $blockedWorldCompareRootPath "candidate-source\witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ($Clean -or -not (Test-Path -LiteralPath $blockedCandidateWitnessPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $blockedWorldCompareSmokeScript,
                "-OutputRoot",
                $blockedWorldCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime-session blocked witness compare bootstrap failed"
    } else {
        Write-Host "[RUNTIME-SESSION-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] blocked_bootstrap=reuse-existing"
    }

    $neutralBaselineWitnessPath = Join-Path $neutralDriftRootPath "baseline-source\witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    foreach ($requiredArtifact in @(
        $neutralCompareSummaryPath,
        $neutralCompareReportPath,
        $neutralCompareCheckPath,
        $neutralBaselineWitnessPath,
        $blockedCandidateWitnessPath
    )) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing runtime-session compare artifact: $requiredArtifact"
        }
    }

    $collapsedCompareRoot = Join-Path $outputRootPath "_collapsed_compare_source"
    $collapsedCompareSummaryPath = Join-Path $collapsedCompareRoot "front-page.entry-opening-flow.open-event.witness.compare.summary.json"
    $collapsedCompareReportPath = Join-Path $collapsedCompareRoot "report.md"
    $collapsedCompareCheckPath = Join-Path $collapsedCompareRoot "check.txt"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareExportScript,
            "--baseline",
            $neutralBaselineWitnessPath,
            "--candidate",
            $blockedCandidateWitnessPath,
            "--output-root",
            $collapsedCompareRoot,
            "--summary",
            $collapsedCompareSummaryPath,
            "--report-markdown",
            $collapsedCompareReportPath,
            "--check-text",
            $collapsedCompareCheckPath
        ) `
        -FailureMessage "runtime-session collapsed open-event witness compare export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareValidateScript, "--summary", $collapsedCompareSummaryPath) `
        -FailureMessage "runtime-session collapsed open-event witness compare validation failed"

    $cases = @(
        [ordered]@{
            Name = "runtime-session-neutral-drift-open-event-witness-compare"
            CompareSummaryPath = $neutralCompareSummaryPath
            CompareReportPath = $neutralCompareReportPath
            CompareCheckPath = $neutralCompareCheckPath
            ExpectedVerdict = "drifted"
            ExpectedCandidateWitnessStatus = "ok"
            ExpectedQuestionPattern = "*drift*"
            ExpectedNarrativePattern = "witness_drift *"
        },
        [ordered]@{
            Name = "runtime-session-collapsed-open-event-witness-compare"
            CompareSummaryPath = $collapsedCompareSummaryPath
            CompareReportPath = $collapsedCompareReportPath
            CompareCheckPath = $collapsedCompareCheckPath
            ExpectedVerdict = "collapsed"
            ExpectedCandidateWitnessStatus = "fail"
            ExpectedQuestionPattern = "*collapsed*"
            ExpectedNarrativePattern = "witness_drift *no longer stands as testimony*"
        }
    )

    foreach ($case in $cases) {
        $landingRoot = Join-Path $outputRootPath ("_{0}_landing" -f $case.Name)
        $landingPath = New-MinimalLandingSummary `
            -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
            -TargetSummaryPath $case.CompareSummaryPath `
            -TargetReportPath $case.CompareReportPath `
            -TargetCheckPath $case.CompareCheckPath `
            -LandingTitle ("Runtime Session Open Event Witness Compare Landing: {0}" -f $case.Name) `
            -LandingSummary "A targeted runtime-session testimony compare landing that stays on the consumer side of the boundary." `
            -GeneratorName "scripts/system_compiler_front_page_entry_runtime_session_opener_open_event_witness_compare_smoke.ps1"

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $openerExportScript,
                "--landing",
                $landingPath,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("runtime-session opener export failed for case '{0}'" -f $case.Name)

        $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($openerValidateScript, "--summary", $openerSummaryPath) `
            -FailureMessage ("runtime-session opener validation failed for case '{0}'" -f $case.Name)

        $openerSummary = Load-JsonObject -Path $openerSummaryPath
        $projection = $openerSummary.opened_projection
        $summaryLines = @($projection.summary_lines) | ForEach-Object { [string]$_ }
        $questionLines = @($projection.question_lines) | ForEach-Object { [string]$_ }
        $evidencePaths = @($projection.evidence_paths) | ForEach-Object { [string]$_ }
        $supportingPaths = @($projection.supporting_summary_paths) | ForEach-Object { [string]$_ }
        $allProjectionPaths = @($evidencePaths + $supportingPaths)

        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.status -eq "ready") `
            -Message ("case '{0}' expected open action ready but got '{1}'" -f $case.Name, [string]$openerSummary.open_action.status)
        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "open_event_witness_compare") `
            -Message ("case '{0}' expected selected tab open_event_witness_compare but got '{1}'" -f $case.Name, [string]$openerSummary.open_action.selected_tab_id)
        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.query_kind -eq "default_overview") `
            -Message ("case '{0}' expected query kind default_overview but got '{1}'" -f $case.Name, [string]$openerSummary.open_action.query_kind)
        Assert-Condition `
            -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
            -Message ("case '{0}' expected inspector invocation to remain blocked for testimony compare target" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$projection.status -eq "available") `
            -Message ("case '{0}' expected projection status available but got '{1}'" -f $case.Name, [string]$projection.status)
        Assert-Condition `
            -Condition ([string]$projection.projection_kind -eq "open_event_witness_compare_overview") `
            -Message ("case '{0}' expected projection kind open_event_witness_compare_overview but got '{1}'" -f $case.Name, [string]$projection.projection_kind)
        Assert-Condition `
            -Condition (([string]$projection.headline).Contains("verdict=$($case.ExpectedVerdict)")) `
            -Message ("case '{0}' expected verdict '{1}' in projection headline but got '{2}'" -f $case.Name, $case.ExpectedVerdict, [string]$projection.headline)
        Assert-Condition `
            -Condition (@($summaryLines | Where-Object { $_ -like "baseline witness=*status=ok*" }).Count -eq 1) `
            -Message ("case '{0}' expected baseline witness summary line with status ok" -f $case.Name)
        Assert-Condition `
            -Condition (@($summaryLines | Where-Object { $_ -like ("candidate witness=*status={0}*" -f $case.ExpectedCandidateWitnessStatus) }).Count -eq 1) `
            -Message ("case '{0}' expected candidate witness summary line with status {1}" -f $case.Name, $case.ExpectedCandidateWitnessStatus)
        Assert-Condition `
            -Condition (@($summaryLines | Where-Object { $_ -like "change_counts identity=*judgment=*evidence=*explanation=*" }).Count -eq 1) `
            -Message ("case '{0}' expected change-count digest line" -f $case.Name)
        Assert-Condition `
            -Condition (@($summaryLines | Where-Object { $_ -like $case.ExpectedNarrativePattern }).Count -ge 1) `
            -Message ("case '{0}' expected witness_drift narrative matching '{1}'" -f $case.Name, $case.ExpectedNarrativePattern)
        Assert-Condition `
            -Condition (@($questionLines | Where-Object { $_ -like $case.ExpectedQuestionPattern }).Count -ge 1) `
            -Message ("case '{0}' expected question line matching '{1}'" -f $case.Name, $case.ExpectedQuestionPattern)
        Assert-Condition `
            -Condition ($evidencePaths.Count -ge 2) `
            -Message ("case '{0}' expected at least two evidence paths" -f $case.Name)
        Assert-Condition `
            -Condition ($supportingPaths.Count -ge 2) `
            -Message ("case '{0}' expected at least two supporting summary paths" -f $case.Name)
        Assert-NoRawRuntimeSessionLeak -Paths $allProjectionPaths -CaseName $case.Name

        Write-Host (
            "[RUNTIME-SESSION-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case={0} projection={1}/{2} headline='{3}' evidence_paths={4} supporting_paths={5} inspector_ready={6}" -f
            $case.Name,
            [string]$projection.status,
            [string]$projection.projection_kind,
            [string]$projection.headline,
            [int]$evidencePaths.Count,
            [int]$supportingPaths.Count,
            [bool]$openerSummary.inspector_invocation.ready
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENER-OPEN-EVENT-WITNESS-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
