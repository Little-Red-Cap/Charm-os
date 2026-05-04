param(
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$InspectCompareSummaryOutputRoot = "",
    [string]$PythonExe = "python",
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -Recurse -Force $Path
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$rootScript = Join-Path $PSScriptRoot "minimal_kernel_runtime_session_witness_smoke.ps1"
$validator = Join-Path $PSScriptRoot "validate_minimal_kernel_runtime_session_witness_smoke.py"
$gate = Join-Path $PSScriptRoot "check_minimal_kernel_runtime_session_witness_smoke_summary.ps1"
$inspectCompareSmoke = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1"

foreach ($script in @($rootScript, $validator, $gate)) {
    if (-not (Test-Path $script)) {
        throw "missing runtime session witness CI dependency: $script"
    }
}

$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "out\minimal-kernel-runtime-session-witness-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

if ($Clean) {
    Remove-PathIfExists -Path $resolvedOutputRoot
}

$resolvedInspectCompareSummaryOutputRoot = ""
if (-not [string]::IsNullOrWhiteSpace($InspectCompareSummaryOutputRoot)) {
    if (-not (Test-Path $inspectCompareSmoke)) {
        throw "missing runtime session witness inspect compare dependency: $inspectCompareSmoke"
    }

    $resolvedInspectCompareSummaryOutputRoot = Resolve-FullPath -Path $InspectCompareSummaryOutputRoot
    if ($Clean) {
        Remove-PathIfExists -Path $resolvedInspectCompareSummaryOutputRoot
    }
}

$invokeArgs = @{
    OutputRoot = $resolvedOutputRoot
    PythonExe  = $PythonExe
}

if (-not [string]::IsNullOrWhiteSpace($SummaryPath)) {
    $invokeArgs.SummaryPath = Resolve-FullPath -Path $SummaryPath
}
if (-not [string]::IsNullOrWhiteSpace($ReportMarkdownPath)) {
    $invokeArgs.ReportMarkdownPath = Resolve-FullPath -Path $ReportMarkdownPath
}
if (-not [string]::IsNullOrWhiteSpace($CheckTextPath)) {
    $invokeArgs.CheckTextPath = Resolve-FullPath -Path $CheckTextPath
}

Push-Location $repoRoot
try {
    & $rootScript @invokeArgs
} finally {
    Pop-Location
}

$summaryPathResolved = if ($invokeArgs.ContainsKey("SummaryPath")) {
    [string]$invokeArgs.SummaryPath
} else {
    Join-Path $resolvedOutputRoot "summary.json"
}

Push-Location $repoRoot
try {
    & $PythonExe $validator --summary $summaryPathResolved
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $gate `
        -Summary $summaryPathResolved `
        -RequireResult "ok" `
        -RequireSessionStatus "standing" `
        -RequireWorldCompareVerdict "collapsed" `
        -RequireWitnessCompareVerdict "collapsed" `
        -RequireSessionDrift "true" `
        -RequireSessionFailureCode @("handoff_continuity_broken") `
        -RequireMissingRuntimeFact @("handoff") `
        -RequireSessionFocus @("session", "runtime", "handoff", "continuity") `
        -MaxViolations 0
} finally {
    Pop-Location
}

if (-not [string]::IsNullOrWhiteSpace($resolvedInspectCompareSummaryOutputRoot)) {
    Push-Location $repoRoot
    try {
        & $inspectCompareSmoke `
            -OutputRoot $resolvedInspectCompareSummaryOutputRoot `
            -PythonExe $PythonExe
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
}

Write-Host "==> minimal kernel runtime session witness CI smoke"
Write-Host ("output_root={0}" -f $resolvedOutputRoot)
Write-Host ("summary={0}" -f $summaryPathResolved)
if (-not [string]::IsNullOrWhiteSpace($resolvedInspectCompareSummaryOutputRoot)) {
    Write-Host ("inspect_compare_output_root={0}" -f $resolvedInspectCompareSummaryOutputRoot)
}
Write-Host "ok=1"
