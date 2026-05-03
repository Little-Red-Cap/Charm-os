param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-entry-capability-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-landing-smoke",
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

$capabilitySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_capability_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_landing.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing.py"
foreach ($requiredPath in @($capabilitySmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$cases = @(
    [ordered]@{
        Name = "root-witness"
        SummaryPath = Join-Path $inputRootPath "root-witness\front-page.entry-capability.summary.json"
        ExpectedMode = "biography"
        ExpectedPrimary = "delivery_biography"
        ExpectedTabsPrefix = @("delivery_biography", "supporting_evidence", "supporting_testimony")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "report"
    },
    [ordered]@{
        Name = "root-world-compare"
        SummaryPath = Join-Path $inputRootPath "root-world-compare\front-page.entry-capability.summary.json"
        ExpectedMode = "compare"
        ExpectedPrimary = "counterfactual_verdict"
        ExpectedTabsPrefix = @("counterfactual_verdict", "delivery_biography", "supporting_evidence")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
    },
    [ordered]@{
        Name = "witness-ci-shelf"
        SummaryPath = Join-Path $inputRootPath "witness-ci-shelf\front-page.entry-capability.summary.json"
        ExpectedMode = "review"
        ExpectedPrimary = "grouped_review"
        ExpectedTabsPrefix = @("grouped_review", "shelf_compare", "candidate_shelf", "baseline_shelf")
        ExpectedProvenanceRoots = 0
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
    },
    [ordered]@{
        Name = "review-provenance"
        SummaryPath = Join-Path $inputRootPath "review-provenance\front-page.entry-capability.summary.json"
        ExpectedMode = "review"
        ExpectedPrimary = "grouped_review"
        ExpectedTabsPrefix = @("grouped_review", "shelf_compare", "candidate_shelf", "baseline_shelf")
        ExpectedProvenanceRoots = 3
        ExpectedPrimaryQueryKind = "default_overview"
        ExpectedPrimaryQueryScope = "artifact_root"
    }
)

Push-Location $repoRoot
try {
    $bootstrapInputs = @($cases | ForEach-Object { [string]$_.SummaryPath })
    if (Test-AllPathsExist -Paths $bootstrapInputs) {
        Write-Host "[FRONT-PAGE-ENTRY-LANDING-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $capabilitySmokeScript,
                "-OutputRoot",
                $inputRootPath
            ) `
            -FailureMessage "front page entry capability smoke bootstrap failed"
    }

    foreach ($case in $cases) {
        if (-not (Test-Path $case.SummaryPath)) {
            throw "entry capability summary not found for case '$($case.Name)': $($case.SummaryPath)"
        }

        $caseOutputRoot = Join-Path $outputRootPath $case.Name
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($exportScript, "--summary", $case.SummaryPath, "--output-root", $caseOutputRoot) `
            -FailureMessage ("front page entry landing export failed for case '{0}'" -f $case.Name)

        $landingSummaryPath = Join-Path $caseOutputRoot "front-page.entry-landing.summary.json"
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList @($validateScript, "--summary", $landingSummaryPath) `
            -FailureMessage ("front page entry landing validation failed for case '{0}'" -f $case.Name)

        $landingSummary = Load-JsonObject -Path $landingSummaryPath
        Assert-Condition `
            -Condition ([string]$landingSummary.landing_status.recommended_entry_mode -eq $case.ExpectedMode) `
            -Message ("case '{0}' expected mode '{1}' but got '{2}'" -f $case.Name, $case.ExpectedMode, $landingSummary.landing_status.recommended_entry_mode)
        Assert-Condition `
            -Condition ([string]$landingSummary.landing_status.primary_tab_id -eq $case.ExpectedPrimary) `
            -Message ("case '{0}' expected primary tab '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimary, $landingSummary.landing_status.primary_tab_id)
        Assert-Condition `
            -Condition ([int]$landingSummary.landing_status.provenance_root_count -eq [int]$case.ExpectedProvenanceRoots) `
            -Message ("case '{0}' expected provenance roots '{1}' but got '{2}'" -f $case.Name, $case.ExpectedProvenanceRoots, $landingSummary.landing_status.provenance_root_count)
        Assert-Condition `
            -Condition ([string]$landingSummary.query_hints.primary_query.query_kind -eq $case.ExpectedPrimaryQueryKind) `
            -Message ("case '{0}' expected primary query kind '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryKind, $landingSummary.query_hints.primary_query.query_kind)
        Assert-Condition `
            -Condition ([string]$landingSummary.query_hints.primary_query.scope -eq $case.ExpectedPrimaryQueryScope) `
            -Message ("case '{0}' expected primary query scope '{1}' but got '{2}'" -f $case.Name, $case.ExpectedPrimaryQueryScope, $landingSummary.query_hints.primary_query.scope)
        Assert-Condition `
            -Condition (@($landingSummary.query_hints.tab_queries).Count -eq @($landingSummary.landing_tabs).Count) `
            -Message ("case '{0}' query_hints.tab_queries must match landing tab count" -f $case.Name)

        $availableTabIds = @([string[]]$landingSummary.landing_status.available_tab_ids)
        for ($i = 0; $i -lt $case.ExpectedTabsPrefix.Count; $i++) {
            $expectedTabId = [string]$case.ExpectedTabsPrefix[$i]
            Assert-Condition `
                -Condition ($availableTabIds.Count -gt $i -and [string]$availableTabIds[$i] -eq $expectedTabId) `
                -Message ("case '{0}' expected tab index {1} to be '{2}' but got '{3}'" -f $case.Name, $i, $expectedTabId, ($(if ($availableTabIds.Count -gt $i) { [string]$availableTabIds[$i] } else { "" })))
        }

        Write-Host (
            "[FRONT-PAGE-ENTRY-LANDING-SMOKE] case={0} mode={1} primary={2} query={3}/{4} tabs={5} provenance_roots={6}" -f
            $case.Name,
            [string]$landingSummary.landing_status.recommended_entry_mode,
            [string]$landingSummary.landing_status.primary_tab_id,
            [string]$landingSummary.query_hints.primary_query.query_kind,
            [string]$landingSummary.query_hints.primary_query.scope,
            ($availableTabIds -join ","),
            [int]$landingSummary.landing_status.provenance_root_count
        )
    }
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-LANDING-SMOKE] output_root={0}" -f $outputRootPath)
