param(
    [string]$InputRoot = "cmake-build-codex-system-compiler-front-page-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-route-smoke",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$inputRootPath = Resolve-FullPath -Path $InputRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $inputRootPath)) {
    throw "input root not found: $inputRootPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
$reviewShelfScript = Join-Path $PSScriptRoot "review_system_compiler_world_shelf.ps1"
foreach ($requiredPath in @($exportScript, $validateScript, $reviewShelfScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$routeProvenanceReviewRoot = Join-Path $outputRootPath "_route_provenance_review"
$routeProvenanceReviewSummary = Join-Path $routeProvenanceReviewRoot "world-shelf.review.summary.json"
$routeProvenanceCandidateBiography = Join-Path $inputRootPath "witness-ci-shelf\biography.summary.json"
$routeProvenanceCompareBiography = Join-Path $inputRootPath "witness-ci-shelf\self-compare\biography.summary.json"
foreach ($requiredPath in @($routeProvenanceCandidateBiography, $routeProvenanceCompareBiography)) {
    if (-not (Test-Path $requiredPath)) {
        throw "route provenance smoke input not found: $requiredPath"
    }
}

& $reviewShelfScript `
    -BiographySummary @($routeProvenanceCandidateBiography, $routeProvenanceCompareBiography) `
    -BaselineBiographySummary @($routeProvenanceCandidateBiography) `
    -OutputRoot $routeProvenanceReviewRoot `
    -CandidateShelfOutputRoot (Join-Path $routeProvenanceReviewRoot "world-shelf") `
    -BaselineShelfOutputRoot (Join-Path $routeProvenanceReviewRoot "world-shelf-baseline") `
    -CompareOutputRoot (Join-Path $routeProvenanceReviewRoot "world-shelf-compare") `
    -ReviewSummaryPath $routeProvenanceReviewSummary `
    -ReviewReportMarkdownPath (Join-Path $routeProvenanceReviewRoot "world-shelf.review.md") `
    -ReviewCheckTextPath (Join-Path $routeProvenanceReviewRoot "world-shelf.check.txt") `
    -PythonExe $resolvedPythonExe `
    -CandidateProfile "front-page-route-smoke-candidate" `
    -BaselineProfile "front-page-route-smoke-baseline" `
    -CandidateRequireBiographyCount 2 `
    -CandidateRequireCompareAttachedCount 1 `
    -CandidateRequireNotAttachedCount 1 `
    -BaselineRequireBiographyCount 1 `
    -BaselineRequireCompareAttachedCount 0 `
    -BaselineRequireNotAttachedCount 1 `
    -CompareRequireVerdict improved `
    -CompareMaxRegressions 0 `
    -CompareRequireAddedEntries 1 `
    -CompareRequireRemovedEntries 0 `
    -CompareRequireChangedEntries 1 `
    -CompareRequireImprovementCount 1 `
    -CompareRequireAddedWorlds 0 `
    -CompareRequireRemovedWorlds 0 `
    -CompareMaxAddedFailedEntries 0 `
    -Clean

$cases = @(
    [ordered]@{
        Name = "root-witness"
        SummaryPath = Join-Path $inputRootPath "root-witness\summary.json"
        ExpectedLevel1 = "biography,runtime_evidence"
        RequiredSurfaces = @("biography", "runtime_evidence", "witness_bundle")
        ExpectedMinRouteProvenanceEntryCount = 0
        RequiredRouteProvenanceIds = @()
    },
    [ordered]@{
        Name = "root-world-compare"
        SummaryPath = Join-Path $inputRootPath "root-world-compare\summary.json"
        ExpectedLevel1 = "biography,world_compare,runtime_evidence"
        RequiredSurfaces = @("biography", "world_compare", "runtime_evidence", "witness_bundle")
        ExpectedMinRouteProvenanceEntryCount = 0
        RequiredRouteProvenanceIds = @()
    },
    [ordered]@{
        Name = "witness-ci-shelf"
        SummaryPath = Join-Path $inputRootPath "witness-ci-shelf\summary.json"
        ExpectedLevel1 = "world_shelf_review,biography,runtime_evidence"
        RequiredSurfaces = @("world_shelf_review", "candidate_shelf", "shelf_compare", "baseline_shelf")
        ExpectedMinRouteProvenanceEntryCount = 0
        RequiredRouteProvenanceIds = @()
    },
    [ordered]@{
        Name = "world-compare-ci-shelf"
        SummaryPath = Join-Path $inputRootPath "world-compare-ci-shelf\summary.json"
        ExpectedLevel1 = "world_shelf_review,biography,world_compare,runtime_evidence"
        RequiredSurfaces = @("world_shelf_review", "candidate_shelf", "shelf_compare", "baseline_shelf")
        ExpectedMinRouteProvenanceEntryCount = 0
        RequiredRouteProvenanceIds = @()
    },
    [ordered]@{
        Name = "review-provenance"
        SummaryPath = $routeProvenanceReviewSummary
        ExpectedLevel1 = "candidate_shelf,shelf_compare,baseline_shelf"
        RequiredSurfaces = @("candidate_shelf", "shelf_compare", "baseline_shelf")
        ExpectedMinRouteProvenanceEntryCount = 5
        RequiredRouteProvenanceIds = @("candidate_shelf", "shelf_compare", "baseline_shelf")
    }
)

Push-Location $repoRoot
try {
    foreach ($case in $cases) {
        if (-not (Test-Path $case.SummaryPath)) {
            throw "summary not found for case '$($case.Name)': $($case.SummaryPath)"
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($exportScript, "--summary", $case.SummaryPath, "--output-root", $caseOutputRoot) `
            -FailureMessage ("front page route export failed for case '{0}'" -f $case.Name)

        $routeSummaryPath = Join-Path $caseOutputRoot "front-page.route.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $routeSummaryPath) `
            -FailureMessage ("front page route validation failed for case '{0}'" -f $case.Name)

        $routeSummary = Load-JsonObject -Path $routeSummaryPath
        $level1SurfaceIds = @(
            @($routeSummary.route_entries) |
                Where-Object { [int]$_.depth -eq 1 } |
                ForEach-Object { [string]$_.surface_id }
        )
        $level1Joined = $level1SurfaceIds -join ","
        Assert-Condition `
            -Condition ($level1Joined -eq $case.ExpectedLevel1) `
            -Message ("case '{0}' expected level-1 surfaces '{1}' but got '{2}'" -f $case.Name, $case.ExpectedLevel1, $level1Joined)

        $allSurfaceIds = @(
            @($routeSummary.route_entries) |
                ForEach-Object { [string]$_.surface_id }
        )
        foreach ($requiredSurfaceId in @($case.RequiredSurfaces)) {
            Assert-Condition `
                -Condition ($allSurfaceIds -contains [string]$requiredSurfaceId) `
                -Message ("case '{0}' is missing route surface '{1}'" -f $case.Name, $requiredSurfaceId)
        }

        $routeProvenanceEntries = @($routeSummary.route_provenance_entries)
        Assert-Condition `
            -Condition ([int]$routeSummary.route_provenance_summary.entry_count -ge [int]$case.ExpectedMinRouteProvenanceEntryCount) `
            -Message ("case '{0}' expected at least {1} route provenance entries but got {2}" -f $case.Name, $case.ExpectedMinRouteProvenanceEntryCount, [int]$routeSummary.route_provenance_summary.entry_count)

        $routeProvenanceIds = @(
            $routeProvenanceEntries |
                ForEach-Object { [string]$_.provenance_id }
        )
        foreach ($requiredRouteProvenanceId in @($case.RequiredRouteProvenanceIds)) {
            Assert-Condition `
                -Condition ($routeProvenanceIds -contains [string]$requiredRouteProvenanceId) `
                -Message ("case '{0}' is missing route provenance id '{1}'" -f $case.Name, $requiredRouteProvenanceId)
        }

        Assert-Condition `
            -Condition ([int]$routeSummary.route_summary.cycle_entry_count -ge 1) `
            -Message ("case '{0}' expected at least one route cycle" -f $case.Name)

        Write-Host ("[FRONT-PAGE-ROUTE-SMOKE] case={0} level1={1} route_provenance={2}" -f $case.Name, $level1Joined, [int]$routeSummary.route_provenance_summary.entry_count)
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ROUTE-SMOKE] output_root={0}" -f $outputRootPath)
