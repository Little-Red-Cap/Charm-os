param(
    [string]$OpeningTestimonyExplainEntryHandoffCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-smoke",
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
$openingTestimonyExplainEntryHandoffCompareRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryHandoffCompareRoot
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

$compareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_smoke.ps1"
$routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$routeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
$routeCompareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$routeCompareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route_compare.py"
foreach ($requiredPath in @($compareSmokeScript, $routeExportScript, $routeValidateScript, $routeCompareScript, $routeCompareValidateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $standingComparePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRootPath "self-standing\front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
    $driftedComparePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRootPath "clean-to-landing-compare\front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
    $compareRouteComparePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRootPath "clean-to-compare-route\front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
    $collapsedComparePath = Join-Path $openingTestimonyExplainEntryHandoffCompareRootPath "ready-to-blocked\front-page.entry-opening-testimony.explain-entry.handoff.compare.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $standingComparePath) -and
        (Test-Path -LiteralPath $driftedComparePath) -and
        (Test-Path -LiteralPath $compareRouteComparePath) -and
        (Test-Path -LiteralPath $collapsedComparePath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-SMOKE] compare_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $compareSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryHandoffCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony explain-entry handoff compare smoke bootstrap failed"
    }

    $routeCases = @(
        [ordered]@{
            Name = "standing-handoff-compare-route"
            SourceSummary = $standingComparePath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0"
            ExpectedLevel1 = @("baseline_opening_testimony_explain_entry_handoff", "candidate_opening_testimony_explain_entry_handoff")
        },
        [ordered]@{
            Name = "drifted-handoff-compare-route"
            SourceSummary = $driftedComparePath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0"
            ExpectedLevel1 = @("baseline_opening_testimony_explain_entry_handoff", "candidate_opening_testimony_explain_entry_handoff")
        },
        [ordered]@{
            Name = "compare-route-handoff-compare-route"
            SourceSummary = $compareRouteComparePath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0"
            ExpectedLevel1 = @("baseline_opening_testimony_explain_entry_handoff", "candidate_opening_testimony_explain_entry_handoff")
        },
        [ordered]@{
            Name = "collapsed-handoff-compare-route"
            SourceSummary = $collapsedComparePath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff_compare/v0"
            ExpectedLevel1 = @("baseline_opening_testimony_explain_entry_handoff", "candidate_opening_testimony_explain_entry_handoff")
        }
    )

    foreach ($case in $routeCases) {
        if (-not (Test-Path -LiteralPath $case.SourceSummary)) {
            throw "handoff compare summary not found for case '$($case.Name)': $($case.SourceSummary)"
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $routeExportScript,
                "--summary",
                [string]$case.SourceSummary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("opening testimony handoff compare route export failed for case '{0}'" -f $case.Name)

        $routeSummaryPath = Join-Path $caseOutputRoot "front-page.route.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($routeValidateScript, "--summary", $routeSummaryPath) `
            -FailureMessage ("opening testimony handoff compare route validation failed for case '{0}'" -f $case.Name)

        $routeSummary = Load-JsonObject -Path $routeSummaryPath
        Assert-Condition `
            -Condition ([string]$routeSummary.root_surface.summary_schema -eq [string]$case.ExpectedRootSchema) `
            -Message ("case '{0}' expected root schema '{1}' but got '{2}'" -f $case.Name, $case.ExpectedRootSchema, $routeSummary.root_surface.summary_schema)
        $level1SurfaceIds = @(
            @($routeSummary.route_entries) |
                Where-Object { [int]$_.depth -eq 1 } |
                ForEach-Object { [string]$_.surface_id }
        )
        foreach ($surfaceId in @($case.ExpectedLevel1)) {
            Assert-Condition `
                -Condition ($level1SurfaceIds -contains [string]$surfaceId) `
                -Message ("case '{0}' expected level-1 surface '{1}'" -f $case.Name, $surfaceId)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-SMOKE] route_case={0} entries={1} level1=[{2}]" -f
            $case.Name,
            [int]$routeSummary.route_summary.entry_count,
            ($level1SurfaceIds -join ",")
        )
    }

    $standingRoutePath = Join-Path $outputRootPath "standing-handoff-compare-route\front-page.route.summary.json"
    $driftedRoutePath = Join-Path $outputRootPath "drifted-handoff-compare-route\front-page.route.summary.json"
    $collapsedRoutePath = Join-Path $outputRootPath "collapsed-handoff-compare-route\front-page.route.summary.json"

    $routeCompareCases = @(
        [ordered]@{
            Name = "standing-handoff-compare-route-self"
            Baseline = $standingRoutePath
            Candidate = $standingRoutePath
            ExpectedVerdict = "standing"
        },
        [ordered]@{
            Name = "drifted-handoff-compare-route-to-collapsed"
            Baseline = $driftedRoutePath
            Candidate = $collapsedRoutePath
            ExpectedVerdict = "drifted"
        }
    )

    foreach ($case in $routeCompareCases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $routeCompareScript,
                "--baseline",
                [string]$case.Baseline,
                "--candidate",
                [string]$case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("opening testimony handoff compare route compare export failed for case '{0}'" -f $case.Name)

        $routeCompareSummaryPath = Join-Path $caseOutputRoot "front-page.route.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($routeCompareValidateScript, "--summary", $routeCompareSummaryPath) `
            -FailureMessage ("opening testimony handoff compare route compare validation failed for case '{0}'" -f $case.Name)

        $routeCompareSummary = Load-JsonObject -Path $routeCompareSummaryPath
        Assert-Condition `
            -Condition ([string]$routeCompareSummary.route_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected route verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $routeCompareSummary.route_verdict)

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-SMOKE] route_compare_case={0} verdict={1} changed_entries={2}" -f
            $case.Name,
            [string]$routeCompareSummary.route_verdict,
            [int]$routeCompareSummary.entry_summary.changed_entry_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-SMOKE] output_root={0}" -f $outputRootPath)
