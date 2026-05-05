param(
    [string]$ConsumerRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-open-event-witness-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    if (-not (Test-Path -LiteralPath $Path)) { New-Item -ItemType Directory -Path $Path -Force | Out-Null }
}

function Remove-PathIfExists {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
}

function Resolve-ToolPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) { return $command.Source }
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
    if ($exitCode -ne 0) { throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode) }
}

function Load-JsonObject {
    param([string]$Path)
    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$consumerRootPath = Resolve-FullPath -Path $ConsumerRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) { Remove-PathIfExists -Path $outputRootPath }
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$consumerSmokeScript = Join-Path $PSScriptRoot "system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1"
$bridgeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$bridgeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$openEventExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$witnessExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$witnessValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
foreach ($requiredPath in @(
    $consumerSmokeScript,
    $bridgeExportScript,
    $bridgeValidateScript,
    $openEventExportScript,
    $openEventValidateScript,
    $witnessExportScript,
    $witnessValidateScript
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "missing path: $requiredPath" }
}

Push-Location $repoRoot
try {
    $consumerSummaryPath = Join-Path $consumerRootPath "session-witness.inspect.compare.consumer.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $consumerSummaryPath)) {
        Write-Host "[RUNTIME-SESSION-OPEN-EVENT-WITNESS-SMOKE] consumer_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $consumerSmokeScript,
                "-OutputRoot",
                $consumerRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session inspect compare consumer smoke bootstrap failed"
    }

    $bridgeRoot = Join-Path $outputRootPath "bridge"
    $openEventRoot = Join-Path $outputRootPath "open-event"
    $witnessRoot = Join-Path $outputRootPath "witness"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $bridgeExportScript,
            "--consumer",
            $consumerSummaryPath,
            "--output-root",
            $bridgeRoot
        ) `
        -FailureMessage "runtime session bridge export failed"

    $bridgeSummaryPath = Join-Path $bridgeRoot "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($bridgeValidateScript, "--summary", $bridgeSummaryPath) `
        -FailureMessage "runtime session bridge validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $openEventExportScript,
            "--bridge",
            $bridgeSummaryPath,
            "--output-root",
            $openEventRoot
        ) `
        -FailureMessage "runtime session open-event export failed"

    $openEventSummaryPath = Join-Path $openEventRoot "front-page.entry-opening-flow.open-event.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openEventValidateScript, "--summary", $openEventSummaryPath) `
        -FailureMessage "runtime session open-event validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $witnessExportScript,
            "--open-event",
            $openEventSummaryPath,
            "--output-root",
            $witnessRoot
        ) `
        -FailureMessage "runtime session open-event witness export failed"

    $witnessSummaryPath = Join-Path $witnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($witnessValidateScript, "--summary", $witnessSummaryPath) `
        -FailureMessage "runtime session open-event witness validation failed"

    $openEvent = Load-JsonObject -Path $openEventSummaryPath
    $witness = Load-JsonObject -Path $witnessSummaryPath
    Assert-Condition `
        -Condition ([string]$openEvent.workspace_facade.primary_summary_path -eq [string]$openEvent.front_page.supporting_surfaces[1].summary_path) `
        -Message "workspace facade primary summary should point to consumer facade"
    Assert-Condition `
        -Condition ([string]$openEvent.open_event.source_artifact.summary_path -eq [string]$openEvent.open_event.opening_input_refs.selected_artifact_ref.path) `
        -Message "source artifact summary path should follow selected artifact ref"
    Assert-Condition `
        -Condition ([string]$openEvent.open_event.status -eq "accepted_with_drift") `
        -Message ("expected accepted_with_drift open event but got '{0}'" -f $openEvent.open_event.status)
    Assert-Condition `
        -Condition ([string]$witness.result -eq "ok") `
        -Message ("expected witness result ok but got '{0}'" -f $witness.result)
    Assert-Condition `
        -Condition ([string]$witness.judgment.witness_status -eq "ok") `
        -Message ("expected witness status ok but got '{0}'" -f $witness.judgment.witness_status)
    Assert-Condition `
        -Condition (@($witness.evidence_refs | Where-Object { [string]$_.role -eq "consumer_summary_ref" }).Count -eq 1) `
        -Message "expected consumer summary ref in witness evidence refs"
    Assert-Condition `
        -Condition (@($witness.evidence_refs | Where-Object { [string]$_.role -eq "selected_artifact_ref" }).Count -eq 1) `
        -Message "expected selected artifact ref in witness evidence refs"
    Assert-Condition `
        -Condition (@($openEvent.open_event.opening_input_refs.fallback_artifact_refs).Count -ge 1) `
        -Message "expected fallback artifact refs to remain attached"

    Write-Host (
        "[RUNTIME-SESSION-OPEN-EVENT-WITNESS-SMOKE] event_status={0} witness_status={1} selected_artifact={2}" -f
        [string]$openEvent.open_event.status,
        [string]$witness.judgment.witness_status,
        [string]$openEvent.open_event.opening_input_refs.selected_artifact_ref.id
    )
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPEN-EVENT-WITNESS-SMOKE] output_root={0}" -f $outputRootPath)
