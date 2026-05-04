param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-compare-sample-smoke",
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

function Write-JsonObject {
    param(
        [string]$Path,
        [object]$Value
    )

    $json = $Value | ConvertTo-Json -Depth 100
    Set-Content -LiteralPath $Path -Value $json -Encoding utf8
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

function Remove-RuntimeSessionLanding {
    param(
        [string]$SourceLandingSummaryPath,
        [string]$OutputDirectory
    )

    Ensure-Directory -Path $OutputDirectory

    $landing = Load-JsonObject -Path $SourceLandingSummaryPath
    $summaryPath = Join-Path $OutputDirectory "front-page.entry-landing.summary.json"
    $reportPath = Join-Path $OutputDirectory "front-page.entry-landing.report.md"
    $checkPath = Join-Path $OutputDirectory "front-page.entry-landing.check.txt"

    $sourceReport = [string]$landing.artifact_context.report_markdown_path
    $sourceCheck = [string]$landing.artifact_context.check_text_path
    if (Test-Path $sourceReport) {
        Copy-Item -LiteralPath $sourceReport -Destination $reportPath -Force
    } else {
        Set-Content -LiteralPath $reportPath -Value "# Runtime Session Removed Landing`n" -Encoding utf8
    }
    if (Test-Path $sourceCheck) {
        Copy-Item -LiteralPath $sourceCheck -Destination $checkPath -Force
    } else {
        Set-Content -LiteralPath $checkPath -Value "runtime_session_removed: true`n" -Encoding utf8
    }

    $landing.artifact_context.output_root = $OutputDirectory
    $landing.artifact_context.landing_summary_path = $summaryPath
    $landing.artifact_context.report_markdown_path = $reportPath
    $landing.artifact_context.check_text_path = $checkPath
    $landing.front_page.summary_path = $summaryPath
    $landing.front_page.report_markdown_path = $reportPath
    $landing.front_page.check_text_path = $checkPath

    $landing.landing_tabs = @(
        @($landing.landing_tabs) |
            Where-Object { [string]$_.tab_id -ne "runtime_session" }
    )
    $landing.secondary_landings = @(
        @($landing.secondary_landings) |
            Where-Object { [string]$_.tab_id -ne "runtime_session" }
    )
    $landing.landing_status.available_tab_ids = @(
        @($landing.landing_status.available_tab_ids) |
            Where-Object { [string]$_ -ne "runtime_session" }
    )
    $landing.landing_status.fallback_tab_ids = @(
        @($landing.landing_status.fallback_tab_ids) |
            Where-Object { [string]$_ -ne "runtime_session" }
    )
    $landing.landing_status.tab_count = @($landing.landing_tabs).Count
    $landing.landing_status.fallback_tab_count = @($landing.landing_status.fallback_tab_ids).Count
    $landing.landing_status.direct_runtime_session_available = $false
    $landing.query_hints.tab_queries = @(
        @($landing.query_hints.tab_queries) |
            Where-Object { [string]$_.tab_id -ne "runtime_session" }
    )

    Write-JsonObject -Path $summaryPath -Value $landing
    return $summaryPath
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
$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_landing.py"
$compareValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_landing_compare.py"
foreach ($requiredPath in @(
    $routeExportScript,
    $routeValidateScript,
    $capabilityExportScript,
    $capabilityValidateScript,
    $landingExportScript,
    $landingValidateScript,
    $compareScript,
    $compareValidateScript
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

    $landingOutputRoot = Join-Path $outputRootPath "landing"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingExportScript, "--summary", $capabilitySummaryPath, "--output-root", $landingOutputRoot) `
        -FailureMessage "front page entry landing sample export failed"

    $fullLandingSummaryPath = Join-Path $landingOutputRoot "front-page.entry-landing.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingValidateScript, "--summary", $fullLandingSummaryPath) `
        -FailureMessage "front page entry landing sample validation failed"

    $withoutRuntimeSessionSummaryPath = Remove-RuntimeSessionLanding `
        -SourceLandingSummaryPath $fullLandingSummaryPath `
        -OutputDirectory (Join-Path $outputRootPath "landing-without-runtime-session")

    $compareOutputRoot = Join-Path $outputRootPath "compare-without-to-with"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareScript, "--baseline", $withoutRuntimeSessionSummaryPath, "--candidate", $fullLandingSummaryPath, "--output-root", $compareOutputRoot) `
        -FailureMessage "front page entry landing compare sample export failed"

    $compareSummaryPath = Join-Path $compareOutputRoot "front-page.entry-landing.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareValidateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "front page entry landing compare sample validation failed"

    $compareSummary = Load-JsonObject -Path $compareSummaryPath
    $addedDirectModes = @([string[]]$compareSummary.landing_changes.direct_capability_changes.added)
    $addedTabs = @([string[]]$compareSummary.landing_changes.available_tab_changes.added)
    Assert-Condition `
        -Condition ($addedDirectModes -contains "runtime_session") `
        -Message ("expected runtime_session added direct mode but got '{0}'" -f ($addedDirectModes -join ","))
    Assert-Condition `
        -Condition ($addedTabs -contains "runtime_session") `
        -Message ("expected runtime_session added tab but got '{0}'" -f ($addedTabs -join ","))
    Assert-Condition `
        -Condition ([bool]$compareSummary.landing_status.candidate_direct_runtime_session_available) `
        -Message "expected candidate_direct_runtime_session_available=true"
    Assert-Condition `
        -Condition (-not [bool]$compareSummary.landing_status.baseline_direct_runtime_session_available) `
        -Message "expected baseline_direct_runtime_session_available=false"

    $compareOutputRootReverse = Join-Path $outputRootPath "compare-with-to-without"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareScript, "--baseline", $fullLandingSummaryPath, "--candidate", $withoutRuntimeSessionSummaryPath, "--output-root", $compareOutputRootReverse) `
        -FailureMessage "front page entry landing reverse compare sample export failed"

    $reverseCompareSummaryPath = Join-Path $compareOutputRootReverse "front-page.entry-landing.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($compareValidateScript, "--summary", $reverseCompareSummaryPath) `
        -FailureMessage "front page entry landing reverse compare sample validation failed"

    $reverseCompareSummary = Load-JsonObject -Path $reverseCompareSummaryPath
    $removedDirectModes = @([string[]]$reverseCompareSummary.landing_changes.direct_capability_changes.removed)
    $lostDirectModes = @([string[]]$reverseCompareSummary.landing_regression_surface.lost_direct_modes)
    Assert-Condition `
        -Condition ($removedDirectModes -contains "runtime_session") `
        -Message ("expected runtime_session removed direct mode but got '{0}'" -f ($removedDirectModes -join ","))
    Assert-Condition `
        -Condition ($lostDirectModes -contains "runtime_session") `
        -Message ("expected runtime_session lost direct mode but got '{0}'" -f ($lostDirectModes -join ","))

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-COMPARE-SAMPLE-SMOKE] full_landing={0}" -f $fullLandingSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-COMPARE-SAMPLE-SMOKE] without_runtime_session={0}" -f $withoutRuntimeSessionSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-COMPARE-SAMPLE-SMOKE] added_direct_modes={0}" -f ($addedDirectModes -join ","))
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-COMPARE-SAMPLE-SMOKE] removed_direct_modes={0}" -f ($removedDirectModes -join ","))
    Write-Host "ok=1"
} finally {
    Pop-Location
}
