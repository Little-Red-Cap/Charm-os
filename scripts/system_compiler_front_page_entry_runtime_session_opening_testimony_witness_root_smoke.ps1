param(
    [string]$RuntimeSessionOpenEventWitnessRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-open-event-witness-smoke",
    [string]$ConsumerRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-testimony-witness-root-smoke",
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

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Load-JsonObject {
    param([string]$Path)
    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Write-JsonFile {
    param(
        [string]$Path,
        $Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
    $json = $Value | ConvertTo-Json -Depth 100
    Set-Content -LiteralPath $Path -Encoding utf8 -Value ($json + [Environment]::NewLine)
}

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function New-CleanConsumerFixture {
    param(
        [string]$SourceConsumerPath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceConsumerPath
    $sourceCompare = $summary.source_compare
    $sourceCompare.changed = $false
    $sourceCompare.source_result = "ok"
    $sourceCompare.baseline_result = "ok"
    $sourceCompare.current_result = "ok"
    $sourceCompare.baseline_session_status = "standing"
    $sourceCompare.current_session_status = "standing"
    $sourceCompare.baseline_failure_domain = ""
    $sourceCompare.current_failure_domain = ""
    $sourceCompare.runtime_regression_count = 0
    $sourceCompare.runtime_improvement_count = 0
    $sourceCompare.world_failure_code_delta_count = 0
    $sourceCompare.witness_failure_code_delta_count = 0
    $sourceCompare.violation_delta_count = 0
    $summary.source_compare = $sourceCompare
    $summary.artifact_context.consumer_summary_path = (Resolve-FullPath -Path $OutputPath)
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

function New-BlockedWitnessFixture {
    param(
        [string]$SourceWitnessPath,
        [string]$OutputPath
    )

    $summary = Load-JsonObject -Path $SourceWitnessPath
    $summary.explanation.text_lines = @()
    $summary.artifact_context.open_event_witness_summary_path = (Resolve-FullPath -Path $OutputPath)
    $summary.front_page.summary_path = (Resolve-FullPath -Path $OutputPath)
    Write-JsonFile -Path $OutputPath -Value $summary
    return (Resolve-FullPath -Path $OutputPath)
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$runtimeSessionOpenEventWitnessRootPath = Resolve-FullPath -Path $RuntimeSessionOpenEventWitnessRoot
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
$runtimeSessionOpenEventWitnessSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1"
$bridgeExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$bridgeValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$openEventExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$openEventValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py"
$witnessExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
$witnessValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py"
foreach ($requiredPath in @(
    $consumerSmokeScript,
    $runtimeSessionOpenEventWitnessSmokeScript,
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
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-WITNESS-ROOT-SMOKE] consumer_bootstrap=reuse-existing"
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

    $driftWitnessPath = Join-Path $runtimeSessionOpenEventWitnessRootPath "witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $driftWitnessPath)) {
        Write-Host "[RUNTIME-SESSION-OPENING-TESTIMONY-WITNESS-ROOT-SMOKE] drift_witness_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $runtimeSessionOpenEventWitnessSmokeScript,
                "-ConsumerRoot",
                $consumerRootPath,
                "-OutputRoot",
                $runtimeSessionOpenEventWitnessRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session open-event witness smoke bootstrap failed"
    }

    $cleanConsumerPath = New-CleanConsumerFixture `
        -SourceConsumerPath $consumerSummaryPath `
        -OutputPath (Join-Path $outputRootPath "_fixtures\clean-consumer\session-witness.inspect.compare.consumer.summary.json")

    $cleanBridgeRoot = Join-Path $outputRootPath "_clean-generated\bridge"
    $cleanOpenEventRoot = Join-Path $outputRootPath "_clean-generated\open-event"
    $cleanWitnessRoot = Join-Path $outputRootPath "_clean-generated\witness"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $bridgeExportScript,
            "--consumer",
            $cleanConsumerPath,
            "--output-root",
            $cleanBridgeRoot
        ) `
        -FailureMessage "runtime session clean bridge export failed"

    $cleanBridgeSummaryPath = Join-Path $cleanBridgeRoot "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($bridgeValidateScript, "--summary", $cleanBridgeSummaryPath) `
        -FailureMessage "runtime session clean bridge validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $openEventExportScript,
            "--bridge",
            $cleanBridgeSummaryPath,
            "--output-root",
            $cleanOpenEventRoot
        ) `
        -FailureMessage "runtime session clean open-event export failed"

    $cleanOpenEventSummaryPath = Join-Path $cleanOpenEventRoot "front-page.entry-opening-flow.open-event.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($openEventValidateScript, "--summary", $cleanOpenEventSummaryPath) `
        -FailureMessage "runtime session clean open-event validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $witnessExportScript,
            "--open-event",
            $cleanOpenEventSummaryPath,
            "--output-root",
            $cleanWitnessRoot
        ) `
        -FailureMessage "runtime session clean open-event witness export failed"

    $cleanGeneratedWitnessPath = Join-Path $cleanWitnessRoot "front-page.entry-opening-flow.open-event.witness.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($witnessValidateScript, "--summary", $cleanGeneratedWitnessPath) `
        -FailureMessage "runtime session clean open-event witness validation failed"

    $cleanWitnessPath = Join-Path $outputRootPath "default-no-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    Ensure-Directory -Path (Split-Path -Parent $cleanWitnessPath)
    Copy-Item -LiteralPath $cleanGeneratedWitnessPath -Destination $cleanWitnessPath -Force

    $driftCopiedWitnessPath = Join-Path $outputRootPath "default-with-drift-compare-witness\front-page.entry-opening-flow.open-event.witness.summary.json"
    Ensure-Directory -Path (Split-Path -Parent $driftCopiedWitnessPath)
    Copy-Item -LiteralPath $driftWitnessPath -Destination $driftCopiedWitnessPath -Force

    $blockedWitnessPath = New-BlockedWitnessFixture `
        -SourceWitnessPath $cleanWitnessPath `
        -OutputPath (Join-Path $outputRootPath "blocked-empty-explanation-witness\front-page.entry-opening-flow.open-event.witness.summary.json")

    $cleanWitness = Load-JsonObject -Path $cleanWitnessPath
    $driftWitness = Load-JsonObject -Path $driftCopiedWitnessPath
    $blockedWitness = Load-JsonObject -Path $blockedWitnessPath

    Assert-Condition `
        -Condition ([string]$cleanWitness.open_event_identity.open_event_status -eq "accepted") `
        -Message "runtime-session clean witness should expose accepted open event status"
    Assert-Condition `
        -Condition ([string]$driftWitness.open_event_identity.open_event_status -eq "accepted_with_drift") `
        -Message "runtime-session drift witness should expose accepted_with_drift open event status"
    Assert-Condition `
        -Condition ([string]$cleanWitness.judgment.compare_verdict -eq "standing") `
        -Message "runtime-session clean witness should expose standing compare verdict"
    Assert-Condition `
        -Condition ([string]$driftWitness.judgment.compare_verdict -eq "drifted") `
        -Message "runtime-session drift witness should expose drifted compare verdict"
    Assert-Condition `
        -Condition (@($blockedWitness.explanation.text_lines).Count -eq 0) `
        -Message "runtime-session blocked witness fixture should clear explanation lines"

    Write-Host (
        "[RUNTIME-SESSION-OPENING-TESTIMONY-WITNESS-ROOT-SMOKE] clean={0} drift={1} blocked={2}" -f
        [string]$cleanWitness.open_event_identity.open_event_status,
        [string]$driftWitness.open_event_identity.open_event_status,
        [int]@($blockedWitness.explanation.text_lines).Count
    )
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-TESTIMONY-WITNESS-ROOT-SMOKE] output_root={0}" -f $outputRootPath)
