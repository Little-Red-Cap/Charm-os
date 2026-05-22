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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
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

function Ensure-TextFileIfMissing {
    param(
        [string]$Path,
        [string]$Content,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or (Test-Path $Path)) {
        return
    }

    Ensure-ParentDirectory -Path $Path
    Set-Content -LiteralPath $Path -Value $Content -Encoding utf8
    $CreatedPaths.Add([System.IO.Path]::GetFullPath($Path)) | Out-Null
}

function Ensure-BiographyRuntimeEvidencePlaceholder {
    param(
        [string]$BiographySummaryPath,
        [System.Collections.Generic.List[string]]$CreatedPaths
    )

    $biographySummary = Load-JsonObject -Path $BiographySummaryPath
    $placeholderSummary = "{`n  `"schema`": `"temporary.placeholder/v0`",`n  `"note`": `"Temporary runtime evidence root placeholder for front-page route smoke.`"`n}`n"
    $placeholderReport = "# Temporary Runtime Evidence Placeholder`n"
    $placeholderCheck = "temporary runtime evidence placeholder`n"

    $artifactContext = $biographySummary.artifact_context
    if ($artifactContext -is [System.Management.Automation.PSCustomObject] -or $artifactContext -is [hashtable]) {
        Ensure-TextFileIfMissing -Path ([string]$artifactContext.runtime_evidence_summary) -Content $placeholderSummary -CreatedPaths $CreatedPaths
        Ensure-TextFileIfMissing -Path ([string]$artifactContext.runtime_evidence_report_markdown_path) -Content $placeholderReport -CreatedPaths $CreatedPaths
        Ensure-TextFileIfMissing -Path ([string]$artifactContext.runtime_evidence_check_text_path) -Content $placeholderCheck -CreatedPaths $CreatedPaths
    }

    $frontPage = $biographySummary.front_page
    if ($frontPage -isnot [System.Management.Automation.PSCustomObject] -and $frontPage -isnot [hashtable]) {
        return
    }

    foreach ($surface in @($frontPage.supporting_surfaces)) {
        if (($surface -isnot [System.Management.Automation.PSCustomObject] -and $surface -isnot [hashtable]) -or [string]$surface.id -ne "runtime_evidence") {
            continue
        }

        Ensure-TextFileIfMissing -Path ([string]$surface.summary_path) -Content $placeholderSummary -CreatedPaths $CreatedPaths
        Ensure-TextFileIfMissing -Path ([string]$surface.report_markdown_path) -Content $placeholderReport -CreatedPaths $CreatedPaths
        Ensure-TextFileIfMissing -Path ([string]$surface.check_text_path) -Content $placeholderCheck -CreatedPaths $CreatedPaths
    }
}

function Remove-TemporaryFiles {
    param(
        [string[]]$Paths,
        [string]$RepoRootPath
    )

    foreach ($path in @($Paths | Sort-Object -Unique | Sort-Object Length -Descending)) {
        if ([string]::IsNullOrWhiteSpace($path) -or -not (Test-Path $path)) {
            continue
        }

        Remove-Item -LiteralPath $path -Force
        $parent = Split-Path -Parent $path
        while (-not [string]::IsNullOrWhiteSpace($parent) -and $parent.StartsWith($RepoRootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            if (-not (Test-Path $parent)) {
                break
            }

            $remaining = @(Get-ChildItem -LiteralPath $parent -Force)
            if ($remaining.Count -ne 0) {
                break
            }

            Remove-Item -LiteralPath $parent -Force
            $parent = Split-Path -Parent $parent
        }
    }
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
    $fixtureBootstrapScript = Join-Path $PSScriptRoot "system_compiler_front_page_smoke_fixture_bootstrap.ps1"
    if (-not (Test-Path $fixtureBootstrapScript)) {
        throw "input root not found and fixture bootstrap is missing: $inputRootPath"
    }

    Write-Host ("[FRONT-PAGE-ROUTE-SMOKE] bootstrap=front-page-fixture input_root={0}" -f $inputRootPath)
    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $fixtureBootstrapScript,
            "-OutputRoot",
            $inputRootPath
        ) `
        -FailureMessage "front page smoke fixture bootstrap failed"
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

$temporaryRuntimeEvidencePaths = [System.Collections.Generic.List[string]]::new()
try {
    # Older front-page smoke inputs still point at a runtime-evidence root under out/.
    # Materialize only the missing files here so the smoke stays self-bootstrapping.
    Ensure-BiographyRuntimeEvidencePlaceholder -BiographySummaryPath $routeProvenanceCandidateBiography -CreatedPaths $temporaryRuntimeEvidencePaths
    Ensure-BiographyRuntimeEvidencePlaceholder -BiographySummaryPath $routeProvenanceCompareBiography -CreatedPaths $temporaryRuntimeEvidencePaths

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
            ExpectedLevel1 = "biography,runtime_evidence,kernel_runtime_session"
            RequiredSurfaces = @("biography", "runtime_evidence", "kernel_runtime_session", "witness_bundle")
            ExpectedMinRouteProvenanceEntryCount = 0
            RequiredRouteProvenanceIds = @()
        },
        [ordered]@{
            Name = "root-world-compare"
            SummaryPath = Join-Path $inputRootPath "root-world-compare\summary.json"
            ExpectedLevel1 = "biography,world_compare,runtime_evidence,kernel_runtime_session"
            RequiredSurfaces = @("biography", "world_compare", "runtime_evidence", "kernel_runtime_session", "witness_bundle")
            ExpectedMinRouteProvenanceEntryCount = 0
            RequiredRouteProvenanceIds = @()
        },
        [ordered]@{
            Name = "witness-ci-shelf"
            SummaryPath = Join-Path $inputRootPath "witness-ci-shelf\summary.json"
            ExpectedLevel1 = "world_shelf_review,biography,runtime_evidence,kernel_runtime_session"
            RequiredSurfaces = @("world_shelf_review", "runtime_evidence", "kernel_runtime_session", "candidate_shelf", "shelf_compare", "baseline_shelf")
            ExpectedMinRouteProvenanceEntryCount = 0
            RequiredRouteProvenanceIds = @()
        },
        [ordered]@{
            Name = "world-compare-ci-shelf"
            SummaryPath = Join-Path $inputRootPath "world-compare-ci-shelf\summary.json"
            ExpectedLevel1 = "world_shelf_review,biography,world_compare,runtime_evidence,kernel_runtime_session"
            RequiredSurfaces = @("world_shelf_review", "world_compare", "runtime_evidence", "kernel_runtime_session", "candidate_shelf", "shelf_compare", "baseline_shelf")
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
} finally {
    Remove-TemporaryFiles -Paths @($temporaryRuntimeEvidencePaths) -RepoRootPath $repoRoot
}
