param(
    [string]$LandingRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
    [string]$LandingCompareRoot = "cmake-build-system-compiler-front-page-entry-landing-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-smoke",
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

    if (-not (Test-Path $Path)) {
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

    if (Test-Path $Path) {
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

function Test-AllPathsExist {
    param(
        [string[]]$Paths
    )

    foreach ($path in @($Paths)) {
        if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path $path)) {
            return $false
        }
    }

    return $true
}

function Assert-OpeningReason {
    param(
        $OpenerSummary,
        [string]$CaseName
    )

    Assert-Condition `
        -Condition ($OpenerSummary.source_landing.opening_reason -is [System.Management.Automation.PSCustomObject] -or $OpenerSummary.source_landing.opening_reason -is [hashtable]) `
        -Message ("case '{0}' source_landing must carry opening_reason" -f $CaseName)
    Assert-Condition `
        -Condition (($OpenerSummary.open_action.opening_reason | ConvertTo-Json -Depth 8 -Compress) -eq ($OpenerSummary.source_landing.opening_reason | ConvertTo-Json -Depth 8 -Compress)) `
        -Message ("case '{0}' expected open_action opening_reason to pass through source_landing opening_reason" -f $CaseName)

    $openingReasonLines = @(
        @($OpenerSummary.opened_projection.summary_lines) |
            Where-Object { ([string]$_).StartsWith("opening_reason ", [System.StringComparison]::Ordinal) }
    )
    Assert-Condition `
        -Condition ($openingReasonLines.Count -gt 0) `
        -Message ("case '{0}' expected opened projection to expose opening_reason summary line" -f $CaseName)
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

    $json = $Value | ConvertTo-Json -Depth 32
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [System.Text.Encoding]::UTF8)
}

function Get-LandingTabById {
    param(
        $LandingSummary,
        [string]$TabId
    )

    foreach ($tab in @($LandingSummary.landing_tabs)) {
        if ([string]$tab.tab_id -eq $TabId) {
            return $tab
        }
    }

    throw "landing tab not found: $TabId"
}

function Get-LandingQueryByTabId {
    param(
        $LandingSummary,
        [string]$TabId
    )

    foreach ($query in @($LandingSummary.query_hints.tab_queries)) {
        if ([string]$query.tab_id -eq $TabId) {
            return $query
        }
    }

    throw "landing query not found: $TabId"
}

function New-SyntheticLandingSummary {
    param(
        [string]$SourceLandingPath,
        [string]$SyntheticLandingPath,
        [string]$SelectedTabId,
        [string]$OverrideSummarySchema = "",
        [string]$OverrideSummaryKind = "",
        [string]$OverrideSummaryPath = "",
        [string]$OverrideReportMarkdownPath = "",
        [string]$OverrideCheckTextPath = "",
        [string]$OverrideQueryKind = "",
        [string]$OverrideQueryScope = "",
        [string[]]$OverrideFollowupQueryKinds = @()
    )

    $landingSummary = Load-JsonObject -Path $SourceLandingPath
    $tab = Get-LandingTabById -LandingSummary $landingSummary -TabId $SelectedTabId
    $query = Get-LandingQueryByTabId -LandingSummary $landingSummary -TabId $SelectedTabId

    $landingSummary.landing_status.primary_tab_id = [string]$SelectedTabId
    $landingSummary.landing_status.primary_summary_schema = [string]$tab.entry.summary_schema
    $landingSummary.landing_status.primary_summary_kind = [string]$tab.entry.summary_kind
    $landingSummary.primary_landing = $tab
    $landingSummary.query_hints.primary_query = $query

    if (-not [string]::IsNullOrWhiteSpace($OverrideSummarySchema)) {
        $landingSummary.primary_landing.entry.summary_schema = [string]$OverrideSummarySchema
        $landingSummary.landing_status.primary_summary_schema = [string]$OverrideSummarySchema
        $landingSummary.query_hints.primary_query.summary_schema = [string]$OverrideSummarySchema
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideSummaryKind)) {
        $landingSummary.primary_landing.entry.summary_kind = [string]$OverrideSummaryKind
        $landingSummary.landing_status.primary_summary_kind = [string]$OverrideSummaryKind
        $landingSummary.query_hints.primary_query.summary_kind = [string]$OverrideSummaryKind
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideSummaryPath)) {
        $landingSummary.primary_landing.entry.summary_path = [string]$OverrideSummaryPath
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideReportMarkdownPath)) {
        $landingSummary.primary_landing.entry.report_markdown_path = [string]$OverrideReportMarkdownPath
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideCheckTextPath)) {
        $landingSummary.primary_landing.entry.check_text_path = [string]$OverrideCheckTextPath
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideQueryKind)) {
        $landingSummary.query_hints.primary_query.query_kind = [string]$OverrideQueryKind
    }
    if (-not [string]::IsNullOrWhiteSpace($OverrideQueryScope)) {
        $landingSummary.query_hints.primary_query.scope = [string]$OverrideQueryScope
    }
    if (@($OverrideFollowupQueryKinds).Count -gt 0) {
        $landingSummary.query_hints.primary_query.followup_query_kinds = @($OverrideFollowupQueryKinds)
    }

    Write-JsonFile -Path $SyntheticLandingPath -Value $landingSummary
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$landingRootPath = Resolve-FullPath -Path $LandingRoot
$landingCompareRootPath = Resolve-FullPath -Path $LandingCompareRoot
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

$landingSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_smoke.ps1"
$landingCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_landing_compare_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($landingSmokeScript, $landingCompareSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $landingInputs = @(
        (Join-Path $landingRootPath "root-witness\front-page.entry-landing.summary.json"),
        (Join-Path $landingRootPath "root-world-compare\front-page.entry-landing.summary.json")
    )
    if (Test-AllPathsExist -Paths $landingInputs) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-SMOKE] landing_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $landingSmokeScript,
                "-OutputRoot",
                $landingRootPath
            ) `
            -FailureMessage "front page entry landing smoke bootstrap failed"
    }

    $landingCompareInputs = @(
        (Join-Path $landingCompareRootPath "root-witness-to-root-world-compare\front-page.entry-landing.compare.summary.json"),
        (Join-Path $landingCompareRootPath "root-world-compare-to-root-witness\front-page.entry-landing.compare.summary.json")
    )
    if (Test-AllPathsExist -Paths $landingCompareInputs) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENER-SMOKE] landing_compare_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $landingCompareSmokeScript,
                "-InputRoot",
                $landingRootPath,
                "-OutputRoot",
                $landingCompareRootPath
            ) `
            -FailureMessage "front page entry landing compare smoke bootstrap failed"
    }

    $syntheticRoot = Join-Path $outputRootPath "_synthetic_landings"
    Ensure-Directory -Path $syntheticRoot

    $witnessLanding = Join-Path $landingRootPath "root-witness\front-page.entry-landing.summary.json"
    $reviewLanding = Join-Path $landingRootPath "witness-ci-shelf\front-page.entry-landing.summary.json"
    $runtimeEvidenceSampleSummary = Resolve-FullPath -Path "schemas\examples\minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json"
    $runtimeEvidenceSampleReport = Resolve-FullPath -Path "docs\system\minimal_kernel_runtime_evidence_bundle_contract.md"
    $runtimeEvidenceSampleCheck = Resolve-FullPath -Path "schemas\README.md"
    $runtimeSessionSampleSummary = Resolve-FullPath -Path "schemas\examples\minimal_kernel.kernel_runtime_session.v0.sample.json"
    $runtimeSessionSampleReport = Resolve-FullPath -Path "docs\system\kernel_runtime_session_witness_v0.md"
    $runtimeSessionSampleCheck = Resolve-FullPath -Path "schemas\README.md"

    New-SyntheticLandingSummary `
        -SourceLandingPath $witnessLanding `
        -SyntheticLandingPath (Join-Path $syntheticRoot "root-witness-supporting-testimony.front-page.entry-landing.summary.json") `
        -SelectedTabId "supporting_testimony"

    New-SyntheticLandingSummary `
        -SourceLandingPath $reviewLanding `
        -SyntheticLandingPath (Join-Path $syntheticRoot "witness-ci-shelf-shelf-compare.front-page.entry-landing.summary.json") `
        -SelectedTabId "shelf_compare"

    New-SyntheticLandingSummary `
        -SourceLandingPath $reviewLanding `
        -SyntheticLandingPath (Join-Path $syntheticRoot "witness-ci-shelf-candidate-shelf.front-page.entry-landing.summary.json") `
        -SelectedTabId "candidate_shelf"

    New-SyntheticLandingSummary `
        -SourceLandingPath $witnessLanding `
        -SyntheticLandingPath (Join-Path $syntheticRoot "runtime-evidence-sample.front-page.entry-landing.summary.json") `
        -SelectedTabId "supporting_evidence" `
        -OverrideSummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" `
        -OverrideSummaryKind "minimal_kernel.runtime_evidence_bundle" `
        -OverrideSummaryPath $runtimeEvidenceSampleSummary `
        -OverrideReportMarkdownPath $runtimeEvidenceSampleReport `
        -OverrideCheckTextPath $runtimeEvidenceSampleCheck `
        -OverrideQueryKind "bringup_evidence" `
        -OverrideQueryScope "report" `
        -OverrideFollowupQueryKinds @("resource_summary", "default_overview", "cap_list")

    New-SyntheticLandingSummary `
        -SourceLandingPath $witnessLanding `
        -SyntheticLandingPath (Join-Path $syntheticRoot "runtime-session-sample.front-page.entry-landing.summary.json") `
        -SelectedTabId "runtime_session" `
        -OverrideSummarySchema "minimal_kernel.kernel_runtime_session/v0" `
        -OverrideSummaryKind "minimal_kernel.kernel_runtime_session" `
        -OverrideSummaryPath $runtimeSessionSampleSummary `
        -OverrideReportMarkdownPath $runtimeSessionSampleReport `
        -OverrideCheckTextPath $runtimeSessionSampleCheck `
        -OverrideQueryKind "bringup_evidence" `
        -OverrideQueryScope "report" `
        -OverrideFollowupQueryKinds @("resource_summary", "default_overview", "cap_list")

    $cases = @(
        [ordered]@{
            Name = "root-witness"
            Landing = Join-Path $landingRootPath "root-witness\front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "delivery_biography"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "report"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "biography_overview"
        },
        [ordered]@{
            Name = "root-world-compare"
            Landing = Join-Path $landingRootPath "root-world-compare\front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "counterfactual_verdict"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "world_compare_overview"
        },
        [ordered]@{
            Name = "root-witness-to-root-world-compare"
            Landing = Join-Path $landingRootPath "root-world-compare\front-page.entry-landing.summary.json"
            LandingCompare = Join-Path $landingCompareRootPath "root-witness-to-root-world-compare\front-page.entry-landing.compare.summary.json"
            ExpectedTab = "counterfactual_verdict"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $true
            ExpectedVerdict = "improved"
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "world_compare_overview"
        },
        [ordered]@{
            Name = "root-world-compare-to-root-witness"
            Landing = Join-Path $landingRootPath "root-witness\front-page.entry-landing.summary.json"
            LandingCompare = Join-Path $landingCompareRootPath "root-world-compare-to-root-witness\front-page.entry-landing.compare.summary.json"
            ExpectedTab = "delivery_biography"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "report"
            ExpectedCompareContext = $true
            ExpectedVerdict = "drifted"
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "biography_overview"
        },
        [ordered]@{
            Name = "witness-ci-shelf"
            Landing = Join-Path $landingRootPath "witness-ci-shelf\front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "grouped_review"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "world_shelf_review_overview"
            ExpectedSummaryLinePrefix = "drift_digest "
        },
        [ordered]@{
            Name = "review-provenance"
            Landing = Join-Path $landingRootPath "review-provenance\front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "grouped_review"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "world_shelf_review_overview"
            ExpectedSummaryLinePrefix = "drift_digest "
        },
        [ordered]@{
            Name = "root-witness-supporting-testimony"
            Landing = Join-Path $syntheticRoot "root-witness-supporting-testimony.front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "supporting_testimony"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "report"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "witness_bundle_overview"
        },
        [ordered]@{
            Name = "witness-ci-shelf-shelf-compare"
            Landing = Join-Path $syntheticRoot "witness-ci-shelf-shelf-compare.front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "shelf_compare"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "shelf_compare_overview"
        },
        [ordered]@{
            Name = "witness-ci-shelf-candidate-shelf"
            Landing = Join-Path $syntheticRoot "witness-ci-shelf-candidate-shelf.front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "candidate_shelf"
            ExpectedQueryKind = "default_overview"
            ExpectedQueryScope = "artifact_root"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "shelf_overview"
        },
        [ordered]@{
            Name = "runtime-evidence-sample"
            Landing = Join-Path $syntheticRoot "runtime-evidence-sample.front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "supporting_evidence"
            ExpectedQueryKind = "bringup_evidence"
            ExpectedQueryScope = "report"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "runtime_evidence_bundle_overview"
        },
        [ordered]@{
            Name = "runtime-session-sample"
            Landing = Join-Path $syntheticRoot "runtime-session-sample.front-page.entry-landing.summary.json"
            LandingCompare = ""
            ExpectedTab = "runtime_session"
            ExpectedQueryKind = "bringup_evidence"
            ExpectedQueryScope = "report"
            ExpectedCompareContext = $false
            ExpectedVerdict = ""
            ExpectedInspectorReady = $false
            ExpectedProjectionStatus = "available"
            ExpectedProjectionKind = "kernel_runtime_session_overview"
        }
    )

    foreach ($case in $cases) {
        if (-not (Test-Path $case.Landing)) {
            throw "entry landing summary not found for case '$($case.Name)': $($case.Landing)"
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingCompare) -and -not (Test-Path $case.LandingCompare)) {
            throw "entry landing compare summary not found for case '$($case.Name)': $($case.LandingCompare)"
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @($exportScript, "--landing", $case.Landing, "--output-root", $caseOutputRoot)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingCompare)) {
            $arguments += @("--landing-compare", $case.LandingCompare)
        }
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opener export failed for case '{0}'" -f $case.Name)

        $openerSummaryPath = Join-Path $caseOutputRoot "front-page.entry-opener.summary.json"
        Assert-Condition `
            -Condition (Test-Path $openerSummaryPath) `
            -Message ("front page entry opener summary missing for case '{0}'" -f $case.Name)

        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
            -FailureMessage ("front page entry opener validation failed for case '{0}'" -f $case.Name)

        $openerSummary = Load-JsonObject -Path $openerSummaryPath
        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.selected_tab_id -eq $case.ExpectedTab) `
            -Message ("case '{0}' expected selected tab '{1}' but got '{2}'" -f $case.Name, $case.ExpectedTab, $openerSummary.open_action.selected_tab_id)
        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.query_kind -eq $case.ExpectedQueryKind) `
            -Message ("case '{0}' expected query kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedQueryKind, $openerSummary.open_action.query_kind)
        Assert-Condition `
            -Condition ([string]$openerSummary.open_action.query_scope -eq $case.ExpectedQueryScope) `
            -Message ("case '{0}' expected query scope '{1}' but got '{2}'" -f $case.Name, $case.ExpectedQueryScope, $openerSummary.open_action.query_scope)
        Assert-Condition `
            -Condition ([bool]$openerSummary.compare_context.available -eq [bool]$case.ExpectedCompareContext) `
            -Message ("case '{0}' expected compare context '{1}' but got '{2}'" -f $case.Name, $case.ExpectedCompareContext, $openerSummary.compare_context.available)
        Assert-Condition `
            -Condition ([string]$openerSummary.compare_context.landing_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $openerSummary.compare_context.landing_verdict)
        Assert-Condition `
            -Condition ([bool]$openerSummary.inspector_invocation.ready -eq [bool]$case.ExpectedInspectorReady) `
            -Message ("case '{0}' expected inspector ready '{1}' but got '{2}'" -f $case.Name, $case.ExpectedInspectorReady, $openerSummary.inspector_invocation.ready)
        Assert-Condition `
            -Condition ([string]$openerSummary.opened_projection.status -eq [string]$case.ExpectedProjectionStatus) `
            -Message ("case '{0}' expected projection status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionStatus, $openerSummary.opened_projection.status)
        Assert-Condition `
            -Condition ([string]$openerSummary.opened_projection.projection_kind -eq [string]$case.ExpectedProjectionKind) `
            -Message ("case '{0}' expected projection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProjectionKind, $openerSummary.opened_projection.projection_kind)
        Assert-OpeningReason -OpenerSummary $openerSummary -CaseName $case.Name
        Assert-Condition `
            -Condition (@($openerSummary.opened_projection.summary_lines).Count -gt 0) `
            -Message ("case '{0}' opened projection must expose summary lines" -f $case.Name)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ExpectedSummaryLinePrefix)) {
            $matchingSummaryLines = @(
                @($openerSummary.opened_projection.summary_lines) |
                    Where-Object { ([string]$_).StartsWith([string]$case.ExpectedSummaryLinePrefix, [System.StringComparison]::Ordinal) }
            )
            Assert-Condition `
                -Condition ($matchingSummaryLines.Count -gt 0) `
                -Message ("case '{0}' expected summary line prefix '{1}'" -f $case.Name, $case.ExpectedSummaryLinePrefix)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENER-SMOKE] case={0} tab={1} query={2}/{3} compare={4}/{5} inspector_ready={6} projection={7}/{8}" -f
            $case.Name,
            [string]$openerSummary.open_action.selected_tab_id,
            [string]$openerSummary.open_action.query_kind,
            [string]$openerSummary.open_action.query_scope,
            [bool]$openerSummary.compare_context.available,
            [string]$openerSummary.compare_context.landing_verdict,
            [bool]$openerSummary.inspector_invocation.ready,
            [string]$openerSummary.opened_projection.status,
            [string]$openerSummary.opened_projection.projection_kind
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-SMOKE] output_root={0}" -f $outputRootPath)
