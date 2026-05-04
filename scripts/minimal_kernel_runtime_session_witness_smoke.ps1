param(
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-witness-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

Ensure-Directory -Path $resolvedOutputRoot

$sessionSmoke = Join-Path $PSScriptRoot "minimal_kernel_runtime_session_smoke.ps1"
$worldCompareSmoke = Join-Path $PSScriptRoot "system_compiler_world_compare_session_drift_smoke.ps1"
$witnessExportSmoke = Join-Path $PSScriptRoot "system_compiler_witness_session_failure_export_smoke.ps1"

foreach ($script in @($sessionSmoke, $worldCompareSmoke, $witnessExportSmoke)) {
    if (-not (Test-Path $script)) {
        throw "required session witness smoke not found: $script"
    }
}

$sessionOutputRoot = Join-Path $resolvedOutputRoot "session"
$worldCompareOutputRoot = Join-Path $resolvedOutputRoot "world_compare_session_drift"
$witnessExportOutputRoot = Join-Path $resolvedOutputRoot "witness_session_failure_export"

Push-Location $repoRoot
try {
    & $sessionSmoke `
        -OutputRoot $sessionOutputRoot `
        -PythonExe $PythonExe

    & $worldCompareSmoke `
        -OutputRoot $worldCompareOutputRoot `
        -PythonExe $PythonExe

    & $witnessExportSmoke `
        -OutputRoot $witnessExportOutputRoot `
        -PythonExe $PythonExe
} finally {
    Pop-Location
}

$sessionSummary = Join-Path $sessionOutputRoot "kernel_runtime_session.summary.json"
$worldCompareSummary = Join-Path $worldCompareOutputRoot "summary.json"
$witnessCompareSummary = Join-Path $witnessExportOutputRoot "world_compare\summary.json"

foreach ($path in @($sessionSummary, $worldCompareSummary, $witnessCompareSummary)) {
    if (-not (Test-Path $path)) {
        throw "missing session witness smoke artifact: $path"
    }
}

Write-Host "==> minimal kernel runtime session witness smoke"
Write-Host ("session_summary={0}" -f $sessionSummary)
Write-Host ("world_compare_session_drift={0}" -f $worldCompareSummary)
Write-Host ("witness_failure_export_compare={0}" -f $witnessCompareSummary)
Write-Host "ok=1"
