param(
    [string]$ConsumerRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-plan-action-bridge-smoke",
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
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
foreach ($requiredPath in @($consumerSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "missing path: $requiredPath" }
}

Push-Location $repoRoot
try {
    $consumerSummaryPath = Join-Path $consumerRootPath "session-witness.inspect.compare.consumer.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $consumerSummaryPath)) {
        Write-Host "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-SMOKE] consumer_bootstrap=reuse-existing"
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

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--consumer",
            $consumerSummaryPath,
            "--output-root",
            $outputRootPath
        ) `
        -FailureMessage "runtime session opening bridge export failed"

    $summaryPath = Join-Path $outputRootPath "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $summaryPath) `
        -FailureMessage "runtime session opening bridge validation failed"

    $summary = Load-JsonObject -Path $summaryPath
    $consumer = Load-JsonObject -Path $consumerSummaryPath
    Assert-Condition `
        -Condition ([string]$summary.open_action.action_id -eq "open-default") `
        -Message ("expected open_action.action_id open-default but got '{0}'" -f $summary.open_action.action_id)
    Assert-Condition `
        -Condition ([string]$summary.open_action.entry_name -eq "runtime-session-inspect-consumer") `
        -Message ("expected entry_name runtime-session-inspect-consumer but got '{0}'" -f $summary.open_action.entry_name)
    Assert-Condition `
        -Condition ([string]$summary.open_action.expected_consumer_operation -eq "open-consumer-summary") `
        -Message ("expected operation open-consumer-summary but got '{0}'" -f $summary.open_action.expected_consumer_operation)
    Assert-Condition `
        -Condition ([string]$summary.artifact_target.selected_artifact_ref.id -eq [string]$consumer.default_explain_hop.artifact_ref.id) `
        -Message "selected artifact should follow consumer default explain hop artifact"
    Assert-Condition `
        -Condition ([string]$summary.opening_preview.headline -eq [string]$consumer.default_focus.headline) `
        -Message "opening preview headline should mirror default focus headline"
    Assert-Condition `
        -Condition ((@($summary.opening_preview.summary_lines) -join "|") -eq (@($consumer.default_focus.summary_lines) -join "|")) `
        -Message "opening preview summary lines should mirror default focus summary lines"
    Assert-Condition `
        -Condition ((@($summary.opening_preview.question_lines) -join "|") -eq (@($consumer.default_focus.question_lines) -join "|")) `
        -Message "opening preview question lines should mirror default focus question lines"

    Write-Host (
        "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-SMOKE] status={0} action={1} entry={2} target={3}" -f
        [string]$summary.open_action.status,
        [string]$summary.open_action.action_id,
        [string]$summary.open_action.entry_name,
        [string]$summary.artifact_target.selected_artifact_ref.id
    )
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-SMOKE] output_root={0}" -f $outputRootPath)
