param(
    [string]$OpenerCompareRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-opener-compare-smoke",
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
        route_id = "opener-compare-route"
        depth = 0
        surface_id = "opener_compare"
        label = "Front page entry opener compare"
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
        tab_title = "Opener Compare"
        entry_role = "opening_judgment_compare"
        summary_schema = "system_compiler.front_page_entry_opener_compare/v0"
        summary_kind = "system_compiler.front_page_entry_opener_compare"
        scope = "report"
        selection_rule = "single_report"
        query_kind = "default_overview"
        compare_expected = $true
        followup_query_kinds = @("opener_evidence_refs", "compare_context")
        rationale = "Open the opener compare as the first explainable opening judgment compare preview."
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
    Write-TextFile -Path $landingReportPath -Content "# Synthetic Opener Compare Landing`n"
    Write-TextFile -Path $landingCheckPath -Content "synthetic opener compare landing`n"

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
        title = "Opener Compare"
        capability_ids = @("system_compiler.front_page_entry_opener_compare")
        entry = $entry
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_opener_compare_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Opener Compare Landing"
            summary = "A targeted landing that proves opener projection for opener compare judgments."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = @(
                [ordered]@{
                    id = "opener_compare"
                    label = "opener compare"
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
                id = "opener-compare-route"
                route_kind = "synthetic_targeted_smoke"
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
            label = "Front page entry opener compare"
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
                summary = "Open the opener compare because it judges whether two opener facades preserve the same explainable opening decision."
                source_summary_path = $targetSummaryPath
                drift_changed = $true
                drift_verdict = "improved"
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
                owner_route_ids = @("opener-compare-route")
                owner_surface_ids = @("opener_compare")
                available_supporting_surface_ids = @("opener_compare")
            }
        )
        query_hints = [ordered]@{
            primary_query = $query
            tab_queries = @($query)
        }
        questions = [ordered]@{
            compare_questions = @("Should this opener compare be shown before opening either side's opener details?")
            next_questions = @("Should opener compare narratives feed the next explain surface prompt?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$openerCompareRootPath = Resolve-FullPath -Path $OpenerCompareRoot
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

$openerCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opener_workspace_compare_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($openerCompareSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $openerCompareCaseRoot = Join-Path $openerCompareRootPath "workspace-cold-to-hot-compare-context\compare"
    $openerCompareSummaryPath = Join-Path $openerCompareCaseRoot "front-page.entry-opener.compare.summary.json"
    $openerCompareReportPath = Join-Path $openerCompareCaseRoot "front-page.entry-opener.compare.report.md"
    $openerCompareCheckPath = Join-Path $openerCompareCaseRoot "front-page.entry-opener.compare.check.txt"

    if ($Clean -or -not (Test-Path -LiteralPath $openerCompareSummaryPath)) {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openerCompareSmokeScript,
                "-OutputRoot",
                $openerCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opener workspace compare smoke bootstrap failed"
    } else {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-OPENER-COMPARE-SMOKE] opener_compare_bootstrap=reuse-existing"
    }

    foreach ($requiredArtifact in @($openerCompareSummaryPath, $openerCompareReportPath, $openerCompareCheckPath)) {
        if (-not (Test-Path -LiteralPath $requiredArtifact)) {
            throw "missing opener compare artifact: $requiredArtifact"
        }
    }

    $compareSummary = Load-JsonObject -Path $openerCompareSummaryPath
    Assert-Condition `
        -Condition ([string]$compareSummary.opener_verdict -eq "improved") `
        -Message ("expected improved opener compare fixture but got '{0}'" -f $compareSummary.opener_verdict)
    Assert-Condition `
        -Condition ([int]$compareSummary.change_summary.changed_field_count -eq 10) `
        -Message ("expected changed field count 10 but got '{0}'" -f $compareSummary.change_summary.changed_field_count)

    $landingRoot = Join-Path $outputRootPath "_synthetic_landing"
    $landingPath = New-MinimalLandingSummary `
        -LandingPath (Join-Path $landingRoot "front-page.entry-landing.summary.json") `
        -TargetSummaryPath $openerCompareSummaryPath `
        -TargetReportPath $openerCompareReportPath `
        -TargetCheckPath $openerCompareCheckPath

    $caseOutputRoot = Join-Path $outputRootPath "opener-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--landing",
            $landingPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "front page entry opener export failed for opener compare"

    $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener validation failed for opener compare"

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
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("verdict=improved")) `
        -Message ("expected improved verdict in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)
    Assert-Condition `
        -Condition (([string]$openerSummary.opened_projection.headline).Contains("changed=10")) `
        -Message ("expected changed=10 in projection headline but got '{0}'" -f $openerSummary.opened_projection.headline)

    $summaryLines = @($openerSummary.opened_projection.summary_lines)
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("change_counts ", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose change_counts summary line"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("impact_counts regressions=0 improvements=1 neutral=9", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose opener compare impact counts"
    Assert-Condition `
        -Condition (($summaryLines | Where-Object { ([string]$_).StartsWith("opener_improvement ", [System.StringComparison]::Ordinal) }).Count -gt 0) `
        -Message "expected opened projection to expose opener_improvement narrative"
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.evidence_paths).Count -ge 2) `
        -Message "expected opened projection to reference baseline and candidate opener summaries"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENER-OPENER-COMPARE-SMOKE] projection={0}/{1} headline='{2}' evidence_paths={3} inspector_ready={4}" -f
        [string]$openerSummary.opened_projection.status,
        [string]$openerSummary.opened_projection.projection_kind,
        [string]$openerSummary.opened_projection.headline,
        [int]@($openerSummary.opened_projection.evidence_paths).Count,
        [bool]$openerSummary.inspector_invocation.ready
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-OPENER-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
