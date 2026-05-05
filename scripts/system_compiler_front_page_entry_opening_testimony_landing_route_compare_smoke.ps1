param(
    [string]$OpeningTestimonyLandingRouteRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-route-compare-smoke",
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
$openingTestimonyLandingRouteRootPath = Resolve-FullPath -Path $OpeningTestimonyLandingRouteRoot
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

$routeSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_landing_route_smoke.ps1"
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_route.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route_compare.py"
foreach ($requiredPath in @($routeSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanRoutePath = Join-Path $openingTestimonyLandingRouteRootPath "clean-landing-route\front-page.route.summary.json"
    $driftRoutePath = Join-Path $openingTestimonyLandingRouteRootPath "drift-landing-route\front-page.route.summary.json"
    $compareRoutePath = Join-Path $openingTestimonyLandingRouteRootPath "landing-compare-route\front-page.route.summary.json"
    if (
        (-not $Clean) -and
        (Test-Path -LiteralPath $cleanRoutePath) -and
        (Test-Path -LiteralPath $driftRoutePath) -and
        (Test-Path -LiteralPath $compareRoutePath)
    ) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-COMPARE-SMOKE] route_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $routeSmokeScript,
                "-OutputRoot",
                $openingTestimonyLandingRouteRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening testimony landing route smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "opening-testimony-route-self-standing"
            Baseline = $cleanRoutePath
            Candidate = $cleanRoutePath
            ExpectedVerdict = "standing"
            ExpectedChangedEntries = 0
            AddedLevel1 = @()
            RemovedLevel1 = @()
        },
        [ordered]@{
            Name = "opening-testimony-route-clean-to-drift"
            Baseline = $cleanRoutePath
            Candidate = $driftRoutePath
            ExpectedVerdict = "improved"
            ExpectedChangedEntries = $null
            AddedLevel1 = @("witness_evidence_ref_3_source_action_compare")
            RemovedLevel1 = @()
        },
        [ordered]@{
            Name = "opening-testimony-route-landing-to-compare"
            Baseline = $cleanRoutePath
            Candidate = $compareRoutePath
            ExpectedVerdict = "drifted"
            ExpectedChangedEntries = $null
            AddedLevel1 = @("baseline_opening_testimony_landing", "candidate_opening_testimony_landing")
            RemovedLevel1 = @("source_open_event_witness", "witness_evidence_ref_0_source_plan_action", "witness_evidence_ref_1_selected_opener", "witness_evidence_ref_2_open_event")
        }
    )

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path -LiteralPath $requiredSummary)) {
                throw "route summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $compareScript,
                "--baseline",
                [string]$case.Baseline,
                "--candidate",
                [string]$case.Candidate,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page route compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.route.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page route compare validation failed for case '{0}'" -f $case.Name)

        $summary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$summary.route_verdict -eq [string]$case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $summary.route_verdict)
        if ($null -ne $case.ExpectedChangedEntries) {
            Assert-Condition `
                -Condition ([int]$summary.entry_summary.changed_entry_count -eq [int]$case.ExpectedChangedEntries) `
                -Message ("case '{0}' expected changed entry count '{1}' but got '{2}'" -f $case.Name, $case.ExpectedChangedEntries, $summary.entry_summary.changed_entry_count)
        } else {
            Assert-Condition `
                -Condition ([int]$summary.entry_summary.changed_entry_count -gt 0) `
                -Message ("case '{0}' expected positive changed entry count" -f $case.Name)
        }

        $level1Added = @([string[]]$summary.route_changes.level1_surface_changes.added)
        $level1Removed = @([string[]]$summary.route_changes.level1_surface_changes.removed)
        foreach ($surfaceId in @($case.AddedLevel1)) {
            Assert-Condition `
                -Condition ($level1Added -contains [string]$surfaceId) `
                -Message ("case '{0}' expected added level-1 surface '{1}'" -f $case.Name, $surfaceId)
        }
        foreach ($surfaceId in @($case.RemovedLevel1)) {
            Assert-Condition `
                -Condition ($level1Removed -contains [string]$surfaceId) `
                -Message ("case '{0}' expected removed level-1 surface '{1}'" -f $case.Name, $surfaceId)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-COMPARE-SMOKE] case={0} verdict={1} changed={2} level1=+[{3}] -[{4}]" -f
            $case.Name,
            [string]$summary.route_verdict,
            [int]$summary.entry_summary.changed_entry_count,
            ($level1Added -join ","),
            ($level1Removed -join ",")
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
