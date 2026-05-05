param(
    [string]$RuntimeSessionRouteCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-route-compare-smoke",
    [string]$RuntimeSessionLandingCompareRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-compare-smoke",
    [string]$RuntimeSessionRouteRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-explain-entry-route-compare-smoke",
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
$runtimeSessionRouteCompareRootPath = Resolve-FullPath -Path $RuntimeSessionRouteCompareRoot
$runtimeSessionLandingCompareRootPath = Resolve-FullPath -Path $RuntimeSessionLandingCompareRoot
$runtimeSessionRouteRootPath = Resolve-FullPath -Path $RuntimeSessionRouteRoot
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

$routeCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_route_compare_smoke.ps1"
$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_route_smoke.ps1"
$routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$compareRouteScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry.py"
foreach ($requiredPath in @($routeCompareSmokeScript, $routeSmokeScript, $routeExportScript, $compareRouteScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $standingComparePath = Join-Path $runtimeSessionRouteCompareRootPath "opening-testimony-route-self-standing\front-page.route.compare.summary.json"
    $improvedComparePath = Join-Path $runtimeSessionRouteCompareRootPath "opening-testimony-route-clean-to-drift\front-page.route.compare.summary.json"
    $driftedComparePath = Join-Path $runtimeSessionRouteCompareRootPath "opening-testimony-route-landing-to-compare\front-page.route.compare.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $standingComparePath) -and
        (Test-Path -LiteralPath $improvedComparePath) -and
        (Test-Path -LiteralPath $driftedComparePath)
    ) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-ROUTE-COMPARE-SMOKE] route_compare_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $routeCompareSmokeScript,
                "-RuntimeSessionRouteRoot",
                $runtimeSessionRouteRootPath,
                "-OutputRoot",
                $runtimeSessionRouteCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session opening testimony landing route compare smoke bootstrap failed"
    }

    $collapsedCompareRoot = Join-Path $outputRootPath "_collapsed-route-compare-fixture"
    $cleanRoutePath = Join-Path $runtimeSessionRouteRootPath "clean-landing-route\front-page.route.summary.json"
    $blockedLandingRoutePath = Join-Path $runtimeSessionRouteRootPath "blocked-landing-route\front-page.route.summary.json"
    if (-not (Test-Path -LiteralPath $blockedLandingRoutePath)) {
        $blockedLandingPath = Join-Path $runtimeSessionLandingCompareRootPath "..\..\cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke\blocked-empty-explanation-landing\front-page.entry-opening-testimony.landing.summary.json"
        $resolvedBlockedLandingPath = Resolve-FullPath -Path $blockedLandingPath
        if (-not (Test-Path -LiteralPath $resolvedBlockedLandingPath)) {
            $resolvedBlockedLandingPath = Join-Path (Resolve-FullPath -Path "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke") "blocked-empty-explanation-landing\front-page.entry-opening-testimony.landing.summary.json"
        }
        if (-not (Test-Path -LiteralPath $resolvedBlockedLandingPath)) {
            Invoke-ExternalTool `
                -Executable $powerShellExe `
                -ArgumentList @(
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    (Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_testimony_landing_smoke.ps1"),
                    "-OutputRoot",
                    (Resolve-FullPath -Path "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-landing-smoke"),
                    "-PythonExe",
                    $resolvedPythonExe,
                    "-Clean"
                ) `
                -FailureMessage "runtime session opening testimony landing smoke bootstrap for collapsed fixture failed"
        }
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $routeExportScript,
                "--summary",
                $resolvedBlockedLandingPath,
                "--output-root",
                (Join-Path $runtimeSessionRouteRootPath "blocked-landing-route")
            ) `
            -FailureMessage "runtime session blocked landing route export failed"
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareRouteScript,
            "--baseline",
            $cleanRoutePath,
            "--candidate",
            $blockedLandingRoutePath,
            "--output-root",
            $collapsedCompareRoot
        ) `
        -FailureMessage "runtime session collapsed route compare fixture export failed"
    $collapsedComparePath = Join-Path $collapsedCompareRoot "front-page.route.compare.summary.json"
    $collapsedCompareSummary = Load-JsonObject -Path $collapsedComparePath
    $collapsedCompareSummary.result = "fail"
    $collapsedCompareSummary.route_verdict = "collapsed"
    Write-JsonFile -Path $collapsedComparePath -Value $collapsedCompareSummary

    $cases = @(
        [ordered]@{
            Name = "standing-route-compare-explain-entry"
            SourceSummary = $standingComparePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_root"
            ExpectedSelectedSurface = "candidate_route"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "improved-route-compare-explain-entry"
            SourceSummary = $improvedComparePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_change"
            ExpectedSelectedSurface = "source_open_event_witness"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "drifted-route-compare-explain-entry"
            SourceSummary = $driftedComparePath
            ExpectedStatus = "ready"
            ExpectedResult = "ok"
            ExpectedSelectionKind = "route_compare_candidate_change"
            ExpectedSelectedSurface = "candidate_opening_testimony_landing"
            ExpectedViolation = ""
        },
        [ordered]@{
            Name = "collapsed-route-compare-explain-entry"
            SourceSummary = $collapsedComparePath
            ExpectedStatus = "blocked"
            ExpectedResult = "fail"
            ExpectedSelectionKind = "blocked"
            ExpectedSelectedSurface = ""
            ExpectedViolation = "source route compare verdict is collapsed"
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $exportScript,
                "--source-summary",
                [string]$case.SourceSummary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("runtime session opening testimony route-compare explain-entry export failed for case '{0}'" -f $case.Name)

        $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $summaryPath) `
            -FailureMessage ("runtime session opening testimony route-compare explain-entry validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $summaryPath
        Assert-Condition `
            -Condition ([string]$summary.result -eq [string]$case.ExpectedResult) `
            -Message ("case '{0}' expected result '{1}' but got '{2}'" -f $case.Name, $case.ExpectedResult, $summary.result)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.status -eq [string]$case.ExpectedStatus) `
            -Message ("case '{0}' expected status '{1}' but got '{2}'" -f $case.Name, $case.ExpectedStatus, $summary.explain_entry_decision.status)
        Assert-Condition `
            -Condition ([string]$summary.explain_entry_decision.selection_kind -eq [string]$case.ExpectedSelectionKind) `
            -Message ("case '{0}' expected selection kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectionKind, $summary.explain_entry_decision.selection_kind)
        Assert-Condition `
            -Condition ([string]$summary.selected_surface.surface_id -eq [string]$case.ExpectedSelectedSurface) `
            -Message ("case '{0}' expected selected surface '{1}' but got '{2}'" -f $case.Name, $case.ExpectedSelectedSurface, $summary.selected_surface.surface_id)
        if (-not [string]::IsNullOrWhiteSpace([string]$case.ExpectedViolation)) {
            Assert-Condition `
                -Condition (@($summary.violations) -contains [string]$case.ExpectedViolation) `
                -Message ("case '{0}' expected violation '{1}'" -f $case.Name, $case.ExpectedViolation)
        } else {
            Assert-Condition `
                -Condition (@($summary.violations).Count -eq 0) `
                -Message ("case '{0}' expected no violations" -f $case.Name)
        }

        $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
        foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary")) {
            Assert-Condition `
                -Condition (-not $serialized.Contains($forbiddenText)) `
                -Message ("case '{0}' should not contain forbidden raw evidence field '{1}'" -f $case.Name, $forbiddenText)
        }

        Write-Host (
            "[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-ROUTE-COMPARE-SMOKE] case={0} status={1} selection={2} selected={3}" -f
            $case.Name,
            [string]$summary.explain_entry_decision.status,
            [string]$summary.explain_entry_decision.selection_kind,
            [string]$summary.selected_surface.surface_id
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-TESTIMONY-EXPLAIN-ENTRY-ROUTE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
