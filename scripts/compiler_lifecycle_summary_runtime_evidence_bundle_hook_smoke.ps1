param(
    [string]$OutputRoot = "out/compiler-lifecycle-summary-runtime-evidence-bundle-hook-smoke",
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

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Assert-Equal {
    param(
        $Actual,
        $Expected,
        [string]$Label
    )

    if ($Actual -ne $Expected) {
        throw ("{0}: expected [{1}] but got [{2}]" -f $Label, $Expected, $Actual)
    }
}

function Read-Json {
    param(
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        throw "json not found: $Path"
    }

    return Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json
}

$outputRootPath = Resolve-FullPath -Path $OutputRoot
if ($Clean -and (Test-Path $outputRootPath)) {
    Remove-Item -LiteralPath $outputRootPath -Recurse -Force
}
Ensure-Directory -Path $outputRootPath

$bundleScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_evidence_bundle.ps1"
if (-not (Test-Path $bundleScript)) {
    throw "required script not found: $bundleScript"
}

$hostExamples = @(
    "runtime_minimal_host",
    "runtime_task_syscall_frame_host",
    "runtime_task_message_session_service_loop_host",
    "runtime_trap_armv7a_host"
)

$hookOutputRoot = Join-Path $outputRootPath "with_hook"
$noHookOutputRoot = Join-Path $outputRootPath "without_hook"

& $bundleScript `
    -OutputRoot $hookOutputRoot `
    -HostExamples $hostExamples `
    -SkipWitnessBundle `
    -ExportCompilerLifecycleSummary `
    -Clean

$hookSummaryPath = Join-Path $hookOutputRoot "summary.json"
$hookLifecyclePath = Join-Path $hookOutputRoot "compiler_lifecycle/compiler_lifecycle.summary.json"
$hookSummary = Read-Json -Path $hookSummaryPath
$hookLifecycle = Read-Json -Path $hookLifecyclePath

Assert-Equal -Actual ([string]$hookSummary.result) -Expected "ok" -Label "with_hook.bundle.result"
if ($null -eq $hookSummary.compiler_lifecycle_sidecar) {
    throw "with_hook.bundle missing compiler_lifecycle_sidecar"
}
Assert-Equal -Actual ([string]$hookSummary.compiler_lifecycle_sidecar.result) -Expected "ok" -Label "with_hook.sidecar.result"
Assert-Equal -Actual ([int]$hookSummary.compiler_lifecycle_sidecar.state_count) -Expected 9 -Label "with_hook.sidecar.state_count"
Assert-Equal -Actual ([string]$hookSummary.compiler_lifecycle_sidecar.frozen_status) -Expected "missing" -Label "with_hook.sidecar.frozen_status"
Assert-Equal -Actual ([string]$hookLifecycle.result) -Expected "ok" -Label "with_hook.lifecycle.result"
Assert-Equal -Actual ([int]$hookLifecycle.state_count) -Expected 9 -Label "with_hook.lifecycle.state_count"
Assert-Equal -Actual ([string]$hookLifecycle.states.frozen.status) -Expected "missing" -Label "with_hook.lifecycle.frozen.status"
Assert-Equal -Actual ([string]$hookLifecycle.states.lowered.projection_kind) -Expected "missing" -Label "with_hook.lifecycle.lowered.projection_kind"
Assert-Equal -Actual ([string]$hookLifecycle.states.archived.status) -Expected "missing" -Label "with_hook.lifecycle.archived.status"

& $bundleScript `
    -OutputRoot $noHookOutputRoot `
    -HostExamples $hostExamples `
    -SkipWitnessBundle `
    -Clean

$noHookSummaryPath = Join-Path $noHookOutputRoot "summary.json"
$noHookSummary = Read-Json -Path $noHookSummaryPath
Assert-Equal -Actual ([string]$noHookSummary.result) -Expected "ok" -Label "without_hook.bundle.result"
if ($null -ne $noHookSummary.compiler_lifecycle_sidecar) {
    throw "without_hook.bundle should not contain compiler_lifecycle_sidecar"
}

Write-Host "[COMPILER-LIFECYCLE-RUNTIME-EVIDENCE-BUNDLE-HOOK-SMOKE] result=ok"
Write-Host ("with_hook_summary={0}" -f $hookSummaryPath)
Write-Host ("with_hook_lifecycle={0}" -f $hookLifecyclePath)
Write-Host ("without_hook_summary={0}" -f $noHookSummaryPath)
