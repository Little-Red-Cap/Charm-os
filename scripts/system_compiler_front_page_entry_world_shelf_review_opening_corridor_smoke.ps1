param(
    [string]$FrontPageRouteRoot = "cmake-build-system-compiler-front-page-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-world-shelf-review-opening-corridor-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param([string]$Path)

    if (-not [string]::IsNullOrWhiteSpace($Path) -and -not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param([string]$Path)

    if (-not [string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path)) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Resolve-ToolPath {
    param([string[]]$Candidates)

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
    param([string]$Path)

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

    $json = $Value | ConvertTo-Json -Depth 100
    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($json + [Environment]::NewLine)
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

    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($Content + [Environment]::NewLine)
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

function New-PlaceholderSurface {
    param(
        [string]$SurfaceRootPath,
        [string]$SurfaceId,
        [string]$SummarySchema,
        [string]$SummaryKind
    )

    $summaryPath = Join-Path $SurfaceRootPath ("{0}.summary.json" -f $SurfaceId)
    $reportPath = Join-Path $SurfaceRootPath ("{0}.report.md" -f $SurfaceId)
    $checkPath = Join-Path $SurfaceRootPath ("{0}.check.txt" -f $SurfaceId)

    Write-JsonFile -Path $summaryPath -Value ([ordered]@{
        schema = $SummarySchema
        kind = $SummaryKind
    })
    Write-TextFile -Path $reportPath -Content ("# {0}" -f $SurfaceId)
    Write-TextFile -Path $checkPath -Content $SurfaceId

    return [ordered]@{
        id = $SurfaceId
        label = $SurfaceId
        role = $SurfaceId
        summary_schema = $SummarySchema
        summary_path = (Resolve-FullPath -Path $summaryPath)
        report_markdown_path = (Resolve-FullPath -Path $reportPath)
        check_text_path = (Resolve-FullPath -Path $checkPath)
    }
}

function New-WorldShelfReviewRootFixture {
    param([string]$OutputRootPath)

    Ensure-Directory -Path $OutputRootPath
    $surfaceRootPath = Join-Path $OutputRootPath "surfaces"
    Ensure-Directory -Path $surfaceRootPath

    $supportingSurfaces = [System.Collections.Generic.List[object]]::new()
    foreach ($surfaceSpec in @(
        [ordered]@{ SurfaceId = "world_shelf_review"; SummarySchema = "system_compiler.world_shelf_review/v0"; SummaryKind = "system_compiler.world_shelf_review" },
        [ordered]@{ SurfaceId = "candidate_shelf"; SummarySchema = "system_compiler.biography_index/v0"; SummaryKind = "system_compiler.biography_index" },
        [ordered]@{ SurfaceId = "shelf_compare"; SummarySchema = "system_compiler.biography_index_compare/v0"; SummaryKind = "system_compiler.biography_index_compare" },
        [ordered]@{ SurfaceId = "baseline_shelf"; SummarySchema = "system_compiler.biography_index/v0"; SummaryKind = "system_compiler.biography_index" }
    )) {
        $supportingSurfaces.Add(
            (New-PlaceholderSurface `
                -SurfaceRootPath $surfaceRootPath `
                -SurfaceId ([string]$surfaceSpec.SurfaceId) `
                -SummarySchema ([string]$surfaceSpec.SummarySchema) `
                -SummaryKind ([string]$surfaceSpec.SummaryKind))
        ) | Out-Null
    }

    $summaryPath = Join-Path $OutputRootPath "world-shelf.review.summary.json"
    $reportPath = Join-Path $OutputRootPath "world-shelf.review.md"
    $checkPath = Join-Path $OutputRootPath "world-shelf.check.txt"
    $supportingSurfacesArray = @($supportingSurfaces)
    $surfaceIds = @($supportingSurfacesArray | ForEach-Object { [string]$_.id })
    $generatedAtUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")

    $rootSummary = [ordered]@{
        schema = "system_compiler.world_shelf_review/v0"
        kind = "system_compiler.world_shelf_review"
        generated_at_utc = $generatedAtUtc
        generator = "scripts/system_compiler_front_page_entry_world_shelf_review_opening_corridor_smoke.ps1"
        result = "ok"
        review_verdict = "standing"
        review = [ordered]@{
            title = "System Compiler World Shelf Review"
            summary = "Synthetic review fixture for opening corridor smoke."
        }
        front_page = [ordered]@{
            summary_path = (Resolve-FullPath -Path $summaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reportPath)
            check_text_path = (Resolve-FullPath -Path $checkPath)
            supporting_surfaces = $supportingSurfacesArray
        }
        artifact_context = [ordered]@{
            output_root = (Resolve-FullPath -Path $OutputRootPath)
            review_summary_path = (Resolve-FullPath -Path $summaryPath)
            review_report_markdown_path = (Resolve-FullPath -Path $reportPath)
            review_check_text_path = (Resolve-FullPath -Path $checkPath)
        }
        route_provenance = @(
            [ordered]@{
                id = "world_shelf_review"
                route_kind = "front_page_root"
                source_summary_schema = "system_compiler.world_shelf_review/v0"
                source_summary_path = (Resolve-FullPath -Path $summaryPath)
                source_front_page_summary_path = (Resolve-FullPath -Path $summaryPath)
                source_front_page_report_markdown_path = (Resolve-FullPath -Path $reportPath)
                source_front_page_check_text_path = (Resolve-FullPath -Path $checkPath)
                available_supporting_surface_ids = $surfaceIds
            }
        )
    }

    Write-JsonFile -Path $summaryPath -Value $rootSummary
    Write-TextFile -Path $reportPath -Content "# System Compiler World Shelf Review"
    Write-TextFile -Path $checkPath -Content ("world_shelf_review_root: {0}" -f (Resolve-FullPath -Path $summaryPath))
    return (Resolve-FullPath -Path $summaryPath)
}

function Export-RouteFixture {
    param(
        [string]$SummaryPath,
        [string]$OutputRootPath,
        [string]$RouteSummaryPath,
        [string]$PythonExePath,
        [string]$FailureMessage
    )

    $routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
    $routeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
    foreach ($requiredPath in @($routeExportScript, $routeValidateScript)) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "missing path: $requiredPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $PythonExePath `
        -ArgumentList @(
            $routeExportScript,
            "--summary",
            $SummaryPath,
            "--output-root",
            $OutputRootPath
        ) `
        -FailureMessage $FailureMessage

    Invoke-ExternalTool `
        -Executable $PythonExePath `
        -ArgumentList @(
            $routeValidateScript,
            "--summary",
            $RouteSummaryPath
        ) `
        -FailureMessage ($FailureMessage + " validation failed")
}

function Ensure-WorldShelfReviewRouteSummary {
    param(
        [string]$FrontPageRouteRootPath,
        [string]$PythonExePath
    )

    $routeOutputRoot = Join-Path $FrontPageRouteRootPath "witness-ci-shelf"
    $routeSummaryPath = Join-Path $routeOutputRoot "front-page.route.summary.json"
    if (Test-Path -LiteralPath $routeSummaryPath) {
        return (Resolve-FullPath -Path $routeSummaryPath)
    }

    $fixtureRootPath = Join-Path $FrontPageRouteRootPath "_synthetic-world-shelf-review-root"
    $summaryPath = New-WorldShelfReviewRootFixture -OutputRootPath $fixtureRootPath
    Export-RouteFixture `
        -SummaryPath $summaryPath `
        -OutputRootPath $routeOutputRoot `
        -RouteSummaryPath $routeSummaryPath `
        -PythonExePath $PythonExePath `
        -FailureMessage "world shelf review route export failed for synthetic fixture"
    return (Resolve-FullPath -Path $routeSummaryPath)
}

function New-BlockedRouteFixture {
    param(
        [string]$SourceRoutePath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceRoutePath
    $summary.route_entries = @(
        @($summary.route_entries) |
            Where-Object { -not ([int]$_.depth -eq 1 -and [string]$_.surface_id -eq "world_shelf_review") }
    )
    if ($summary.artifact_context -is [System.Management.Automation.PSCustomObject] -or $summary.artifact_context -is [hashtable]) {
        $summary.artifact_context.route_summary_path = (Resolve-FullPath -Path $OutputPath)
    }
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

function New-RouteSummaryCopy {
    param(
        [string]$SourceRoutePath,
        [string]$OutputPath,
        [scriptblock]$Mutator
    )

    $summary = Load-JsonObject -Path $SourceRoutePath
    if ($null -ne $Mutator) {
        & $Mutator $summary
    }
    if ($summary.artifact_context -is [System.Management.Automation.PSCustomObject] -or $summary.artifact_context -is [hashtable]) {
        $summary.artifact_context.route_summary_path = (Resolve-FullPath -Path $OutputPath)
    }
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

function Export-RouteCompareFixture {
    param(
        [string]$BaselineRoutePath,
        [string]$CandidateRoutePath,
        [string]$OutputRootPath,
        [string]$PythonExePath,
        [string]$Label
    )

    Invoke-ExternalTool `
        -Executable $PythonExePath `
        -ArgumentList @(
            $routeCompareScript,
            "--baseline",
            $BaselineRoutePath,
            "--candidate",
            $CandidateRoutePath,
            "--output-root",
            $OutputRootPath
        ) `
        -FailureMessage ("{0} export failed" -f $Label) | Out-Null

    $compareSummaryPath = Join-Path $OutputRootPath "front-page.route.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $PythonExePath `
        -ArgumentList @(
            $routeCompareValidateScript,
            "--summary",
            $compareSummaryPath
        ) `
        -FailureMessage ("{0} validation failed" -f $Label) | Out-Null

    return (Resolve-FullPath -Path $compareSummaryPath)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageRouteRootPath = Resolve-FullPath -Path $FrontPageRouteRoot
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

$routeCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$routeCompareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route_compare.py"
$explainEntryExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_world_shelf_review_explain_entry.py"
$explainEntryValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_world_shelf_review_explain_entry.py"
$handoffExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_world_shelf_review_explain_entry_handoff.py"
$handoffValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_world_shelf_review_explain_entry_handoff.py"
foreach ($requiredPath in @($routeCompareScript, $routeCompareValidateScript, $explainEntryExportScript, $explainEntryValidateScript, $handoffExportScript, $handoffValidateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanRoutePath = Ensure-WorldShelfReviewRouteSummary -FrontPageRouteRootPath $frontPageRouteRootPath -PythonExePath $resolvedPythonExe
    $cleanRouteSummary = Load-JsonObject -Path $cleanRoutePath
    Assert-Condition `
        -Condition ([string]$cleanRouteSummary.root_surface.summary_schema -eq "system_compiler.world_shelf_review/v0") `
        -Message ("expected route root schema system_compiler.world_shelf_review/v0 but got '{0}'" -f $cleanRouteSummary.root_surface.summary_schema)

    $level1SurfaceIds = @(
        @($cleanRouteSummary.route_entries) |
            Where-Object { [int]$_.depth -eq 1 } |
            ForEach-Object { [string]$_.surface_id }
    )
    foreach ($requiredSurfaceId in @("world_shelf_review", "candidate_shelf", "shelf_compare", "baseline_shelf")) {
        Assert-Condition `
            -Condition ($level1SurfaceIds -contains [string]$requiredSurfaceId) `
            -Message ("expected route level1 surface '{0}'" -f $requiredSurfaceId)
    }

    $blockedRoutePath = New-BlockedRouteFixture `
        -SourceRoutePath $cleanRoutePath `
        -OutputPath (Join-Path $outputRootPath "_blocked-fixtures\missing-world-shelf-review-surface\front-page.route.summary.json")

    $compareFixtureRootPath = Join-Path $outputRootPath "_route-compare-fixtures"
    Ensure-Directory -Path $compareFixtureRootPath

    $driftedSurface = New-PlaceholderSurface `
        -SurfaceRootPath (Join-Path $compareFixtureRootPath "drifted-surface") `
        -SurfaceId "world_shelf_review_drifted" `
        -SummarySchema "system_compiler.world_shelf_review/v0" `
        -SummaryKind "system_compiler.world_shelf_review"

    $worldCompareSurface = New-PlaceholderSurface `
        -SurfaceRootPath (Join-Path $compareFixtureRootPath "improved-surface") `
        -SurfaceId "world_compare" `
        -SummarySchema "system_compiler.world_compare/v0" `
        -SummaryKind "system_compiler.world_compare"

    $driftedRoutePath = New-RouteSummaryCopy `
        -SourceRoutePath $cleanRoutePath `
        -OutputPath (Join-Path $compareFixtureRootPath "drifted-route\front-page.route.summary.json") `
        -Mutator {
            param($summary)
            foreach ($entry in @($summary.route_entries)) {
                if ([int]$entry.depth -eq 1 -and [string]$entry.surface_id -eq "world_shelf_review") {
                    $entry.summary_path = $driftedSurface.summary_path
                    $entry.report_markdown_path = $driftedSurface.report_markdown_path
                    $entry.check_text_path = $driftedSurface.check_text_path
                }
            }
        }

    $improvedRoutePath = New-RouteSummaryCopy `
        -SourceRoutePath $cleanRoutePath `
        -OutputPath (Join-Path $compareFixtureRootPath "improved-route\front-page.route.summary.json") `
        -Mutator {
            param($summary)
            $summary.route_entries = @($summary.route_entries) + @(
                [ordered]@{
                    route_id = "root/0/0"
                    parent_route_id = "root/0"
                    depth = 2
                    surface_id = "world_compare"
                    label = "world_compare"
                    role = "world_compare"
                    declared_summary_schema = "system_compiler.world_compare/v0"
                    summary_schema = "system_compiler.world_compare/v0"
                    summary_kind = "system_compiler.world_compare"
                    summary_path = $worldCompareSurface.summary_path
                    report_markdown_path = $worldCompareSurface.report_markdown_path
                    check_text_path = $worldCompareSurface.check_text_path
                    route_provenance_count = 0
                    supporting_surface_count = 0
                    revisit = $false
                    cycle = $false
                    first_route_id = "root/0/0"
                    expanded = $false
                }
            )
        }

    $collapsedRoutePath = New-RouteSummaryCopy `
        -SourceRoutePath $cleanRoutePath `
        -OutputPath (Join-Path $compareFixtureRootPath "collapsed-route\front-page.route.summary.json") `
        -Mutator {
            param($summary)
            $summary.result = "fail"
        }

    $standingComparePath = Export-RouteCompareFixture `
        -BaselineRoutePath $cleanRoutePath `
        -CandidateRoutePath $cleanRoutePath `
        -OutputRootPath (Join-Path $outputRootPath "route-compare-standing") `
        -PythonExePath $resolvedPythonExe `
        -Label "world shelf review standing route compare"

    $driftedComparePath = Export-RouteCompareFixture `
        -BaselineRoutePath $cleanRoutePath `
        -CandidateRoutePath $driftedRoutePath `
        -OutputRootPath (Join-Path $outputRootPath "route-compare-drifted") `
        -PythonExePath $resolvedPythonExe `
        -Label "world shelf review drifted route compare"

    $improvedComparePath = Export-RouteCompareFixture `
        -BaselineRoutePath $cleanRoutePath `
        -CandidateRoutePath $improvedRoutePath `
        -OutputRootPath (Join-Path $outputRootPath "route-compare-improved") `
        -PythonExePath $resolvedPythonExe `
        -Label "world shelf review improved route compare"

    $collapsedComparePath = Export-RouteCompareFixture `
        -BaselineRoutePath $cleanRoutePath `
        -CandidateRoutePath $collapsedRoutePath `
        -OutputRootPath (Join-Path $outputRootPath "route-compare-collapsed") `
        -PythonExePath $resolvedPythonExe `
        -Label "world shelf review collapsed route compare"

    $cases = @(
        [ordered]@{
            Name = "clean-world-shelf-review-opening-corridor"
            SourceSummary = $cleanRoutePath
            ExpectedRouteVerdict = ""
            ExpectedExplainStatus = "ready"
            ExpectedExplainResult = "ok"
            ExpectedSelectionKind = "route_world_shelf_review_default"
            ExpectedSelectedSurface = "world_shelf_review"
            ExpectedSelectedSummarySchema = "system_compiler.world_shelf_review/v0"
            ExpectedExplainViolations = @()
            ExpectedHandoffStatus = "ready"
            ExpectedHandoffResult = "ok"
            ExpectedOpenTarget = "world_shelf_review"
            ExpectedOpenTargetSchema = "system_compiler.world_shelf_review/v0"
            ExpectedHandoffViolations = @()
        },
        [ordered]@{
            Name = "blocked-world-shelf-review-opening-corridor"
            SourceSummary = $blockedRoutePath
            ExpectedRouteVerdict = ""
            ExpectedExplainStatus = "blocked"
            ExpectedExplainResult = "fail"
            ExpectedSelectionKind = "route_world_shelf_review_default"
            ExpectedSelectedSurface = ""
            ExpectedSelectedSummarySchema = ""
            ExpectedExplainViolations = @(
                "selected world shelf review surface is missing",
                "selected world shelf review surface summary_path is missing"
            )
            ExpectedHandoffStatus = "blocked"
            ExpectedHandoffResult = "fail"
            ExpectedOpenTarget = ""
            ExpectedOpenTargetSchema = ""
            ExpectedHandoffViolations = @(
                "source explain entry result is not ok",
                "source explain_entry_decision.status is not ready",
                "selected surface is missing",
                "selected surface summary_path is missing"
            )
        },
        [ordered]@{
            Name = "standing-world-shelf-review-route-compare-opening-corridor"
            SourceSummary = $standingComparePath
            ExpectedRouteVerdict = "standing"
            ExpectedExplainStatus = "ready"
            ExpectedExplainResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_root"
            ExpectedSelectedSurface = "candidate_route"
            ExpectedSelectedSummarySchema = "system_compiler.front_page_route/v0"
            ExpectedExplainViolations = @()
            ExpectedHandoffStatus = "ready"
            ExpectedHandoffResult = "ok"
            ExpectedOpenTarget = "candidate_route"
            ExpectedOpenTargetSchema = "system_compiler.front_page_route/v0"
            ExpectedHandoffViolations = @()
        },
        [ordered]@{
            Name = "drifted-world-shelf-review-route-compare-opening-corridor"
            SourceSummary = $driftedComparePath
            ExpectedRouteVerdict = "drifted"
            ExpectedExplainStatus = "ready"
            ExpectedExplainResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_change"
            ExpectedSelectedSurface = "world_shelf_review"
            ExpectedSelectedSummarySchema = "system_compiler.world_shelf_review/v0"
            ExpectedExplainViolations = @()
            ExpectedHandoffStatus = "ready"
            ExpectedHandoffResult = "ok"
            ExpectedOpenTarget = "world_shelf_review"
            ExpectedOpenTargetSchema = "system_compiler.world_shelf_review/v0"
            ExpectedHandoffViolations = @()
        },
        [ordered]@{
            Name = "improved-world-shelf-review-route-compare-opening-corridor"
            SourceSummary = $improvedComparePath
            ExpectedRouteVerdict = "improved"
            ExpectedExplainStatus = "ready"
            ExpectedExplainResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_change"
            ExpectedSelectedSurface = "world_compare"
            ExpectedSelectedSummarySchema = "system_compiler.world_compare/v0"
            ExpectedExplainViolations = @()
            ExpectedHandoffStatus = "ready"
            ExpectedHandoffResult = "ok"
            ExpectedOpenTarget = "world_compare"
            ExpectedOpenTargetSchema = "system_compiler.world_compare/v0"
            ExpectedHandoffViolations = @()
        },
        [ordered]@{
            Name = "collapsed-world-shelf-review-route-compare-opening-corridor"
            SourceSummary = $collapsedComparePath
            ExpectedRouteVerdict = "collapsed"
            ExpectedExplainStatus = "blocked"
            ExpectedExplainResult = "fail"
            ExpectedSelectionKind = "blocked"
            ExpectedSelectedSurface = ""
            ExpectedSelectedSummarySchema = ""
            ExpectedExplainViolations = @(
                "source route compare verdict is collapsed",
                "selected world shelf review surface is missing",
                "selected world shelf review surface summary_path is missing"
            )
            ExpectedHandoffStatus = "blocked"
            ExpectedHandoffResult = "fail"
            ExpectedOpenTarget = ""
            ExpectedOpenTargetSchema = ""
            ExpectedHandoffViolations = @(
                "source explain entry result is not ok",
                "source explain_entry_decision.status is not ready",
                "selected surface is missing",
                "selected surface summary_path is missing"
            )
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $explainEntryExportScript,
                "--source-summary",
                [string]$case.SourceSummary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("world shelf review explain-entry export failed for case '{0}'" -f $case.Name)

        $explainEntrySummaryPath = Join-Path $caseOutputRoot "front-page.entry-world-shelf-review.explain-entry.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($explainEntryValidateScript, "--summary", $explainEntrySummaryPath) `
            -FailureMessage ("world shelf review explain-entry validation failed for case '{0}'" -f $case.Name)

        $explainEntrySummary = Load-JsonObject -Path $explainEntrySummaryPath
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.result -eq [string]$case.ExpectedExplainResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedExplainResult, $explainEntrySummary.result)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.explain_entry_decision.status -eq [string]$case.ExpectedExplainStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedExplainStatus, $explainEntrySummary.explain_entry_decision.status)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.explain_entry_decision.selection_kind -eq [string]$case.ExpectedSelectionKind) `
            -Message ("case '{0}' expected selection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectionKind, $explainEntrySummary.explain_entry_decision.selection_kind)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.selected_surface.surface_id -eq [string]$case.ExpectedSelectedSurface) `
            -Message ("case '{0}' expected selected surface '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSurface, $explainEntrySummary.selected_surface.surface_id)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.selected_surface.summary_schema -eq [string]$case.ExpectedSelectedSummarySchema) `
            -Message ("case '{0}' expected selected schema '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSummarySchema, $explainEntrySummary.selected_surface.summary_schema)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.explain_entry_decision.selected_tab_id -eq "grouped_review") `
            -Message ("case '{0}' expected selected tab grouped_review" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$explainEntrySummary.explain_entry_decision.selected_role -eq "world_shelf_review_explain_entry") `
            -Message ("case '{0}' expected selected role world_shelf_review_explain_entry" -f $case.Name)
        if ([string]::IsNullOrWhiteSpace([string]$case.ExpectedRouteVerdict)) {
            Assert-Condition `
                -Condition ([string]$explainEntrySummary.source_route_ref.route_verdict -eq "") `
                -Message ("case '{0}' expected empty route verdict" -f $case.Name)
        } else {
            Assert-Condition `
                -Condition ([string]$explainEntrySummary.source_route_ref.route_verdict -eq [string]$case.ExpectedRouteVerdict) `
                -Message ("case '{0}' expected route verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedRouteVerdict, $explainEntrySummary.source_route_ref.route_verdict)
        }
        foreach ($expectedViolation in @($case.ExpectedExplainViolations)) {
            Assert-Condition `
                -Condition (@($explainEntrySummary.violations) -contains [string]$expectedViolation) `
                -Message ("case '{0}' expected violation '{1}'" -f $case.Name, $expectedViolation)
        }
        if (@($case.ExpectedExplainViolations).Count -eq 0) {
            Assert-Condition `
                -Condition (@($explainEntrySummary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no violations" -f $case.Name)
        }

        $serializedExplainEntry = $explainEntrySummary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer", "runtime_evidence_bundle", "open_event_witness", "landing_decision")) {
            Assert-Condition `
                -Condition (-not $serializedExplainEntry.Contains($forbiddenText)) `
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        $handoffOutputRoot = Join-Path $caseOutputRoot "handoff"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $handoffExportScript,
                "--source-summary",
                $explainEntrySummaryPath,
                "--output-root",
                $handoffOutputRoot
            ) `
            -FailureMessage ("world shelf review handoff export failed for case '{0}'" -f $case.Name)

        $handoffSummaryPath = Join-Path $handoffOutputRoot "front-page.entry-world-shelf-review.explain-entry.handoff.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($handoffValidateScript, "--summary", $handoffSummaryPath) `
            -FailureMessage ("world shelf review handoff validation failed for case '{0}'" -f $case.Name)

        $handoffSummary = Load-JsonObject -Path $handoffSummaryPath
        Assert-Condition `
            -Condition ([string]$handoffSummary.result -eq [string]$case.ExpectedHandoffResult) `
            -Message ("case '{0}' expected handoff result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedHandoffResult, $handoffSummary.result)
        Assert-Condition `
            -Condition ([string]$handoffSummary.handoff_decision.status -eq [string]$case.ExpectedHandoffStatus) `
            -Message ("case '{0}' expected handoff status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedHandoffStatus, $handoffSummary.handoff_decision.status)
        Assert-Condition `
            -Condition ([string]$handoffSummary.open_target.surface_id -eq [string]$case.ExpectedOpenTarget) `
            -Message ("case '{0}' expected open target '{1}' but got '{2}'" -f $case.Name, $case.ExpectedOpenTarget, $handoffSummary.open_target.surface_id)
        Assert-Condition `
            -Condition ([string]$handoffSummary.open_target.summary_schema -eq [string]$case.ExpectedOpenTargetSchema) `
            -Message ("case '{0}' expected open target schema '{1}' but got '{2}'" -f $case.Name, $case.ExpectedOpenTargetSchema, $handoffSummary.open_target.summary_schema)
        Assert-Condition `
            -Condition ([string]$handoffSummary.handoff_action.expected_consumer_operation -eq "open-selected-summary") `
            -Message ("case '{0}' expected operation open-selected-summary" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$handoffSummary.handoff_decision.selected_tab_id -eq "grouped_review") `
            -Message ("case '{0}' expected handoff selected tab grouped_review" -f $case.Name)
        Assert-Condition `
            -Condition ([string]$handoffSummary.handoff_decision.selected_role -eq "world_shelf_review_explain_handoff") `
            -Message ("case '{0}' expected handoff selected role world_shelf_review_explain_handoff" -f $case.Name)
        foreach ($expectedViolation in @($case.ExpectedHandoffViolations)) {
            Assert-Condition `
                -Condition (@($handoffSummary.violations) -contains [string]$expectedViolation) `
                -Message ("case '{0}' expected handoff violation '{1}'" -f $case.Name, $expectedViolation)
        }
        if (@($case.ExpectedHandoffViolations).Count -eq 0) {
            Assert-Condition `
                -Condition (@($handoffSummary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no handoff violations" -f $case.Name)
        }

        $serializedHandoff = $handoffSummary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer", "runtime_evidence_bundle", "open_event_witness", "landing_decision")) {
            Assert-Condition `
                -Condition (-not $serializedHandoff.Contains($forbiddenText)) `
                -Message ("case '{0}' handoff should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-WORLD-SHELF-REVIEW-OPENING-CORRIDOR-SMOKE] case={0} explain_status={1} handoff_status={2} selected={3}" -f
            $case.Name,
            [string]$explainEntrySummary.explain_entry_decision.status,
            [string]$handoffSummary.handoff_decision.status,
            [string]$explainEntrySummary.selected_surface.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-WORLD-SHELF-REVIEW-OPENING-CORRIDOR-SMOKE] output_root={0}" -f $outputRootPath)
