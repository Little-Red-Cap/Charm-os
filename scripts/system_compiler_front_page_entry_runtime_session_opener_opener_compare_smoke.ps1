param(
    [string]$RuntimeSessionOpenerCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-opener-compare-smoke",
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
        route_id = "runtime-session-opener-compare-route"
        depth = 0
        surface_id = "opener_compare"
        label = "Runtime-session opener compare"
        role = "opening_judgment_compare"
        summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opener_compare"
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
        tab_title = "Runtime Session Opener Compare"
        entry_role = "opening_judgment_compare"
        summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opener_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $true
        followup_query_kinds = @("opener_evidence_refs", "compare_context")
        rationale = "Open the runtime-session opener compare as the nearest consumer-side opening judgment compare."
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
    Write-TextFile -Path $landingReportPath -Content "# Runtime Session Opener Compare Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "runtime_session_opener_compare_landing`n"

    $landingPath = Resolve-FullPath -Path $LandingPath
    $landingReportPath = Resolve-FullPath -Path $landingReportPath
    $landingCheckPath = Resolve-FullPath -Path $landingCheckPath
    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $caseRoot
    $tabId = "opener_compare"
    $entry = New-EntryRef -SummaryPath $targetSummaryPath -ReportPath $targetReportPath -CheckPath $targetCheckPath
    $query = New-QueryHint -TabId $tabId
    $landingTab = [ordered]@{
        tab_id = $tabId
        title = "Runtime Session Opener Compare"
        capability_ids = @("system_compiler.front_page_entry_opener_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_runtime_session_opener_opener_compare_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Runtime Session Opener Compare Landing"
            summary = "A targeted landing that keeps runtime-session opener compare on the consumer side of the boundary."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "opener_compare"
                    label = "runtime session opener compare"
                    role = "opening_judgment_compare"
                    summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
                    summary_path = $targetSummaryPath
                    report_markdown_path = $targetReportPath
                    check_text_path = $targetCheckPath
                }
            )
        }
        route_provenance = @(
            [ordered]@{
                id = "runtime-session-opener-compare-route"
                route_kind = "runtime_session_targeted_smoke"
                source_summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
                source_summary_path = $targetSummaryPath
                source_input_summary_path = $targetSummaryPath
                source_root_summary_path = $targetSummaryPath
                source_report_markdown_path = $targetReportPath
                source_check_text_path = $targetCheckPath
                level1_surface_ids = @("opener_compare")
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
            surface_id = "opener_compare"
            label = "runtime session opener compare"
            role = "opening_judgment_compare"
            summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
            summary_kind = "system_compiler.front_page_entry_opener_compare"
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "compare"
            entry_tier = "compare_ready"
            opening_reason = [ordered]@{
                kind = "counterfactual_verdict"
                summary = "Open the runtime-session opener compare because it explains whether consumer-side opening judgments stayed aligned."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "drifted"
            }
            primary_tab_id = $tabId
            primary_summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
            primary_summary_kind = "system_compiler.front_page_entry_opener_compare"
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
                root_id = "opener_compare"
                root_kind = "opener_compare"
                source_summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = $targetSummaryPath
                owner_route_ids = @("runtime-session-opener-compare-route")
                owner_surface_ids = @("opener_compare")
                available_supporting_surface_ids = @("opener_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this runtime-session opener compare be rendered before reopening lower opening testimonies?")
            next_questions = @("Should the next consumer follow opener evidence only, rather than reopening runtime/session artifacts?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionOpenerCompareRootPath = Resolve-FullPath -Path $RuntimeSessionOpenerCompareRoot
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

$runtimeSessionOpenerCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opener_compare_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($runtimeSessionOpenerCompareSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $runtimeSessionCompareCaseRoot = Join-Path $runtimeSessionOpenerCompareRootPath "runtime-session-opener-neutral-to-collapsed-testimony\compare"
    $runtimeSessionCompareSummaryPath = Join-Path $runtimeSessionCompareCaseRoot "front-page.entry-opener.compare.summary.json"
    $runtimeSessionCompareReportPath = Join-Path $runtimeSessionCompareCaseRoot "front-page.entry-opener.compare.report.md"
    $runtimeSessionCompareCheckPath = Join-Path $runtimeSessionCompareCaseRoot "front-page.entry-opener.compare.check.txt"

    if ($Clean -or -not (Test-Path -LiteralPath $runtimeSessionCompareSummaryPath)) {
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
        Write-Host "[RUNTIME-SESSION-OPENER-OPENER-COMPARE-SMOKE] opener_compare_bootstrap=reuse-existing"
    }

    foreach ($requiredArtifact in @($runtimeSessionCompareSummaryPath, $runtimeSessionCompareReportPath, $runtimeSessionCompareCheckPath)) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing runtime-session opener compare artifact: $requiredArtifact"
        }
    }

    $compareSummary = Load-JsonObject -Path $runtimeSessionCompareSummaryPath
    Assert-Condition `
        -Condition ([string]$compareSummary.opener_verdict -eq "drifted") `
        -Message ("expected drifted runtime-session opener compare fixture but got '{0}'" -f $compareSummary.opener_verdict)
    Assert-Condition `
        -Condition ([int]$compareSummary.change_summary.changed_field_count -eq 6) `
        -Message ("expected changed field count 6 but got '{0}'" -f $compareSummary.change_summary.changed_field_count)
    Assert-Condition `
        -Condition ([bool]$compareSummary.opener_changes.projection_changed) `
        -Message "expected runtime-session opener compare fixture to change projection"
    Assert-Condition `
        -Condition (-not [bool]$compareSummary.opener_changes.compare_context_changed) `
        -Message "did not expect runtime-session opener compare fixture to change compare_context"

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $runtimeSessionCompareSummaryPath `
        -TargetReportPath $runtimeSessionCompareReportPath `
        -TargetCheckPath $runtimeSessionCompareCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "runtime-session-opener-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "runtime-session opener export failed for opener compare"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "runtime-session opener validation failed for opener compare"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.status -eq "ready") `
        -Message ("expected open action ready but got '{0}'" -f $openerSummary.open_action.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "opener_compare") `
        -Message ("expected selected tab opener_compare but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([bool]$openerSummary.inspector_invocation.ready -eq $false) `
        -Message "expected inspector invocation to stay blocked for non-artifact-report target"
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected opened projection available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "opener_compare_overview") `
        -Message ("expected opener_compare_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("verdict=drifted")) `
        -Message ("expected drifted verdict in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("changed=6")) `
        -Message ("expected changed=6 in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $summaryLines = @($openerSummary.opened_projection.summary_lines) | ForEach-Object { [string]$_ }
    Assert-Condition `
        -Condition (@($summaryLines | Where-Object { $_ -like "change_counts opening=*projection=*compare_context=*inspector=*questions=*" }).Count -eq 1) `
        -Message "expected opened projection to expose change_counts summary line"
    Assert-Condition `
        -Condition (@($summaryLines | Where-Object { $_ -like "opener_changes compare_context=no projection=yes inspector=no questions=no" }).Count -eq 1) `
        -Message "expected opened projection to expose projection-only opener change digest"
    Assert-Condition `
        -Condition (@($summaryLines | Where-Object { $_ -like "impact_counts regressions=0 improvements=0 neutral=6" }).Count -eq 1) `
        -Message "expected opened projection to expose neutral-only impact counts"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference baseline and candidate opener summaries"

    Write-Host (
        "[RUNTIME-SESSION-OPENER-OPENER-COMPARE-SMOKE] projection={0}/{1} headline='{2}' evidence_paths={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]@($openerSummary.opened_projection.evidence_paths).Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENER-OPENER-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
