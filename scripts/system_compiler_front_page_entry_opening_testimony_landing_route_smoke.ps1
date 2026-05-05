param(
    [string]$OpeningTestimonyLandingRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-smoke",
    [string]$OpeningTestimonyLandingCompareRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-compare-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-landing-route-smoke",
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
$openingTestimonyLandingRootPath = Resolve-FullPath -Path $OpeningTestimonyLandingRoot
$openingTestimonyLandingCompareRootPath = Resolve-FullPath -Path $OpeningTestimonyLandingCompareRoot
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

$openingTestimonyLandingSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_landing_smoke.ps1"
$openingTestimonyLandingCompareSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_landing_compare_smoke.ps1"
$routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$routeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
foreach ($requiredPath in @($openingTestimonyLandingSmokeScript, $openingTestimonyLandingCompareSmokeScript, $routeExportScript, $routeValidateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cleanLandingPath = Join-Path $openingTestimonyLandingRootPath "clean-witness-landing\front-page.entry-opening-testimony.landing.summary.json"
    $driftLandingPath = Join-Path $openingTestimonyLandingRootPath "drift-witness-landing\front-page.entry-opening-testimony.landing.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $cleanLandingPath) -and (Test-Path -LiteralPath $driftLandingPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-SMOKE] landing_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openingTestimonyLandingSmokeScript,
                "-OutputRoot",
                $openingTestimonyLandingRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening testimony landing smoke bootstrap failed"
    }

    $compareLandingPath = Join-Path $openingTestimonyLandingCompareRootPath "opening-testimony-landing-clean-to-drift\front-page.entry-opening-testimony.landing.compare.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $compareLandingPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-SMOKE] compare_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $openingTestimonyLandingCompareSmokeScript,
                "-OpeningTestimonyLandingRoot",
                $openingTestimonyLandingRootPath,
                "-OutputRoot",
                $openingTestimonyLandingCompareRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "front page entry opening testimony landing compare smoke bootstrap failed"
    }

    $cases = @(
        [ordered]@{
            Name = "clean-landing-route"
            Summary = $cleanLandingPath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_landing/v0"
            ExpectedLevel1 = @(
                "source_open_event_witness",
                "witness_evidence_ref_0_source_plan_action",
                "witness_evidence_ref_1_selected_opener",
                "witness_evidence_ref_2_open_event"
            )
            RequiredRoles = @("root", "source_open_event_witness", "witness_evidence_ref:source_plan_action", "witness_evidence_ref:selected_opener", "witness_evidence_ref:open_event")
            ExpectedMinEntries = 5
        },
        [ordered]@{
            Name = "drift-landing-route"
            Summary = $driftLandingPath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_landing/v0"
            ExpectedLevel1 = @(
                "source_open_event_witness",
                "witness_evidence_ref_0_source_plan_action",
                "witness_evidence_ref_1_selected_opener",
                "witness_evidence_ref_2_open_event",
                "witness_evidence_ref_3_source_action_compare"
            )
            RequiredRoles = @("root", "source_open_event_witness", "witness_evidence_ref:source_action_compare")
            ExpectedMinEntries = 6
        },
        [ordered]@{
            Name = "landing-compare-route"
            Summary = $compareLandingPath
            ExpectedRootSchema = "system_compiler.front_page_entry_opening_testimony_landing_compare/v0"
            ExpectedLevel1 = @(
                "baseline_opening_testimony_landing",
                "candidate_opening_testimony_landing"
            )
            RequiredRoles = @("root", "baseline_opening_testimony_landing", "candidate_opening_testimony_landing", "source_open_event_witness")
            ExpectedMinEntries = 8
        }
    )

    foreach ($case in $cases) {
        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @(
                $routeExportScript,
                "--summary",
                [string]$case.Summary,
                "--output-root",
                $caseOutputRoot
            ) `
            -FailureMessage ("front page route export failed for case '{0}'" -f $case.Name)

        $routeSummaryPath = Join-Path $caseOutputRoot "front-page.route.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($routeValidateScript, "--summary", $routeSummaryPath) `
            -FailureMessage ("front page route validation failed for case '{0}'" -f $case.Name)

        $routeSummary = Load-JsonObject -Path $routeSummaryPath
        Assert-Condition `
            -Condition ([string]$routeSummary.root_surface.summary_schema -eq [string]$case.ExpectedRootSchema) `
            -Message ("case '{0}' expected root schema '{1}' but got '{2}'" -f $case.Name, $case.ExpectedRootSchema, $routeSummary.root_surface.summary_schema)
        Assert-Condition `
            -Condition ([int]$routeSummary.route_summary.entry_count -ge [int]$case.ExpectedMinEntries) `
            -Message ("case '{0}' expected at least '{1}' route entries but got '{2}'" -f $case.Name, $case.ExpectedMinEntries, $routeSummary.route_summary.entry_count)

        $level1SurfaceIds = @(
            @($routeSummary.route_entries) |
                Where-Object { [int]$_.depth -eq 1 } |
                ForEach-Object { [string]$_.surface_id }
        )
        Assert-Condition `
            -Condition (($level1SurfaceIds -join ",") -eq (@($case.ExpectedLevel1) -join ",")) `
            -Message ("case '{0}' expected level-1 '{1}' but got '{2}'" -f $case.Name, (@($case.ExpectedLevel1) -join ","), ($level1SurfaceIds -join ","))

        $allRoles = @(
            @($routeSummary.route_entries) |
                ForEach-Object { [string]$_.role }
        )
        foreach ($requiredRole in @($case.RequiredRoles)) {
            Assert-Condition `
                -Condition ($allRoles -contains [string]$requiredRole) `
                -Message ("case '{0}' missing route role '{1}'" -f $case.Name, $requiredRole)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-SMOKE] case={0} entries={1} level1={2} root_schema={3}" -f
            $case.Name,
            [int]$routeSummary.route_summary.entry_count,
            ($level1SurfaceIds -join ","),
            [string]$routeSummary.root_surface.summary_schema
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-LANDING-ROUTE-SMOKE] output_root={0}" -f $outputRootPath)
