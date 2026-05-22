param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-route-sample-smoke",
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

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
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

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
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

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    Ensure-Directory -Path $parent
    $json = $Value | ConvertTo-Json -Depth 64
    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($json + [Environment]::NewLine)
}

function Write-Utf8Text {
    param(
        [string]$Path,
        [string]$Text
    )

    $parent = Split-Path -Parent $Path
    Ensure-Directory -Path $parent
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $Text
}

function New-LegacyRouteProvenanceFixture {
    param(
        [string]$FixtureRoot
    )

    $rootSummaryPath = Join-Path $FixtureRoot "legacy-root.summary.json"
    $rootReportPath = Join-Path $FixtureRoot "legacy-root.report.md"
    $rootCheckPath = Join-Path $FixtureRoot "legacy-root.check.txt"
    $producerSummaryPath = Join-Path $FixtureRoot "legacy-producer.summary.json"
    $producerReportPath = Join-Path $FixtureRoot "legacy-producer.report.md"
    $producerCheckPath = Join-Path $FixtureRoot "legacy-producer.check.txt"

    Write-Utf8Text -Path $rootReportPath -Text "# Legacy Root`n"
    Write-Utf8Text -Path $rootCheckPath -Text "legacy-root=ok`n"
    Write-Utf8Text -Path $producerReportPath -Text "# Legacy Producer`n"
    Write-Utf8Text -Path $producerCheckPath -Text "legacy-producer=ok`n"

    Write-JsonFile `
        -Path $rootSummaryPath `
        -Value ([ordered]@{
            schema = "system_compiler.legacy_front_page_root/v0"
            kind = "system_compiler.legacy_front_page_root"
            result = "ok"
            title = "Legacy front-page root fixture"
            front_page = [ordered]@{
                summary_path = (Resolve-FullPath -Path $rootSummaryPath)
                report_markdown_path = (Resolve-FullPath -Path $rootReportPath)
                check_text_path = (Resolve-FullPath -Path $rootCheckPath)
                supporting_surfaces = @()
            }
        })

    Write-JsonFile `
        -Path $producerSummaryPath `
        -Value ([ordered]@{
            schema = "system_compiler.legacy_route_producer/v0"
            kind = "system_compiler.legacy_route_producer"
            result = "ok"
            title = "Legacy route provenance producer fixture"
            front_page = [ordered]@{
                summary_path = (Resolve-FullPath -Path $producerSummaryPath)
                report_markdown_path = (Resolve-FullPath -Path $producerReportPath)
                check_text_path = (Resolve-FullPath -Path $producerCheckPath)
                supporting_surfaces = @()
            }
            route_provenance = @(
                [ordered]@{
                    id = "legacy_source_route"
                    route_kind = "front_page_route_root"
                    source_summary_schema = "system_compiler.front_page_route/v0"
                    source_summary_path = (Resolve-FullPath -Path $producerSummaryPath)
                    source_root_summary_path = (Resolve-FullPath -Path $rootSummaryPath)
                    source_report_markdown_path = (Resolve-FullPath -Path $rootReportPath)
                    source_check_text_path = (Resolve-FullPath -Path $rootCheckPath)
                    level1_surface_ids = @()
                }
            )
        })

    return (Resolve-FullPath -Path $producerSummaryPath)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$sampleSummaryPath = Resolve-FullPath -Path $SampleSummary
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $sampleSummaryPath)) {
    throw "sample summary not found: $sampleSummaryPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
foreach ($requiredPath in @($exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($exportScript, "--summary", $sampleSummaryPath, "--output-root", $outputRootPath) `
        -FailureMessage "front page route sample export failed"

    $routeSummaryPath = Join-Path $outputRootPath "front-page.route.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $routeSummaryPath) `
        -FailureMessage "front page route sample validation failed"

    $routeSummary = Load-JsonObject -Path $routeSummaryPath
    $level1SurfaceIds = @(
        @($routeSummary.route_entries) |
            Where-Object { [int]$_.depth -eq 1 } |
            ForEach-Object { [string]$_.surface_id }
    )
    $level1Joined = $level1SurfaceIds -join ","
    Assert-Condition `
        -Condition ($level1Joined -eq "runtime_evidence,kernel_runtime_session") `
        -Message ("expected sample level-1 surfaces 'runtime_evidence,kernel_runtime_session' but got '{0}'" -f $level1Joined)

    $sessionEntry = @(
        @($routeSummary.route_entries) |
            Where-Object {
                [string]$_.surface_id -eq "kernel_runtime_session" -and
                [string]$_.summary_schema -eq "minimal_kernel.kernel_runtime_session/v0" -and
                [string]$_.summary_kind -eq "minimal_kernel.kernel_runtime_session"
            }
    )
    Assert-Condition `
        -Condition ($sessionEntry.Count -eq 1) `
        -Message ("expected one opened kernel_runtime_session route entry but got {0}" -f $sessionEntry.Count)

    Write-Host ("[FRONT-PAGE-ROUTE-SAMPLE-SMOKE] summary={0}" -f $routeSummaryPath)
    Write-Host ("[FRONT-PAGE-ROUTE-SAMPLE-SMOKE] level1={0}" -f $level1Joined)

    $legacyFixtureRoot = Join-Path $outputRootPath "_legacy-route-provenance-fixture"
    $legacySummaryPath = New-LegacyRouteProvenanceFixture -FixtureRoot $legacyFixtureRoot
    $legacyOutputRoot = Join-Path $outputRootPath "legacy-route-provenance"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($exportScript, "--summary", $legacySummaryPath, "--output-root", $legacyOutputRoot) `
        -FailureMessage "front page route legacy provenance export failed"

    $legacyRouteSummaryPath = Join-Path $legacyOutputRoot "front-page.route.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $legacyRouteSummaryPath) `
        -FailureMessage "front page route legacy provenance validation failed"

    $legacyRouteSummary = Load-JsonObject -Path $legacyRouteSummaryPath
    $legacyRouteProvenanceEntries = @($legacyRouteSummary.route_provenance_entries)
    Assert-Condition `
        -Condition ($legacyRouteProvenanceEntries.Count -eq 1) `
        -Message ("expected one legacy route provenance entry but got {0}" -f $legacyRouteProvenanceEntries.Count)

    $legacyRouteProvenance = $legacyRouteProvenanceEntries[0]
    Assert-Condition `
        -Condition ([string]$legacyRouteProvenance.provenance_route_kind -eq "front_page_route_root") `
        -Message ("expected legacy route provenance kind front_page_route_root but got '{0}'" -f $legacyRouteProvenance.provenance_route_kind)
    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace([string]$legacyRouteProvenance.source_front_page_summary_path)) `
        -Message "expected legacy route provenance to populate source_front_page_summary_path"
    Assert-Condition `
        -Condition (Test-Path -LiteralPath ([string]$legacyRouteProvenance.source_front_page_summary_path)) `
        -Message ("expected normalized source_front_page_summary_path to exist: {0}" -f $legacyRouteProvenance.source_front_page_summary_path)
    Assert-Condition `
        -Condition (Test-Path -LiteralPath ([string]$legacyRouteProvenance.source_front_page_report_markdown_path)) `
        -Message ("expected normalized source_front_page_report_markdown_path to exist: {0}" -f $legacyRouteProvenance.source_front_page_report_markdown_path)
    Assert-Condition `
        -Condition (Test-Path -LiteralPath ([string]$legacyRouteProvenance.source_front_page_check_text_path)) `
        -Message ("expected normalized source_front_page_check_text_path to exist: {0}" -f $legacyRouteProvenance.source_front_page_check_text_path)

    Write-Host ("[FRONT-PAGE-ROUTE-SAMPLE-SMOKE] legacy_route_provenance={0}" -f $legacyRouteProvenance.provenance_route_kind)
    Write-Host "ok=1"
} finally {
    Pop-Location
}
