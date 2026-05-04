param(
    [string]$LandingRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-smoke-landing",
    [string]$LandingCompareRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-smoke-landing-compare",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opener-workspace-smoke",
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

function Assert-CleanPath {
    param(
        [string]$Path,
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $RootPath
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $rootPrefix = $resolvedRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if ($resolvedPath.Equals($resolvedRoot, $comparison)) {
        throw "refusing to clean repo root: $resolvedPath"
    }
    if (-not $resolvedPath.StartsWith($rootPrefix, $comparison)) {
        throw "refusing to clean outside repo root: $resolvedPath"
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
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
        [string]$RouteId,
        [string]$SurfaceId,
        [string]$Label,
        [string]$Role,
        [string]$SummarySchema,
        [string]$SummaryKind,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath,
        [int]$Depth = 0,
        [int]$SupportingSurfaceCount = 0
    )

    return [ordered]@{
        route_id = $RouteId
        depth = $Depth
        surface_id = $SurfaceId
        label = $Label
        role = $Role
        summary_schema = $SummarySchema
        summary_kind = $SummaryKind
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        revisit = $false
        cycle = $false
        expanded = $true
        route_provenance_count = 1
        supporting_surface_count = $SupportingSurfaceCount
    }
}

function New-QueryHint {
    param(
        [string]$TabId,
        [string]$TabTitle,
        [string]$EntryRole,
        [string]$SummarySchema,
        [string]$SummaryKind,
        [string]$Scope,
        [string]$SelectionRule,
        [string]$QueryKind,
        [bool]$CompareExpected,
        [string[]]$FollowupQueryKinds,
        [string]$Rationale
    )

    return [ordered]@{
        tab_id = $TabId
        tab_title = $TabTitle
        entry_role = $EntryRole
        summary_schema = $SummarySchema
        summary_kind = $SummaryKind
        scope = $Scope
        selection_rule = $SelectionRule
        query_kind = $QueryKind
        compare_expected = $CompareExpected
        followup_query_kinds = @($FollowupQueryKinds)
        rationale = $Rationale
    }
}

function New-LandingTab {
    param(
        [string]$TabId,
        [string]$Title,
        [string[]]$CapabilityIds,
        $Entry
    )

    return [ordered]@{
        tab_id = $TabId
        title = $Title
        capability_ids = @($CapabilityIds)
        entry = $Entry
    }
}

function New-RouteProvenanceEntry {
    param(
        [string]$Id,
        [string]$RouteKind,
        [string]$SourceSummarySchema,
        [string]$SourceSummaryPath,
        [string]$SourceReportPath,
        [string]$SourceCheckPath,
        [string[]]$SurfaceIds
    )

    return [ordered]@{
        id = $Id
        route_kind = $RouteKind
        source_summary_schema = $SourceSummarySchema
        source_summary_path = $SourceSummaryPath
        source_input_summary_path = $SourceSummaryPath
        source_root_summary_path = $SourceSummaryPath
        source_report_markdown_path = $SourceReportPath
        source_check_text_path = $SourceCheckPath
        level1_surface_ids = @($SurfaceIds)
    }
}

function New-FrontPageSurface {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Role,
        [string]$SummarySchema,
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath
    )

    return [ordered]@{
        id = $Id
        label = $Label
        role = $Role
        summary_schema = $SummarySchema
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
    }
}

function New-MinimalLandingSummary {
    param(
        [string]$WorkspaceRoot,
        [string]$CaseName,
        [string]$TargetSummaryPath,
        [string]$TargetReportPath,
        [string]$TargetCheckPath,
        [bool]$CompareExpected,
        [bool]$AddSecondaryTab
    )

    $landingDir = Join-Path $WorkspaceRoot "landing"
    Ensure-Directory -Path $landingDir

    $landingPath = Resolve-FullPath -Path (Join-Path $landingDir "front-page.entry-landing.summary.json")
    $landingReportPath = Resolve-FullPath -Path (Join-Path $landingDir "front-page.entry-landing.report.md")
    $landingCheckPath = Resolve-FullPath -Path (Join-Path $landingDir "front-page.entry-landing.check.txt")
    $inputCapabilityPath = Resolve-FullPath -Path (Join-Path $landingDir "front-page.entry-capability.summary.json")

    Write-TextFile -Path $landingReportPath -Content ("# Synthetic Runtime Evidence Landing`n`ncase={0}`n" -f $CaseName)
    Write-TextFile -Path $landingCheckPath -Content ("synthetic runtime evidence landing: {0}`n" -f $CaseName)
    Write-JsonFile `
        -Path $inputCapabilityPath `
        -Value ([ordered]@{
            schema = "synthetic.front_page_entry_capability/v0"
            kind = "synthetic.front_page_entry_capability"
            case = $CaseName
        })

    $targetSummaryPath = Resolve-FullPath -Path $TargetSummaryPath
    $targetReportPath = Resolve-FullPath -Path $TargetReportPath
    $targetCheckPath = Resolve-FullPath -Path $TargetCheckPath
    $outputRoot = Resolve-FullPath -Path $WorkspaceRoot
    $summarySchema = "minimal_kernel.runtime_evidence_bundle.summary/v1"
    $summaryKind = "minimal_kernel.runtime_evidence_bundle"

    $primaryEntry = New-EntryRef `
        -RouteId "runtime-evidence-route" `
        -SurfaceId "runtime_evidence" `
        -Label "Minimal kernel runtime evidence" `
        -Role "supporting_evidence" `
        -SummarySchema $summarySchema `
        -SummaryKind $summaryKind `
        -SummaryPath $targetSummaryPath `
        -ReportPath $targetReportPath `
        -CheckPath $targetCheckPath `
        -SupportingSurfaceCount ($(if ($AddSecondaryTab) { 1 } else { 0 }))
    $primaryTab = New-LandingTab `
        -TabId "supporting_evidence" `
        -Title "Runtime Evidence" `
        -CapabilityIds @("minimal_kernel.runtime_evidence") `
        -Entry $primaryEntry

    $primaryFollowups = if ($CompareExpected) {
        @("evidence_refs", "landing_compare")
    } else {
        @("evidence_refs")
    }
    $primaryQuery = New-QueryHint `
        -TabId "supporting_evidence" `
        -TabTitle "Runtime Evidence" `
        -EntryRole "supporting_evidence" `
        -SummarySchema $summarySchema `
        -SummaryKind $summaryKind `
        -Scope "report" `
        -SelectionRule "single_report" `
        -QueryKind "default_overview" `
        -CompareExpected $CompareExpected `
        -FollowupQueryKinds $primaryFollowups `
        -Rationale "Open the runtime evidence bundle as the first explainable upper-half target."

    $secondaryLandings = @()
    $landingTabs = @($primaryTab)
    $tabQueries = @($primaryQuery)
    $routeProvenance = @(
        New-RouteProvenanceEntry `
            -Id "runtime-evidence-route" `
            -RouteKind "synthetic_runtime_evidence" `
            -SourceSummarySchema $summarySchema `
            -SourceSummaryPath $targetSummaryPath `
            -SourceReportPath $targetReportPath `
            -SourceCheckPath $targetCheckPath `
            -SurfaceIds @("runtime_evidence")
    )
    $supportingSurfaces = @(
        New-FrontPageSurface `
            -Id "runtime_evidence" `
            -Label "runtime evidence" `
            -Role "supporting_evidence" `
            -SummarySchema $summarySchema `
            -SummaryPath $targetSummaryPath `
            -ReportPath $targetReportPath `
            -CheckPath $targetCheckPath
    )
    $availableTabIds = @("supporting_evidence")
    $ownerRouteIds = @("runtime-evidence-route")
    $ownerSurfaceIds = @("runtime_evidence")

    if ($AddSecondaryTab) {
        $secondaryEntry = New-EntryRef `
            -RouteId "runtime-check-route" `
            -SurfaceId "runtime_check" `
            -Label "Runtime evidence check" `
            -Role "supporting_testimony" `
            -SummarySchema $summarySchema `
            -SummaryKind $summaryKind `
            -SummaryPath $targetSummaryPath `
            -ReportPath $targetReportPath `
            -CheckPath $targetCheckPath
        $secondaryTab = New-LandingTab `
            -TabId "runtime_check" `
            -Title "Runtime Check" `
            -CapabilityIds @("minimal_kernel.runtime_evidence.check") `
            -Entry $secondaryEntry
        $secondaryQuery = New-QueryHint `
            -TabId "runtime_check" `
            -TabTitle "Runtime Check" `
            -EntryRole "supporting_testimony" `
            -SummarySchema $summarySchema `
            -SummaryKind $summaryKind `
            -Scope "report" `
            -SelectionRule "single_report" `
            -QueryKind "default_overview" `
            -CompareExpected $false `
            -FollowupQueryKinds @("check_text") `
            -Rationale "Keep the check artifact reachable as a secondary explain tab."

        $secondaryLandings = @($secondaryTab)
        $landingTabs = @($primaryTab, $secondaryTab)
        $tabQueries = @($primaryQuery, $secondaryQuery)
        $routeProvenance += New-RouteProvenanceEntry `
            -Id "runtime-check-route" `
            -RouteKind "synthetic_runtime_check" `
            -SourceSummarySchema $summarySchema `
            -SourceSummaryPath $targetSummaryPath `
            -SourceReportPath $targetReportPath `
            -SourceCheckPath $targetCheckPath `
            -SurfaceIds @("runtime_check")
        $supportingSurfaces += New-FrontPageSurface `
            -Id "runtime_check" `
            -Label "runtime check" `
            -Role "supporting_testimony" `
            -SummarySchema $summarySchema `
            -SummaryPath $targetSummaryPath `
            -ReportPath $targetReportPath `
            -CheckPath $targetCheckPath
        $availableTabIds = @("supporting_evidence", "runtime_check")
        $ownerRouteIds = @("runtime-evidence-route", "runtime-check-route")
        $ownerSurfaceIds = @("runtime_evidence", "runtime_check")
    }

    $openingReason = [ordered]@{
        kind = "supporting_evidence"
        summary = if ($CompareExpected) {
            "Runtime evidence remains the primary opening target, now with compare-aware follow-up context."
        } else {
            "Runtime evidence is the nearest stable testimony for the minimal kernel world."
        }
        source_summary_path = $targetSummaryPath
        drift_changed = $CompareExpected
        drift_verdict = if ($CompareExpected) { "improved" } else { "" }
    }

    $summary = [ordered]@{
        schema = "system_compiler.front_page_entry_landing/v0"
        kind = "system_compiler.front_page_entry_landing"
        generated_at_utc = "2026-05-05T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opener_workspace_smoke.ps1"
        result = "ok"
        entry_landing = [ordered]@{
            title = "Synthetic Runtime Evidence Landing"
            summary = "A self-contained landing fixture for the opener workspace facade."
        }
        front_page = [ordered]@{
            summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
            supporting_surfaces = $supportingSurfaces
        }
        route_provenance = $routeProvenance
        artifact_context = [ordered]@{
            input_capability_summary_path = $inputCapabilityPath
            output_root = $outputRoot
            landing_summary_path = $landingPath
            report_markdown_path = $landingReportPath
            check_text_path = $landingCheckPath
        }
        root_surface = [ordered]@{
            surface_id = "runtime_evidence"
            label = "Minimal kernel runtime evidence"
            role = "supporting_evidence"
            summary_schema = $summarySchema
            summary_kind = $summaryKind
            summary_path = $targetSummaryPath
        }
        landing_status = [ordered]@{
            landing_result = "ok"
            recommended_entry_mode = "evidence"
            entry_tier = "evidence_only"
            opening_reason = $openingReason
            primary_tab_id = "supporting_evidence"
            primary_summary_schema = $summarySchema
            primary_summary_kind = $summaryKind
            available_tab_ids = $availableTabIds
            fallback_tab_ids = @()
            tab_count = $landingTabs.Count
            fallback_tab_count = 0
            provenance_root_count = 1
            route_provenance_entry_count = $routeProvenance.Count
            direct_review_available = $false
            direct_compare_available = $false
            direct_biography_available = $false
            direct_evidence_available = $true
        }
        fallback_mode_order = @("evidence", "route")
        primary_landing = $primaryTab
        secondary_landings = $secondaryLandings
        landing_tabs = $landingTabs
        provenance_roots = @(
            [ordered]@{
                root_id = "runtime_evidence"
                root_kind = "runtime_evidence_bundle"
                source_summary_schema = $summarySchema
                source_summary_path = $targetSummaryPath
                source_front_page_summary_path = ""
                owner_route_ids = $ownerRouteIds
                owner_surface_ids = $ownerSurfaceIds
                available_supporting_surface_ids = $ownerSurfaceIds
            }
        )
        query_hints = [ordered]@{
            primary_query = $primaryQuery
            tab_queries = $tabQueries
        }
        questions = [ordered]@{
            compare_questions = @("Does the opener preserve the runtime evidence landing decision?")
            next_questions = @("Should the workspace facade render this runtime evidence preview first?")
        }
        violations = @()
    }

    Write-JsonFile -Path $landingPath -Value $summary
    return $landingPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$landingRootPath = Resolve-FullPath -Path $LandingRoot
$landingCompareRootPath = Resolve-FullPath -Path $LandingCompareRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    foreach ($path in @($landingRootPath, $landingCompareRootPath, $outputRootPath)) {
        Assert-CleanPath -Path $path -RootPath $repoRoot
    }
    foreach ($path in @($landingRootPath, $landingCompareRootPath, $outputRootPath)) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $landingRootPath
Ensure-Directory -Path $landingCompareRootPath
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$targetSamplePath = Join-Path $repoRoot "schemas\examples\minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_landing.py"
$compareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing_compare.py"
$workspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener_workspace.ps1"
foreach ($requiredPath in @($targetSamplePath, $compareScript, $compareValidateScript, $workspaceScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $targetRoot = Join-Path $landingRootPath "_target"
    Ensure-Directory -Path $targetRoot
    $targetSummaryPath = Join-Path $targetRoot "minimal-kernel-runtime-evidence.summary.json"
    $targetReportPath = Join-Path $targetRoot "minimal-kernel-runtime-evidence.report.md"
    $targetCheckPath = Join-Path $targetRoot "minimal-kernel-runtime-evidence.check.txt"
    Copy-Item -LiteralPath $targetSamplePath -Destination $targetSummaryPath -Force
    Write-TextFile -Path $targetReportPath -Content "# Synthetic Minimal Kernel Runtime Evidence Report`n"
    Write-TextFile -Path $targetCheckPath -Content "synthetic minimal kernel runtime evidence check`n"

    $coldWorkspaceRoot = Join-Path $landingRootPath "cold-runtime-evidence"
    $hotWorkspaceRoot = Join-Path $landingRootPath "hot-runtime-evidence"
    $coldLandingPath = New-MinimalLandingSummary `
        -WorkspaceRoot $coldWorkspaceRoot `
        -CaseName "cold-runtime-evidence" `
        -TargetSummaryPath $targetSummaryPath `
        -TargetReportPath $targetReportPath `
        -TargetCheckPath $targetCheckPath `
        -CompareExpected $false `
        -AddSecondaryTab $false
    $hotLandingPath = New-MinimalLandingSummary `
        -WorkspaceRoot $hotWorkspaceRoot `
        -CaseName "hot-runtime-evidence" `
        -TargetSummaryPath $targetSummaryPath `
        -TargetReportPath $targetReportPath `
        -TargetCheckPath $targetCheckPath `
        -CompareExpected $true `
        -AddSecondaryTab $true

    $compareWorkspaceRoot = Join-Path $landingCompareRootPath "cold-to-hot-runtime-evidence"
    $compareOutputRoot = Join-Path $compareWorkspaceRoot "landing-compare"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $coldLandingPath,
            "--candidate",
            $hotLandingPath,
            "--output-root",
            $compareOutputRoot
        ) `
        -FailureMessage "front page entry landing compare fixture export failed"

    $landingCompareSummaryPath = Join-Path $compareOutputRoot "front-page.entry-landing.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareValidateScript, "--summary", $landingCompareSummaryPath) `
        -FailureMessage "front page entry landing compare fixture validation failed"

    $cases = @(
        [ordered]@{
            Name = "cold-runtime-evidence"
            LandingSummaryPath = $coldLandingPath
            LandingWorkspaceRoot = ""
            LandingCompareSummaryPath = ""
            LandingCompareWorkspaceRoot = ""
            ExpectedCompareContext = $false
            ExpectedCompareExpected = $false
            ExpectedVerdict = ""
            ExpectedPrimaryQueryChanged = $false
        },
        [ordered]@{
            Name = "hot-runtime-evidence-with-landing-compare"
            LandingSummaryPath = ""
            LandingWorkspaceRoot = $hotWorkspaceRoot
            LandingCompareSummaryPath = ""
            LandingCompareWorkspaceRoot = $compareWorkspaceRoot
            ExpectedCompareContext = $true
            ExpectedCompareExpected = $true
            ExpectedVerdict = "improved"
            ExpectedPrimaryQueryChanged = $true
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        $arguments = @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $workspaceScript,
            "-OutputRoot",
            $caseOutputRoot,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        )
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingSummaryPath)) {
            $arguments += @("-LandingSummaryPath", [string]$case.LandingSummaryPath)
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingWorkspaceRoot)) {
            $arguments += @("-LandingWorkspaceRoot", [string]$case.LandingWorkspaceRoot)
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingCompareSummaryPath)) {
            $arguments += @("-LandingCompareSummaryPath", [string]$case.LandingCompareSummaryPath)
        }
        if (-not [string]::IsNullOrWhiteSpace([string]$case.LandingCompareWorkspaceRoot)) {
            $arguments += @("-LandingCompareWorkspaceRoot", [string]$case.LandingCompareWorkspaceRoot)
        }

        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList $arguments `
            -FailureMessage ("front page entry opener workspace export failed for case '{0}'" -f $case.Name)

        $openerSummaryPath = Join-Path $caseOutputRoot "opener\front-page.entry-opener.summary.json"
        $summary = Load-JsonObject -Path $openerSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.open_action.status -eq "ready") `
            -Message ("case '{0}' expected open action ready but got '{1}'" -f $case.Name, $summary.open_action.status)
        Assert-Condition `
            -Condition ([string]$summary.open_action.selected_tab_id -eq "supporting_evidence") `
            -Message ("case '{0}' expected supporting_evidence tab but got '{1}'" -f $case.Name, $summary.open_action.selected_tab_id)
        Assert-Condition `
            -Condition ([bool]$summary.open_action.compare_expected -eq [bool]$case.ExpectedCompareExpected) `
            -Message ("case '{0}' compare_expected expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.opened_projection.status -eq "available") `
            -Message ("case '{0}' expected available projection but got '{1}'" -f $case.Name, $summary.opened_projection.status)
        Assert-Condition `
            -Condition ([string]$summary.opened_projection.projection_kind -eq "runtime_evidence_bundle_overview") `
            -Message ("case '{0}' expected runtime_evidence_bundle_overview but got '{1}'" -f $case.Name, $summary.opened_projection.projection_kind)
        Assert-Condition `
            -Condition ([bool]$summary.compare_context.available -eq [bool]$case.ExpectedCompareContext) `
            -Message ("case '{0}' compare context expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$summary.compare_context.landing_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected landing verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.compare_context.landing_verdict)
        Assert-Condition `
            -Condition ([bool]$summary.compare_context.primary_query_changed -eq [bool]$case.ExpectedPrimaryQueryChanged) `
            -Message ("case '{0}' primary query change expectation mismatch" -f $case.Name)
        Assert-Condition `
            -Condition ([bool]$summary.inspector_invocation.ready -eq $false) `
            -Message ("case '{0}' expected blocked inspector invocation for non-artifact-report target" -f $case.Name)
        Assert-Condition `
            -Condition (@($summary.opened_projection.summary_lines).Count -gt 0) `
            -Message ("case '{0}' expected projection summary lines" -f $case.Name)

        if ([bool]$case.ExpectedCompareContext) {
            Assert-Condition `
                -Condition ([string]$summary.source_landing_compare.related_landing_role -eq "candidate_landing") `
                -Message ("case '{0}' expected candidate_landing compare relation" -f $case.Name)
            Assert-Condition `
                -Condition ([string]$summary.artifact_context.source_landing_compare_summary_path -eq (Resolve-FullPath -Path $landingCompareSummaryPath)) `
                -Message ("case '{0}' expected compare summary path to be preserved" -f $case.Name)
        } else {
            Assert-Condition `
                -Condition ($null -eq $summary.source_landing_compare) `
                -Message ("case '{0}' expected no source landing compare projection" -f $case.Name)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-SMOKE] case={0} tab={1} projection={2}/{3} compare={4}/{5} inspector_ready={6}" -f
            $case.Name,
            [string]$summary.open_action.selected_tab_id,
            [string]$summary.opened_projection.status,
            [string]$summary.opened_projection.projection_kind,
            [bool]$summary.compare_context.available,
            [string]$summary.compare_context.landing_verdict,
            [bool]$summary.inspector_invocation.ready
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-SMOKE] landing_root={0}" -f $landingRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-SMOKE] landing_compare_root={0}" -f $landingCompareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-SMOKE] output_root={0}" -f $outputRootPath)
