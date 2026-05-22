param(
    [string]$OutputRoot = "cmake-build-codex-system-compiler-front-page-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return
    }

    Remove-Item -LiteralPath $Path -Recurse -Force
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

    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $json = $Value | ConvertTo-Json -Depth 100
    Write-TextFile -Path $Path -Content ($json + [Environment]::NewLine)
}

function Copy-JsonWithPathPatch {
    param(
        [string]$SourcePath,
        [string]$DestinationPath,
        [hashtable]$PathRewrites
    )

    $document = Load-JsonObject -Path $SourcePath
    foreach ($entry in $PathRewrites.GetEnumerator()) {
        $pathParts = [string[]]$entry.Key.Split(".")
        $cursor = $document
        for ($index = 0; $index -lt ($pathParts.Count - 1); $index++) {
            $cursor = $cursor.($pathParts[$index])
        }
        $cursor.($pathParts[-1]) = [string]$entry.Value
    }

    Write-JsonFile -Path $DestinationPath -Value $document
}

function New-FrontPageSurface {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Role,
        [string]$SummarySchema,
        [string]$SummaryPath,
        [string]$ReportMarkdownPath,
        [string]$CheckTextPath
    )

    return [ordered]@{
        id = $Id
        label = $Label
        role = $Role
        summary_schema = $SummarySchema
        summary_path = $SummaryPath
        report_markdown_path = $ReportMarkdownPath
        check_text_path = $CheckTextPath
    }
}

function Write-FrontPageRoot {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath,
        [string]$Schema,
        [string]$Kind,
        [string]$Title,
        [object[]]$SupportingSurfaces
    )

    Write-TextFile -Path $ReportPath -Content ("# {0}`n" -f $Title)
    Write-TextFile -Path $CheckPath -Content "result: ok`n"
    Write-JsonFile -Path $SummaryPath -Value ([ordered]@{
        schema = $Schema
        kind = $Kind
        generated_at_utc = [DateTime]::UtcNow.ToString("o")
        generator = "scripts/system_compiler_front_page_smoke_fixture_bootstrap.ps1"
        result = "ok"
        title = $Title
        world = [ordered]@{
            name = "minimal_kernel_runtime"
            title = "Minimal Kernel Runtime Witness World"
            summary = "Front-page opening-flow smoke fixture root."
            subject = [ordered]@{
                profile = "debug"
                board = "armv7a_qemu"
                active_facets = [string[]]@("runtime", "trap", "session")
            }
            first_class_terms = [string[]]@("subject", "session", "case", "fact", "contract", "witness", "transition", "ingress")
            core_questions = [string[]]@("What should this front-page route open first?")
            compare_questions = [string[]]@("Does opening-flow stay stable when compare surfaces exist?")
            contract_refs = [string[]]@(
                "docs/system/minimal_kernel_runtime_evidence_bundle_contract.md",
                "docs/system/kernel_runtime_session_witness_v0.md"
            )
        }
        front_page = [ordered]@{
            summary_path = $SummaryPath
            report_markdown_path = $ReportPath
            check_text_path = $CheckPath
            supporting_surfaces = [object[]]@($SupportingSurfaces)
        }
        artifact_context = [ordered]@{
            output_root = (Split-Path -Parent $SummaryPath)
            report_markdown_path = $ReportPath
            check_text_path = $CheckPath
        }
        witness_summary = [ordered]@{
            entry_count = 4
            ok_count = 4
            missing_count = 0
            fail_count = 0
            required_missing_count = 0
        }
        bundle_status = [ordered]@{
            baseline_state = "standing"
            candidate_state = "improved"
        }
        collapse_surface = [ordered]@{
            changed = $false
            narratives = [string[]]@()
        }
        questions = [ordered]@{
            compare_questions = [string[]]@("Does opening-flow preserve compare context?")
            next_questions = [string[]]@("Which opener projection should be rendered first?")
        }
        violations = [string[]]@()
    })
}

function Set-FixtureFrontPageSurfaces {
    param(
        [string]$SummaryPath,
        [string]$ReportPath,
        [string]$CheckPath,
        [object[]]$SupportingSurfaces
    )

    $document = Load-JsonObject -Path $SummaryPath
    $frontPage = [ordered]@{
        summary_path = $SummaryPath
        report_markdown_path = $ReportPath
        check_text_path = $CheckPath
        supporting_surfaces = [object[]]@($SupportingSurfaces)
    }

    if ($null -eq $document.PSObject.Properties["front_page"]) {
        $document | Add-Member -MemberType NoteProperty -Name "front_page" -Value $frontPage
    } else {
        $document.front_page = $frontPage
    }

    Write-JsonFile -Path $SummaryPath -Value $document
}

function Clear-FixtureRouteProvenance {
    param(
        [string]$SummaryPath
    )

    if (-not (Test-Path -LiteralPath $SummaryPath)) {
        return
    }

    $document = Load-JsonObject -Path $SummaryPath
    if ($null -eq $document.PSObject.Properties["route_provenance"]) {
        $document | Add-Member -MemberType NoteProperty -Name "route_provenance" -Value ([object[]]@())
    } else {
        $document.route_provenance = [object[]]@()
    }

    Write-JsonFile -Path $SummaryPath -Value $document
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot

Push-Location $repoRoot
try {
    if ($Clean) {
        Remove-PathIfExists -Path $outputRootPath
    }
    Ensure-Directory -Path $outputRootPath

    $resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
        Resolve-ToolPath -Candidates @("python.exe", "python")
    } else {
        Resolve-FullPath -Path $PythonExe
    }

    $biographyExportScript = Join-Path $PSScriptRoot "export_system_compiler_biography.py"
    $reviewShelfScript = Join-Path $PSScriptRoot "review_system_compiler_world_shelf.ps1"
    foreach ($requiredPath in @($biographyExportScript, $reviewShelfScript)) {
        if (-not (Test-Path $requiredPath)) {
            throw "missing path: $requiredPath"
        }
    }

    $fixtureSupportRoot = Join-Path $outputRootPath "_fixture_support"
    Ensure-Directory -Path $fixtureSupportRoot

    $runtimeEvidenceSource = Resolve-FullPath -Path "schemas\examples\minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json"
    $runtimeEvidenceSummary = Join-Path $fixtureSupportRoot "runtime-evidence.summary.json"
    $runtimeEvidenceReport = Join-Path $fixtureSupportRoot "runtime-evidence.report.md"
    $runtimeEvidenceCheck = Join-Path $fixtureSupportRoot "runtime-evidence.check.txt"
    $witnessSource = Resolve-FullPath -Path "schemas\examples\system_compiler.witness_bundle.v0.sample.json"
    $worldCompareSource = Resolve-FullPath -Path "schemas\examples\system_compiler.world_compare.v0.sample.json"
    $witnessSummary = Join-Path $fixtureSupportRoot "witness-bundle.summary.json"
    $witnessReport = Join-Path $fixtureSupportRoot "witness-bundle.report.md"
    $witnessCheck = Join-Path $fixtureSupportRoot "witness-bundle.check.txt"
    $worldCompareSummary = Join-Path $fixtureSupportRoot "world-compare.summary.json"
    $worldCompareReport = Join-Path $fixtureSupportRoot "world-compare.report.md"
    $worldCompareCheck = Join-Path $fixtureSupportRoot "world-compare.check.txt"
    $runtimeSessionSummary = Resolve-FullPath -Path "schemas\examples\minimal_kernel.kernel_runtime_session.v0.sample.json"
    $runtimeSessionReport = Resolve-FullPath -Path "docs\system\kernel_runtime_session_witness_v0.md"
    $runtimeSessionCheck = Resolve-FullPath -Path "docs\system\kernel_runtime_session_witness_v0.md"

    Write-TextFile -Path $runtimeEvidenceReport -Content "# Front Page Smoke Runtime Evidence Fixture`n"
    Write-TextFile -Path $runtimeEvidenceCheck -Content "result: ok`n"
    Write-TextFile -Path $witnessReport -Content "# Front Page Smoke Witness Bundle Fixture`n"
    Write-TextFile -Path $witnessCheck -Content "result: ok`n"
    Write-TextFile -Path $worldCompareReport -Content "# Front Page Smoke World Compare Fixture`n"
    Write-TextFile -Path $worldCompareCheck -Content "result: ok`n"

    Copy-JsonWithPathPatch `
        -SourcePath $runtimeEvidenceSource `
        -DestinationPath $runtimeEvidenceSummary `
        -PathRewrites @{
            "report_markdown_path" = $runtimeEvidenceReport
            "check_text_path" = $runtimeEvidenceCheck
        }
    Copy-JsonWithPathPatch `
        -SourcePath $witnessSource `
        -DestinationPath $witnessSummary `
        -PathRewrites @{
            "front_page.summary_path" = $witnessSummary
            "front_page.report_markdown_path" = $witnessReport
            "front_page.check_text_path" = $witnessCheck
            "artifact_context.output_root" = $fixtureSupportRoot
            "artifact_context.report_markdown_path" = $witnessReport
            "artifact_context.check_text_path" = $witnessCheck
            "artifact_context.artifact_report_index" = ""
        }
    Copy-JsonWithPathPatch `
        -SourcePath $worldCompareSource `
        -DestinationPath $worldCompareSummary `
        -PathRewrites @{
            "world_verdict" = "improved"
            "artifact_context.baseline_witness_bundle" = $witnessSummary
            "artifact_context.candidate_witness_bundle" = $witnessSummary
            "artifact_context.output_root" = $fixtureSupportRoot
            "artifact_context.report_markdown_path" = $worldCompareReport
            "artifact_context.check_text_path" = $worldCompareCheck
        }

    $candidateBiographyRoot = Join-Path $outputRootPath "witness-ci-shelf"
    $baselineBiographyRoot = Join-Path $outputRootPath "witness-ci-shelf\self-compare"
    $rootWitnessBiographyRoot = Join-Path $outputRootPath "_biography\root-witness"
    $rootWorldCompareBiographyRoot = Join-Path $outputRootPath "_biography\root-world-compare"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $biographyExportScript,
            "--runtime-evidence",
            $runtimeEvidenceSummary,
            "--witness-bundle",
            $witnessSummary,
            "--output-root",
            $candidateBiographyRoot,
            "--profile",
            "front-page-smoke-candidate"
        ) `
        -FailureMessage "candidate biography fixture export failed"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $biographyExportScript,
            "--runtime-evidence",
            $runtimeEvidenceSummary,
            "--witness-bundle",
            $witnessSummary,
            "--world-compare",
            $worldCompareSummary,
            "--output-root",
            $baselineBiographyRoot,
            "--profile",
            "front-page-smoke-baseline"
        ) `
        -FailureMessage "baseline biography fixture export failed"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $biographyExportScript,
            "--runtime-evidence",
            $runtimeEvidenceSummary,
            "--witness-bundle",
            $witnessSummary,
            "--output-root",
            $rootWitnessBiographyRoot,
            "--profile",
            "front-page-smoke-root-witness"
        ) `
        -FailureMessage "root witness biography fixture export failed"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $biographyExportScript,
            "--runtime-evidence",
            $runtimeEvidenceSummary,
            "--witness-bundle",
            $witnessSummary,
            "--world-compare",
            $worldCompareSummary,
            "--output-root",
            $rootWorldCompareBiographyRoot,
            "--profile",
            "front-page-smoke-root-world-compare"
        ) `
        -FailureMessage "root world compare biography fixture export failed"

    $reviewRoot = Join-Path $outputRootPath "_review"
    $reviewSummary = Join-Path $reviewRoot "world-shelf.review.summary.json"
    Write-Host "==> review_system_compiler_world_shelf.ps1"
    & $reviewShelfScript `
        -BiographySummary @(
            (Join-Path $candidateBiographyRoot "biography.summary.json"),
            (Join-Path $baselineBiographyRoot "biography.summary.json")
        ) `
        -BaselineBiographySummary @((Join-Path $candidateBiographyRoot "biography.summary.json")) `
        -OutputRoot $reviewRoot `
        -CandidateShelfOutputRoot (Join-Path $reviewRoot "world-shelf") `
        -BaselineShelfOutputRoot (Join-Path $reviewRoot "world-shelf-baseline") `
        -CompareOutputRoot (Join-Path $reviewRoot "world-shelf-compare") `
        -ReviewSummaryPath $reviewSummary `
        -ReviewReportMarkdownPath (Join-Path $reviewRoot "world-shelf.review.md") `
        -ReviewCheckTextPath (Join-Path $reviewRoot "world-shelf.check.txt") `
        -PythonExe $resolvedPythonExe `
        -CandidateProfile "front-page-smoke-candidate" `
        -BaselineProfile "front-page-smoke-baseline" `
        -CandidateRequireBiographyCount 2 `
        -CandidateRequireCompareAttachedCount 1 `
        -CandidateRequireNotAttachedCount 1 `
        -BaselineRequireBiographyCount 1 `
        -BaselineRequireCompareAttachedCount 0 `
        -BaselineRequireNotAttachedCount 1 `
        -CompareRequireVerdict "improved" `
        -CompareMaxRegressions 0 `
        -CompareRequireAddedEntries 1 `
        -CompareRequireRemovedEntries 0 `
        -CompareRequireChangedEntries 1 `
        -CompareRequireImprovementCount 1 `
        -CompareRequireAddedWorlds 0 `
        -CompareRequireRemovedWorlds 0 `
        -CompareMaxAddedFailedEntries 0 `
        -Clean

    $rootWitnessSummary = Join-Path $outputRootPath "root-witness\summary.json"
    $rootWitnessReport = Join-Path $outputRootPath "root-witness\report.md"
    $rootWitnessCheck = Join-Path $outputRootPath "root-witness\check.txt"
    $rootWorldCompareSummary = Join-Path $outputRootPath "root-world-compare\summary.json"
    $rootWorldCompareReport = Join-Path $outputRootPath "root-world-compare\report.md"
    $rootWorldCompareCheck = Join-Path $outputRootPath "root-world-compare\check.txt"
    $witnessShelfSummary = Join-Path $outputRootPath "witness-ci-shelf\summary.json"
    $witnessShelfReport = Join-Path $outputRootPath "witness-ci-shelf\report.md"
    $witnessShelfCheck = Join-Path $outputRootPath "witness-ci-shelf\check.txt"
    $worldCompareShelfSummary = Join-Path $outputRootPath "world-compare-ci-shelf\summary.json"
    $worldCompareShelfReport = Join-Path $outputRootPath "world-compare-ci-shelf\report.md"
    $worldCompareShelfCheck = Join-Path $outputRootPath "world-compare-ci-shelf\check.txt"

    Clear-FixtureRouteProvenance -SummaryPath $reviewSummary
    Clear-FixtureRouteProvenance -SummaryPath (Join-Path $reviewRoot "world-shelf-compare\summary.json")
    Set-FixtureFrontPageSurfaces `
        -SummaryPath $worldCompareSummary `
        -ReportPath $worldCompareReport `
        -CheckPath $worldCompareCheck `
        -SupportingSurfaces @(
            (New-FrontPageSurface -Id "fixture_root_world_compare_cycle" -Label "fixture root world compare cycle" -Role "fixture_cycle" -SummarySchema "system_compiler.world_compare/v0" -SummaryPath $rootWorldCompareSummary -ReportMarkdownPath $rootWorldCompareReport -CheckTextPath $rootWorldCompareCheck)
        )
    Set-FixtureFrontPageSurfaces `
        -SummaryPath $reviewSummary `
        -ReportPath (Join-Path $reviewRoot "world-shelf.review.md") `
        -CheckPath (Join-Path $reviewRoot "world-shelf.check.txt") `
        -SupportingSurfaces @(
            (New-FrontPageSurface -Id "candidate_shelf" -Label "candidate shelf" -Role "candidate_shelf" -SummarySchema "system_compiler.biography_index/v0" -SummaryPath (Join-Path $reviewRoot "world-shelf\biography.index.summary.json") -ReportMarkdownPath (Join-Path $reviewRoot "world-shelf\biography.index.report.md") -CheckTextPath (Join-Path $reviewRoot "world-shelf\biography.index.check.txt")),
            (New-FrontPageSurface -Id "shelf_compare" -Label "shelf compare" -Role "shelf_compare" -SummarySchema "system_compiler.biography_index_compare/v0" -SummaryPath (Join-Path $reviewRoot "world-shelf-compare\summary.json") -ReportMarkdownPath (Join-Path $reviewRoot "world-shelf-compare\report.md") -CheckTextPath (Join-Path $reviewRoot "world-shelf-compare\check.txt")),
            (New-FrontPageSurface -Id "baseline_shelf" -Label "baseline shelf" -Role "baseline_shelf" -SummarySchema "system_compiler.biography_index/v0" -SummaryPath (Join-Path $reviewRoot "world-shelf-baseline\biography.index.summary.json") -ReportMarkdownPath (Join-Path $reviewRoot "world-shelf-baseline\biography.index.report.md") -CheckTextPath (Join-Path $reviewRoot "world-shelf-baseline\biography.index.check.txt")),
            (New-FrontPageSurface -Id "fixture_witness_shelf_cycle" -Label "fixture witness shelf cycle" -Role "fixture_cycle" -SummarySchema "system_compiler.witness_bundle/v0" -SummaryPath $witnessShelfSummary -ReportMarkdownPath $witnessShelfReport -CheckTextPath $witnessShelfCheck),
            (New-FrontPageSurface -Id "fixture_world_compare_shelf_cycle" -Label "fixture world compare shelf cycle" -Role "fixture_cycle" -SummarySchema "system_compiler.world_compare/v0" -SummaryPath $worldCompareShelfSummary -ReportMarkdownPath $worldCompareShelfReport -CheckTextPath $worldCompareShelfCheck)
        )

    $witnessFixture = Load-JsonObject -Path $witnessSummary
    $witnessFixtureSurfaces = [System.Collections.Generic.List[object]]::new()
    foreach ($surface in @($witnessFixture.front_page.supporting_surfaces)) {
        $witnessFixtureSurfaces.Add($surface) | Out-Null
    }
    $witnessFixtureSurfaces.Add((New-FrontPageSurface -Id "fixture_root_witness_cycle" -Label "fixture root witness cycle" -Role "fixture_cycle" -SummarySchema "system_compiler.witness_bundle/v0" -SummaryPath $rootWitnessSummary -ReportMarkdownPath $rootWitnessReport -CheckTextPath $rootWitnessCheck)) | Out-Null
    $witnessFixture.front_page.supporting_surfaces = [object[]]$witnessFixtureSurfaces.ToArray()
    Write-JsonFile -Path $witnessSummary -Value $witnessFixture

    $rootWitnessSurfaces = [object[]]@(
        (New-FrontPageSurface -Id "biography" -Label "biography" -Role "delivery_biography" -SummarySchema "system_compiler.biography/v0" -SummaryPath (Join-Path $rootWitnessBiographyRoot "biography.summary.json") -ReportMarkdownPath (Join-Path $rootWitnessBiographyRoot "biography.report.md") -CheckTextPath (Join-Path $rootWitnessBiographyRoot "biography.check.txt")),
        (New-FrontPageSurface -Id "runtime_evidence" -Label "runtime evidence" -Role "supporting_evidence" -SummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" -SummaryPath $runtimeEvidenceSummary -ReportMarkdownPath $runtimeEvidenceReport -CheckTextPath $runtimeEvidenceCheck),
        (New-FrontPageSurface -Id "kernel_runtime_session" -Label "kernel runtime session" -Role "supporting_evidence" -SummarySchema "minimal_kernel.kernel_runtime_session/v0" -SummaryPath $runtimeSessionSummary -ReportMarkdownPath $runtimeSessionReport -CheckTextPath $runtimeSessionCheck)
    )
    $rootWorldCompareSurfaces = [object[]]@(
        (New-FrontPageSurface -Id "biography" -Label "biography" -Role "delivery_biography" -SummarySchema "system_compiler.biography/v0" -SummaryPath (Join-Path $rootWorldCompareBiographyRoot "biography.summary.json") -ReportMarkdownPath (Join-Path $rootWorldCompareBiographyRoot "biography.report.md") -CheckTextPath (Join-Path $rootWorldCompareBiographyRoot "biography.check.txt")),
        (New-FrontPageSurface -Id "world_compare" -Label "world compare" -Role "counterfactual_verdict" -SummarySchema "system_compiler.world_compare/v0" -SummaryPath $worldCompareSummary -ReportMarkdownPath $worldCompareReport -CheckTextPath $worldCompareCheck),
        (New-FrontPageSurface -Id "runtime_evidence" -Label "runtime evidence" -Role "supporting_evidence" -SummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" -SummaryPath $runtimeEvidenceSummary -ReportMarkdownPath $runtimeEvidenceReport -CheckTextPath $runtimeEvidenceCheck),
        (New-FrontPageSurface -Id "kernel_runtime_session" -Label "kernel runtime session" -Role "supporting_evidence" -SummarySchema "minimal_kernel.kernel_runtime_session/v0" -SummaryPath $runtimeSessionSummary -ReportMarkdownPath $runtimeSessionReport -CheckTextPath $runtimeSessionCheck)
    )
    $reviewSurfaces = [object[]]@(
        (New-FrontPageSurface -Id "world_shelf_review" -Label "world shelf review" -Role "grouped_review" -SummarySchema "system_compiler.world_shelf_review/v0" -SummaryPath $reviewSummary -ReportMarkdownPath (Join-Path $reviewRoot "world-shelf.review.md") -CheckTextPath (Join-Path $reviewRoot "world-shelf.check.txt")),
        (New-FrontPageSurface -Id "biography" -Label "biography" -Role "delivery_biography" -SummarySchema "system_compiler.biography/v0" -SummaryPath (Join-Path $candidateBiographyRoot "biography.summary.json") -ReportMarkdownPath (Join-Path $candidateBiographyRoot "biography.report.md") -CheckTextPath (Join-Path $candidateBiographyRoot "biography.check.txt")),
        (New-FrontPageSurface -Id "runtime_evidence" -Label "runtime evidence" -Role "supporting_evidence" -SummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" -SummaryPath $runtimeEvidenceSummary -ReportMarkdownPath $runtimeEvidenceReport -CheckTextPath $runtimeEvidenceCheck),
        (New-FrontPageSurface -Id "kernel_runtime_session" -Label "kernel runtime session" -Role "supporting_evidence" -SummarySchema "minimal_kernel.kernel_runtime_session/v0" -SummaryPath $runtimeSessionSummary -ReportMarkdownPath $runtimeSessionReport -CheckTextPath $runtimeSessionCheck)
    )
    $reviewCompareSurfaces = [object[]]@(
        (New-FrontPageSurface -Id "world_shelf_review" -Label "world shelf review" -Role "grouped_review" -SummarySchema "system_compiler.world_shelf_review/v0" -SummaryPath $reviewSummary -ReportMarkdownPath (Join-Path $reviewRoot "world-shelf.review.md") -CheckTextPath (Join-Path $reviewRoot "world-shelf.check.txt")),
        (New-FrontPageSurface -Id "biography" -Label "biography" -Role "delivery_biography" -SummarySchema "system_compiler.biography/v0" -SummaryPath (Join-Path $candidateBiographyRoot "biography.summary.json") -ReportMarkdownPath (Join-Path $candidateBiographyRoot "biography.report.md") -CheckTextPath (Join-Path $candidateBiographyRoot "biography.check.txt")),
        (New-FrontPageSurface -Id "world_compare" -Label "world compare" -Role "counterfactual_verdict" -SummarySchema "system_compiler.world_compare/v0" -SummaryPath $worldCompareSummary -ReportMarkdownPath $worldCompareReport -CheckTextPath $worldCompareCheck),
        (New-FrontPageSurface -Id "runtime_evidence" -Label "runtime evidence" -Role "supporting_evidence" -SummarySchema "minimal_kernel.runtime_evidence_bundle.summary/v1" -SummaryPath $runtimeEvidenceSummary -ReportMarkdownPath $runtimeEvidenceReport -CheckTextPath $runtimeEvidenceCheck),
        (New-FrontPageSurface -Id "kernel_runtime_session" -Label "kernel runtime session" -Role "supporting_evidence" -SummarySchema "minimal_kernel.kernel_runtime_session/v0" -SummaryPath $runtimeSessionSummary -ReportMarkdownPath $runtimeSessionReport -CheckTextPath $runtimeSessionCheck)
    )

    Write-FrontPageRoot -SummaryPath $rootWitnessSummary -ReportPath $rootWitnessReport -CheckPath $rootWitnessCheck -Schema "system_compiler.witness_bundle/v0" -Kind "system_compiler.witness_bundle" -Title "Front Page Smoke Root Witness" -SupportingSurfaces $rootWitnessSurfaces
    Write-FrontPageRoot -SummaryPath $rootWorldCompareSummary -ReportPath $rootWorldCompareReport -CheckPath $rootWorldCompareCheck -Schema "system_compiler.world_compare/v0" -Kind "system_compiler.world_compare" -Title "Front Page Smoke Root World Compare" -SupportingSurfaces $rootWorldCompareSurfaces
    Write-FrontPageRoot -SummaryPath $witnessShelfSummary -ReportPath $witnessShelfReport -CheckPath $witnessShelfCheck -Schema "system_compiler.witness_bundle/v0" -Kind "system_compiler.witness_bundle" -Title "Front Page Smoke Witness Shelf" -SupportingSurfaces $reviewSurfaces
    Write-FrontPageRoot -SummaryPath $worldCompareShelfSummary -ReportPath $worldCompareShelfReport -CheckPath $worldCompareShelfCheck -Schema "system_compiler.world_compare/v0" -Kind "system_compiler.world_compare" -Title "Front Page Smoke World Compare Shelf" -SupportingSurfaces $reviewCompareSurfaces

    Write-Host ("[FRONT-PAGE-SMOKE-FIXTURE-BOOTSTRAP] output_root={0}" -f $outputRootPath)
    Write-Host ("[FRONT-PAGE-SMOKE-FIXTURE-BOOTSTRAP] root_witness={0}" -f $rootWitnessSummary)
    Write-Host ("[FRONT-PAGE-SMOKE-FIXTURE-BOOTSTRAP] root_world_compare={0}" -f $rootWorldCompareSummary)
    Write-Host ("[FRONT-PAGE-SMOKE-FIXTURE-BOOTSTRAP] witness_ci_shelf={0}" -f $witnessShelfSummary)
    Write-Host "[FRONT-PAGE-SMOKE-FIXTURE-BOOTSTRAP] ok=1"
} finally {
    Pop-Location
}
