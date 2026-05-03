param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-landing-compare-smoke",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$inputRootPath = Resolve-FullPath -Path $InputRoot
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
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_landing.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing_compare.py"
foreach ($requiredPath in @($landingSmokeScript, $compareScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $cases = @(
        [ordered]@{
            Name = "root-witness-to-root-witness"
            Baseline = Join-Path $inputRootPath "root-witness\front-page.entry-landing.summary.json"
            Candidate = Join-Path $inputRootPath "root-witness\front-page.entry-landing.summary.json"
            ExpectedVerdict = "standing"
            ExpectedPrimary = "delivery_biography"
            ExpectedPrimaryQueryKind = "default_overview"
            ExpectedPrimaryQueryScope = "report"
            ExpectedPrimaryQueryChanged = $false
            ExpectedQueryRegression = $false
            AddedTabs = @()
            RemovedTabs = @()
            AddedDirectModes = @()
            RemovedDirectModes = @()
            AddedProvenanceRoots = @()
        },
        [ordered]@{
            Name = "root-witness-to-root-world-compare"
            Baseline = Join-Path $inputRootPath "root-witness\front-page.entry-landing.summary.json"
            Candidate = Join-Path $inputRootPath "root-world-compare\front-page.entry-landing.summary.json"
            ExpectedVerdict = "improved"
            ExpectedPrimary = "counterfactual_verdict"
            ExpectedPrimaryQueryKind = "default_overview"
            ExpectedPrimaryQueryScope = "artifact_root"
            ExpectedPrimaryQueryChanged = $true
            ExpectedQueryRegression = $false
            AddedTabs = @("counterfactual_verdict")
            RemovedTabs = @()
            AddedDirectModes = @("compare")
            RemovedDirectModes = @()
            AddedProvenanceRoots = @()
        },
        [ordered]@{
            Name = "root-world-compare-to-root-witness"
            Baseline = Join-Path $inputRootPath "root-world-compare\front-page.entry-landing.summary.json"
            Candidate = Join-Path $inputRootPath "root-witness\front-page.entry-landing.summary.json"
            ExpectedVerdict = "drifted"
            ExpectedPrimary = "delivery_biography"
            ExpectedPrimaryQueryKind = "default_overview"
            ExpectedPrimaryQueryScope = "report"
            ExpectedPrimaryQueryChanged = $true
            ExpectedQueryRegression = $true
            AddedTabs = @()
            RemovedTabs = @("counterfactual_verdict")
            AddedDirectModes = @()
            RemovedDirectModes = @("compare")
            AddedProvenanceRoots = @()
        },
        [ordered]@{
            Name = "witness-ci-shelf-to-review-provenance"
            Baseline = Join-Path $inputRootPath "witness-ci-shelf\front-page.entry-landing.summary.json"
            Candidate = Join-Path $inputRootPath "review-provenance\front-page.entry-landing.summary.json"
            ExpectedVerdict = "improved"
            ExpectedPrimary = "grouped_review"
            ExpectedPrimaryQueryKind = "default_overview"
            ExpectedPrimaryQueryScope = "artifact_root"
            ExpectedPrimaryQueryChanged = $false
            ExpectedQueryRegression = $false
            AddedTabs = @()
            RemovedTabs = @()
            AddedDirectModes = @()
            RemovedDirectModes = @()
            AddedProvenanceRoots = @("candidate_shelf", "shelf_compare", "baseline_shelf")
        }
    )

    $bootstrapInputs = @()
    foreach ($case in $cases) {
        $bootstrapInputs += [string]$case.Baseline
        $bootstrapInputs += [string]$case.Candidate
    }

    if (Test-AllPathsExist -Paths $bootstrapInputs) {
        Write-Host "[FRONT-PAGE-ENTRY-LANDING-COMPARE-SMOKE] bootstrap=reuse-existing"
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
                $inputRootPath
            ) `
            -FailureMessage "front page entry landing smoke bootstrap failed"
    }

    foreach ($case in $cases) {
        foreach ($requiredSummary in @($case.Baseline, $case.Candidate)) {
            if (-not (Test-Path $requiredSummary)) {
                throw "entry landing summary not found for case '$($case.Name)': $requiredSummary"
            }
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($compareScript, "--baseline", $case.Baseline, "--candidate", $case.Candidate, "--output-root", $caseOutputRoot) `
            -FailureMessage ("front page entry landing compare export failed for case '{0}'" -f $case.Name)

        $compareSummaryPath = Join-Path $caseOutputRoot "front-page.entry-landing.compare.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
            -FailureMessage ("front page entry landing compare validation failed for case '{0}'" -f $case.Name)

        $compareSummary = Load-JsonObject -Path $compareSummaryPath
        Assert-Condition `
            -Condition ([string]$compareSummary.landing_verdict -eq $case.ExpectedVerdict) `
            -Message ("case '{0}' expected verdict '{1}' but got '{2}'" -f $case.Name, $case.ExpectedVerdict, $compareSummary.landing_verdict)
        Assert-Condition `
            -Condition ([string]$compareSummary.landing_status.candidate_primary_tab_id -eq $case.ExpectedPrimary) `
            -Message ("case '{0}' expected candidate primary tab '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimary, $compareSummary.landing_status.candidate_primary_tab_id)
        Assert-Condition `
            -Condition ([string]$compareSummary.primary_query_status.candidate_query_kind -eq $case.ExpectedPrimaryQueryKind) `
            -Message ("case '{0}' expected candidate primary query kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryKind, $compareSummary.primary_query_status.candidate_query_kind)
        Assert-Condition `
            -Condition ([string]$compareSummary.primary_query_status.candidate_scope -eq $case.ExpectedPrimaryQueryScope) `
            -Message ("case '{0}' expected candidate primary query scope '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryScope, $compareSummary.primary_query_status.candidate_scope)
        Assert-Condition `
            -Condition ([bool]$compareSummary.query_plan_changes.primary_query_changed -eq [bool]$case.ExpectedPrimaryQueryChanged) `
            -Message ("case '{0}' expected primary_query_changed '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryChanged, $compareSummary.query_plan_changes.primary_query_changed)
        Assert-Condition `
            -Condition ([bool]$compareSummary.query_regression_surface.changed -eq [bool]$case.ExpectedQueryRegression) `
            -Message ("case '{0}' expected query regression '{1}' but got '{2}'" -f $case.Name, $case.ExpectedQueryRegression, $compareSummary.query_regression_surface.changed)

        $addedTabs = @([string[]]$compareSummary.landing_changes.available_tab_changes.added)
        $removedTabs = @([string[]]$compareSummary.landing_changes.available_tab_changes.removed)
        $addedDirectModes = @([string[]]$compareSummary.landing_changes.direct_capability_changes.added)
        $removedDirectModes = @([string[]]$compareSummary.landing_changes.direct_capability_changes.removed)
        $addedProvenanceRoots = @([string[]]$compareSummary.landing_changes.provenance_root_changes.added)

        foreach ($tabId in @($case.AddedTabs)) {
            Assert-Condition `
                -Condition ($addedTabs -contains [string]$tabId) `
                -Message ("case '{0}' expected added tab '{1}'" -f $case.Name, $tabId)
        }
        foreach ($tabId in @($case.RemovedTabs)) {
            Assert-Condition `
                -Condition ($removedTabs -contains [string]$tabId) `
                -Message ("case '{0}' expected removed tab '{1}'" -f $case.Name, $tabId)
        }
        foreach ($mode in @($case.AddedDirectModes)) {
            Assert-Condition `
                -Condition ($addedDirectModes -contains [string]$mode) `
                -Message ("case '{0}' expected added direct mode '{1}'" -f $case.Name, $mode)
        }
        foreach ($mode in @($case.RemovedDirectModes)) {
            Assert-Condition `
                -Condition ($removedDirectModes -contains [string]$mode) `
                -Message ("case '{0}' expected removed direct mode '{1}'" -f $case.Name, $mode)
        }
        foreach ($rootId in @($case.AddedProvenanceRoots)) {
            Assert-Condition `
                -Condition ($addedProvenanceRoots -contains [string]$rootId) `
                -Message ("case '{0}' expected added provenance root '{1}'" -f $case.Name, $rootId)
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-LANDING-COMPARE-SMOKE] case={0} verdict={1} primary={2} query={3}/{4} query_changed={5} query_regression={6} tabs=+[{7}] -[{8}] direct=+[{9}] -[{10}] provenance=+[{11}]" -f
            $case.Name,
            [string]$compareSummary.landing_verdict,
            [string]$compareSummary.landing_status.candidate_primary_tab_id,
            [string]$compareSummary.primary_query_status.candidate_query_kind,
            [string]$compareSummary.primary_query_status.candidate_scope,
            [bool]$compareSummary.query_plan_changes.primary_query_changed,
            [bool]$compareSummary.query_regression_surface.changed,
            ($addedTabs -join ","),
            ($removedTabs -join ","),
            ($addedDirectModes -join ","),
            ($removedDirectModes -join ","),
            ($addedProvenanceRoots -join ",")
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-LANDING-COMPARE-SMOKE] output_root={0}" -f $outputRootPath)
