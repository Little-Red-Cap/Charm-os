param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opener-sample-smoke",
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

function Select-RuntimeSessionPrimaryLanding {
    param(
        [string]$SourceLandingSummaryPath,
        [string]$OutputDirectory
    )

    Ensure-Directory -Path $OutputDirectory

    $landing = Load-JsonObject -Path $SourceLandingSummaryPath
    $runtimeSessionTab = @($landing.landing_tabs) |
        Where-Object { [string]$_.tab_id -eq "runtime_session" } |
        Select-Object -First 1
    $runtimeSessionQuery = @($landing.query_hints.tab_queries) |
        Where-Object { [string]$_.tab_id -eq "runtime_session" } |
        Select-Object -First 1
    Assert-Condition `
        -Condition ($runtimeSessionTab -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionTab -is [hashtable]) `
        -Message "runtime_session landing tab not found"
    Assert-Condition `
        -Condition ($runtimeSessionQuery -is [System.Management.Automation.PSCustomObject] -or $runtimeSessionQuery -is [hashtable]) `
        -Message "runtime_session query hint not found"

    $summaryPath = Join-Path $OutputDirectory "front-page.entry-landing.summary.json"
    $reportPath = Join-Path $OutputDirectory "front-page.entry-landing.report.md"
    $checkPath = Join-Path $OutputDirectory "front-page.entry-landing.check.txt"

    $sourceReport = [string]$landing.artifact_context.report_markdown_path
    $sourceCheck = [string]$landing.artifact_context.check_text_path
    if (Test-Path $sourceReport) {
        Copy-Item -LiteralPath $sourceReport -Destination $reportPath -Force
    } else {
        Set-Content -LiteralPath $reportPath -Value "# Runtime Session Primary Landing`n" -Encoding utf8
    }
    if (Test-Path $sourceCheck) {
        Copy-Item -LiteralPath $sourceCheck -Destination $checkPath -Force
    } else {
        Set-Content -LiteralPath $checkPath -Value "runtime_session_primary: true`n" -Encoding utf8
    }

    $landing.artifact_context.output_root = $OutputDirectory
    $landing.artifact_context.landing_summary_path = $summaryPath
    $landing.artifact_context.report_markdown_path = $reportPath
    $landing.artifact_context.check_text_path = $checkPath
    $landing.front_page.summary_path = $summaryPath
    $landing.front_page.report_markdown_path = $reportPath
    $landing.front_page.check_text_path = $checkPath
    $landing.primary_landing = $runtimeSessionTab
    $landing.query_hints.primary_query = $runtimeSessionQuery
    $landing.landing_status.primary_tab_id = "runtime_session"
    $landing.landing_status.primary_summary_schema = [string]$runtimeSessionTab.entry.summary_schema
    $landing.landing_status.primary_summary_kind = [string]$runtimeSessionTab.entry.summary_kind

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
$openerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$openerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @(
    $routeExportScript,
    $routeValidateScript,
    $capabilityExportScript,
    $capabilityValidateScript,
    $landingExportScript,
    $landingValidateScript,
    $openerExportScript,
    $openerValidateScript
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

    $landingSummaryPath = Join-Path $landingOutputRoot "front-page.entry-landing.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($landingValidateScript, "--summary", $landingSummaryPath) `
        -FailureMessage "front page entry landing sample validation failed"

    $runtimeSessionLandingPath = Select-RuntimeSessionPrimaryLanding `
        -SourceLandingSummaryPath $landingSummaryPath `
        -OutputDirectory (Join-Path $outputRootPath "runtime-session-landing")

    $openerOutputRoot = Join-Path $outputRootPath "opener"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openerExportScript, "--landing", $runtimeSessionLandingPath, "--output-root", $openerOutputRoot) `
        -FailureMessage "front page entry opener runtime session sample export failed"

    $openerSummaryPath = Join-Path $openerOutputRoot "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openerValidateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener runtime session sample validation failed"

    $openerSummary = Load-JsonObject -Path $openerSummaryPath
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.selected_tab_id -eq "runtime_session") `
        -Message ("expected selected_tab_id runtime_session but got '{0}'" -f $openerSummary.open_action.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$openerSummary.open_action.query_kind -eq "bringup_evidence") `
        -Message ("expected query_kind bringup_evidence but got '{0}'" -f $openerSummary.open_action.query_kind)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.status -eq "available") `
        -Message ("expected projection status available but got '{0}'" -f $openerSummary.opened_projection.status)
    Assert-Condition `
        -Condition ([string]$openerSummary.opened_projection.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected projection kind kernel_runtime_session_overview but got '{0}'" -f $openerSummary.opened_projection.projection_kind)
    Assert-Condition `
        -Condition (@($openerSummary.opened_projection.summary_lines).Count -gt 0) `
        -Message "expected runtime session projection summary lines"
    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace([string]$openerSummary.opened_projection.headline)) `
        -Message "expected runtime session projection headline"

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENER-SAMPLE-SMOKE] landing={0}" -f $runtimeSessionLandingPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENER-SAMPLE-SMOKE] opener={0}" -f $openerSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENER-SAMPLE-SMOKE] tab={0}" -f $openerSummary.open_action.selected_tab_id)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-OPENER-SAMPLE-SMOKE] projection={0}/{1}" -f $openerSummary.opened_projection.status, $openerSummary.opened_projection.projection_kind)
    Write-Host "ok=1"
} finally {
    Pop-Location
}
