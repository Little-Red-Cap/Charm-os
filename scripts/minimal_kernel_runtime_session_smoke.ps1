param(
    [string]$RuntimeEvidenceSummary = "",
    [string]$CanonicalWorld = "",
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

function Resolve-ToolPath {
    param(
        [string]$Tool
    )

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedInput = if ([string]::IsNullOrWhiteSpace($RuntimeEvidenceSummary)) {
    Join-Path $repoRoot "schemas\examples\minimal_kernel.runtime_evidence_bundle.summary.v1.sample.json"
} else {
    Resolve-FullPath -Path $RuntimeEvidenceSummary
}
$resolvedCanonicalWorld = if ([string]::IsNullOrWhiteSpace($CanonicalWorld)) {
    Join-Path $repoRoot "Examples\kernel\canonical_worlds\minimal_kernel_runtime.world.json"
} else {
    Resolve-FullPath -Path $CanonicalWorld
}
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-minimal-kernel-runtime-session-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

Ensure-Directory -Path $resolvedOutputRoot

$python = Resolve-ToolPath -Tool $PythonExe
$exporter = Join-Path $PSScriptRoot "export_minimal_kernel_runtime_session.py"
if (-not (Test-Path $exporter)) {
    throw "missing exporter: $exporter"
}

Push-Location $repoRoot
try {
    & $python $exporter `
        --runtime-evidence-summary $resolvedInput `
        --canonical-world $resolvedCanonicalWorld `
        --output-root $resolvedOutputRoot `
        --validate

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

$summaryPath = Join-Path $resolvedOutputRoot "kernel_runtime_session.summary.json"
$runtimeLedgerPath = Join-Path $resolvedOutputRoot "runtime_ledger.json"
$reportPath = Join-Path $resolvedOutputRoot "report.md"
$checkPath = Join-Path $resolvedOutputRoot "check.txt"

foreach ($path in @($summaryPath, $runtimeLedgerPath, $reportPath, $checkPath)) {
    if (-not (Test-Path $path)) {
        throw "missing session smoke artifact: $path"
    }
}

$summary = Get-Content -LiteralPath $summaryPath -Raw -Encoding utf8 | ConvertFrom-Json
if ([string]$summary.verdict.session_status -ne "standing") {
    throw "kernel runtime session smoke did not stand"
}
if (@($summary.failures).Count -ne 0) {
    throw "kernel runtime session smoke produced failures"
}

Write-Host "==> kernel runtime session smoke"
Write-Host ("input={0}" -f $resolvedInput)
Write-Host ("output_root={0}" -f $resolvedOutputRoot)
Write-Host ("summary={0}" -f $summaryPath)
Write-Host ("runtime_ledger={0}" -f $runtimeLedgerPath)
Write-Host ("report={0}" -f $reportPath)
Write-Host ("check={0}" -f $checkPath)
Write-Host "ok=1"
