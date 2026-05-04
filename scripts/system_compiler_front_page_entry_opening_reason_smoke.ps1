param(
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-reason-smoke",
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

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Write-JsonFile {
    param(
        [string]$Path,
        [object]$Value
    )

    Ensure-ParentDirectory -Path $Path
    $json = $Value | ConvertTo-Json -Depth 32
    Set-Content -LiteralPath $Path -Value $json -Encoding utf8
}

function New-WorldShelfReviewDriftRouteFixture {
    param(
        [string]$FixtureRoot
    )

    $fixtureRootPath = Resolve-FullPath -Path $FixtureRoot
    $reviewRoot = Join-Path $fixtureRootPath "world-review"
    $routeRoot = Join-Path $fixtureRootPath "front-page-route"
    Ensure-Directory -Path $reviewRoot
    Ensure-Directory -Path $routeRoot

    $reviewSummaryPath = Join-Path $reviewRoot "world-shelf.review.summary.json"
    $reviewReportPath = Join-Path $reviewRoot "world-shelf.review.md"
    $reviewCheckPath = Join-Path $reviewRoot "world-shelf.check.txt"
    $routeSummaryPath = Join-Path $routeRoot "front-page.route.summary.json"
    $routeReportPath = Join-Path $routeRoot "front-page.route.report.md"
    $routeCheckPath = Join-Path $routeRoot "front-page.route.check.txt"

    $reviewSummary = [ordered]@{
        schema = "system_compiler.world_shelf_review/v0"
        kind = "system_compiler.world_shelf_review"
        generated_at_utc = "2026-01-01T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opening_reason_smoke.ps1"
        result = "ok"
        review_verdict = "drifted"
        drift_digest = [ordered]@{
            changed = $true
            verdict = "drifted"
        }
    }

    $routeSummary = [ordered]@{
        schema = "system_compiler.front_page_route/v0"
        kind = "system_compiler.front_page_route"
        generated_at_utc = "2026-01-01T00:00:00Z"
        generator = "scripts/system_compiler_front_page_entry_opening_reason_smoke.ps1"
        result = "ok"
        route = [ordered]@{
            title = "Opening reason drift fixture route"
            summary = "Synthetic route rooted at a drifted world shelf review."
        }
        artifact_context = [ordered]@{
            input_summary_path = (Resolve-FullPath -Path $reviewSummaryPath)
            output_root = (Resolve-FullPath -Path $routeRoot)
            route_summary_path = (Resolve-FullPath -Path $routeSummaryPath)
            report_markdown_path = (Resolve-FullPath -Path $routeReportPath)
            check_text_path = (Resolve-FullPath -Path $routeCheckPath)
        }
        root_surface = [ordered]@{
            surface_id = "world_shelf_review"
            label = "drifted world shelf review"
            role = "grouped_review"
            declared_summary_schema = "system_compiler.world_shelf_review/v0"
            summary_schema = "system_compiler.world_shelf_review/v0"
            summary_kind = "system_compiler.world_shelf_review"
            summary_path = (Resolve-FullPath -Path $reviewSummaryPath)
            report_markdown_path = (Resolve-FullPath -Path $reviewReportPath)
            check_text_path = (Resolve-FullPath -Path $reviewCheckPath)
        }
        route_summary = [ordered]@{
            entry_count = 1
            unique_summary_count = 1
            repeated_entry_count = 0
            cycle_entry_count = 0
            leaf_entry_count = 1
            expanded_entry_count = 0
            max_depth = 0
        }
        route_provenance_summary = [ordered]@{
            entry_count = 0
            owner_count = 0
            unique_source_summary_count = 0
            unique_front_page_summary_count = 0
        }
        schema_counts = [ordered]@{
            "system_compiler.world_shelf_review/v0" = 1
        }
        role_counts = [ordered]@{
            grouped_review = 1
        }
        route_entries = @()
        route_provenance_entries = @()
        violations = @()
    }

    Write-JsonFile -Path $reviewSummaryPath -Value $reviewSummary
    Set-Content -LiteralPath $reviewReportPath -Value "# Opening Reason Drift Fixture`n" -Encoding utf8
    Set-Content -LiteralPath $reviewCheckPath -Value "drift_digest_changed: True`n" -Encoding utf8
    Write-JsonFile -Path $routeSummaryPath -Value $routeSummary
    Set-Content -LiteralPath $routeReportPath -Value "# Opening Reason Drift Fixture Route`n" -Encoding utf8
    Set-Content -LiteralPath $routeCheckPath -Value "fixture: opening_reason_drift`n" -Encoding utf8

    return $routeSummaryPath
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
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

$capabilityExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_capability.py"
$capabilityValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_capability.py"
$landingExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_landing.py"
$landingValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing.py"
foreach ($requiredPath in @($capabilityExportScript, $capabilityValidateScript, $landingExportScript, $landingValidateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $routeSummaryPath = New-WorldShelfReviewDriftRouteFixture -FixtureRoot (Join-Path $outputRootPath "_fixtures\world-review-drift-reason")
    $capabilityOutputRoot = Join-Path $outputRootPath "capability"
    $landingOutputRoot = Join-Path $outputRootPath "landing"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityExportScript, "--summary", $routeSummaryPath, "--output-root", $capabilityOutputRoot) `
        -FailureMessage "front page entry capability opening_reason export failed"

    $capabilitySummaryPath = Join-Path $capabilityOutputRoot "front-page.entry-capability.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityValidateScript, "--summary", $capabilitySummaryPath) `
        -FailureMessage "front page entry capability opening_reason validation failed"

    $capabilitySummary = Load-JsonObject -Path $capabilitySummaryPath
    Assert-Condition `
        -Condition ([string]$capabilitySummary.entry_status.opening_reason.kind -eq "world_shelf_review_drift") `
        -Message ("expected capability opening_reason kind world_shelf_review_drift but got '{0}'" -f $capabilitySummary.entry_status.opening_reason.kind)
    Assert-Condition `
        -Condition ([bool]$capabilitySummary.entry_status.opening_reason.drift_changed -eq $true) `
        -Message "expected capability opening_reason drift_changed true"
    Assert-Condition `
        -Condition ([string]$capabilitySummary.entry_status.opening_reason.drift_verdict -eq "drifted") `
        -Message ("expected capability opening_reason drift_verdict drifted but got '{0}'" -f $capabilitySummary.entry_status.opening_reason.drift_verdict)

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingExportScript, "--summary", $capabilitySummaryPath, "--output-root", $landingOutputRoot) `
        -FailureMessage "front page entry landing opening_reason export failed"

    $landingSummaryPath = Join-Path $landingOutputRoot "front-page.entry-landing.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingValidateScript, "--summary", $landingSummaryPath) `
        -FailureMessage "front page entry landing opening_reason validation failed"

    $landingSummary = Load-JsonObject -Path $landingSummaryPath
    Assert-Condition `
        -Condition (($landingSummary.landing_status.opening_reason | ConvertTo-Json -Depth 8 -Compress) -eq ($capabilitySummary.entry_status.opening_reason | ConvertTo-Json -Depth 8 -Compress)) `
        -Message "expected landing opening_reason to pass through capability opening_reason"

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-REASON-SMOKE] reason={0} drift_changed={1} verdict={2}" -f
        [string]$landingSummary.landing_status.opening_reason.kind,
        [bool]$landingSummary.landing_status.opening_reason.drift_changed,
        [string]$landingSummary.landing_status.opening_reason.drift_verdict
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-REASON-SMOKE] output_root={0}" -f $outputRootPath)
