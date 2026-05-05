param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-sample-smoke",
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

$routeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_route.py"
$routeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_route.py"
$capabilityExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_capability.py"
$capabilityValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_capability.py"
$landingExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_landing.py"
$landingValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing.py"
foreach ($requiredPath in @(
    $routeExportScript,
    $routeValidateScript,
    $capabilityExportScript,
    $capabilityValidateScript,
    $landingExportScript,
    $landingValidateScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $routeOutputRoot = Join-Path $outputRootPath "route"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($routeExportScript, "--summary", $sampleSummaryPath, "--output-root", $routeOutputRoot) `
        -FailureMessage "front page route sample export failed"

    $routeSummaryPath = Join-Path $routeOutputRoot "front-page.route.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($routeValidateScript, "--summary", $routeSummaryPath) `
        -FailureMessage "front page route sample validation failed"

    $capabilityOutputRoot = Join-Path $outputRootPath "capability"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityExportScript, "--summary", $routeSummaryPath, "--output-root", $capabilityOutputRoot) `
        -FailureMessage "front page entry capability sample export failed"

    $capabilitySummaryPath = Join-Path $capabilityOutputRoot "front-page.entry-capability.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($capabilityValidateScript, "--summary", $capabilitySummaryPath) `
        -FailureMessage "front page entry capability sample validation failed"

    $capabilitySummary = Load-JsonObject -Path $capabilitySummaryPath
    $availableCapabilities = @([string[]]$capabilitySummary.capability_summary.available_capability_ids)
    Assert-Condition `
        -Condition ($availableCapabilities -contains "runtime_session") `
        -Message ("expected runtime_session capability but got '{0}'" -f ($availableCapabilities -join ","))
    Assert-Condition `
        -Condition ([bool]$capabilitySummary.capability_summary.capability_flags.runtime_session) `
        -Message "expected capability_flags.runtime_session=true"
    Assert-Condition `
        -Condition ([int]$capabilitySummary.capability_summary.capability_counts.runtime_session -eq 1) `
        -Message ("expected capability_counts.runtime_session=1 but got {0}" -f $capabilitySummary.capability_summary.capability_counts.runtime_session)

    $runtimeSessionEntry = $capabilitySummary.capability_summary.preferred_entries.runtime_session
    Assert-Condition `
        -Condition ($runtimeSessionEntry -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionEntry -is [hashtable]) `
        -Message "expected preferred_entries.runtime_session object"
    Assert-Condition `
        -Condition ([string]$runtimeSessionEntry.surface_id -eq "kernel_runtime_session") `
        -Message ("expected runtime_session surface_id kernel_runtime_session but got '{0}'" -f $runtimeSessionEntry.surface_id)
    Assert-Condition `
        -Condition ([string]$runtimeSessionEntry.summary_schema -eq "minimal_kernel.kernel_runtime_session/v0") `
        -Message ("expected runtime_session summary_schema minimal_kernel.kernel_runtime_session/v0 but got '{0}'" -f $runtimeSessionEntry.summary_schema)

    $landingOutputRoot = Join-Path $outputRootPath "landing"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingExportScript, "--summary", $capabilitySummaryPath, "--output-root", $landingOutputRoot) `
        -FailureMessage "front page entry landing sample export failed"

    $landingSummaryPath = Join-Path $landingOutputRoot "front-page.entry-landing.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingValidateScript, "--summary", $landingSummaryPath) `
        -FailureMessage "front page entry landing sample validation failed"

    $landingSummary = Load-JsonObject -Path $landingSummaryPath
    $availableTabs = @([string[]]$landingSummary.landing_status.available_tab_ids)
    Assert-Condition `
        -Condition ($availableTabs -contains "runtime_session") `
        -Message ("expected runtime_session landing tab but got '{0}'" -f ($availableTabs -join ","))
    Assert-Condition `
        -Condition ([bool]$landingSummary.landing_status.direct_runtime_session_available) `
        -Message "expected landing_status.direct_runtime_session_available=true"

    $runtimeSessionQuery = @($landingSummary.query_hints.tab_queries) |
        Where-Object { [string]$_.tab_id -eq "runtime_session" } |
        Select-Object -First 1
    Assert-Condition `
        -Condition ($runtimeSessionQuery -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionQuery -is [hashtable]) `
        -Message "expected runtime_session query hint"
    Assert-Condition `
        -Condition ([string]$runtimeSessionQuery.query_kind -eq "bringup_evidence") `
        -Message ("expected runtime_session query_kind bringup_evidence but got '{0}'" -f $runtimeSessionQuery.query_kind)
    Assert-Condition `
        -Condition ([string]$runtimeSessionQuery.scope -eq "report") `
        -Message ("expected runtime_session query scope report but got '{0}'" -f $runtimeSessionQuery.scope)

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-SAMPLE-SMOKE] route_summary={0}" -f $routeSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-SAMPLE-SMOKE] capability_summary={0}" -f $capabilitySummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-SAMPLE-SMOKE] landing_summary={0}" -f $landingSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-SAMPLE-SMOKE] capabilities={0}" -f ($availableCapabilities -join ","))
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-SAMPLE-SMOKE] tabs={0}" -f ($availableTabs -join ","))
    Write-Host "ok=1"
} finally {
    Pop-Location
}
