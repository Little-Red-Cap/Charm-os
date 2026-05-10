param(
    [string]$Summary = "",
    [string]$OutputRoot = "",
    [string]$PythonExe = "python"
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

function Require-Line {
    param(
        [string[]]$Lines,
        [string]$Needle
    )

    if (-not (@($Lines) | Where-Object { $_ -like "*$Needle*" })) {
        throw "expected inspect output to contain: $Needle"
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$consumerSmoke = Join-Path $PSScriptRoot "system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1"
$inspectScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1"

foreach ($path in @($inspectScript)) {
    if (-not (Test-Path $path)) {
        throw "missing inspect consumer dependency: $path"
    }
}

$summaryPath = ""
if (-not [string]::IsNullOrWhiteSpace($Summary)) {
    $summaryPath = Resolve-FullPath -Path $Summary
    if (-not (Test-Path $summaryPath)) {
        throw "missing consumer summary: $summaryPath"
    }

    Write-Host "[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] bootstrap=reuse-existing-summary"
} else {
    if (-not (Test-Path $consumerSmoke)) {
        throw "missing inspect consumer dependency: $consumerSmoke"
    }

    $resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
        Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-witness-inspect-consumer-smoke"
    } else {
        Resolve-FullPath -Path $OutputRoot
    }

    if (Test-Path $resolvedOutputRoot) {
        Remove-Item -LiteralPath $resolvedOutputRoot -Recurse -Force
    }
    Ensure-Directory -Path $resolvedOutputRoot

    $consumerRoot = Join-Path $resolvedOutputRoot "consumer"
    Push-Location $repoRoot
    try {
        & $consumerSmoke -OutputRoot $consumerRoot -PythonExe $PythonExe -Clean
    } finally {
        Pop-Location
    }

    $summaryPath = Join-Path $consumerRoot "session-witness.inspect.compare.consumer.summary.json"
    if (-not (Test-Path $summaryPath)) {
        throw "missing consumer summary: $summaryPath"
    }
}

$defaultOutput = @(
    & $inspectScript -Summary $summaryPath -ShowFallbacks
)

Require-Line -Lines $defaultOutput -Needle "default_focus: session-state-drift kind=session_state_drift severity=critical"
Require-Line -Lines $defaultOutput -Needle "default_explain_hop: artifact=session-report reason=session_report"
Require-Line -Lines $defaultOutput -Needle "selected_focus: session-state-drift kind=session_state_drift severity=critical changed=true"
Require-Line -Lines $defaultOutput -Needle "selected_explain_hop: artifact=session-report reason=session_report"
Require-Line -Lines $defaultOutput -Needle "fallback_explain_hop: focus=runtime-regression artifact=runtime-ledger reason=runtime_ledger"

$runtimeOutput = @(
    & $inspectScript -Summary $summaryPath -FocusId "runtime-regression" -ShowArtifacts -ShowFallbacks
)

Require-Line -Lines $runtimeOutput -Needle "selected_focus: runtime-regression kind=runtime_regression severity=high changed=true"
Require-Line -Lines $runtimeOutput -Needle "selected_explain_hop: artifact=runtime-ledger reason=runtime_ledger"
Require-Line -Lines $runtimeOutput -Needle "selected_explain_fallback: artifact=session-report"

$jsonText = & $inspectScript -Summary $summaryPath -FocusId "world-compare-drift" -AsJson
$json = $jsonText | ConvertFrom-Json
if ([string]$json.selected_focus.focus_id -ne "world-compare-drift") {
    throw "expected selected focus world-compare-drift in JSON inspect view"
}
if ([string]$json.selected_explain_hop.artifact_ref.id -ne "world-compare-report") {
    throw "expected selected explain hop world-compare-report in JSON inspect view"
}

Write-Host "==> inspect minimal kernel runtime session witness inspect compare consumer smoke"
Write-Host ("summary={0}" -f $summaryPath)
Write-Host ("default_focus={0}" -f [string]$json.default_focus.focus_id)
Write-Host ("selected_focus={0}" -f [string]$json.selected_focus.focus_id)
Write-Host ("selected_explain_hop={0}" -f [string]$json.selected_explain_hop.artifact_ref.id)
Write-Host "ok=1"
