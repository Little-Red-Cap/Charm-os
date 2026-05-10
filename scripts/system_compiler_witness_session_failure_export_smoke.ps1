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
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    Join-Path $repoRoot "cmake-build-system-compiler-witness-session-failure-export-smoke"
} else {
    Resolve-FullPath -Path $OutputRoot
}

$artifactRoot = Join-Path $resolvedOutputRoot "artifacts"
$baselineRoot = Join-Path $resolvedOutputRoot "baseline"
$candidateRoot = Join-Path $resolvedOutputRoot "candidate"
$compareRoot = Join-Path $resolvedOutputRoot "world_compare"
Ensure-Directory -Path $artifactRoot
Ensure-Directory -Path $baselineRoot
Ensure-Directory -Path $candidateRoot
Ensure-Directory -Path $compareRoot

$sampleSessionPath = Join-Path $repoRoot "schemas\examples\minimal_kernel.kernel_runtime_session.v0.sample.json"
$baselineSessionPath = Join-Path $artifactRoot "kernel_runtime_session.standing.json"
$candidateSessionPath = Join-Path $artifactRoot "kernel_runtime_session.collapsed.json"
$baselineWorldPath = Join-Path $artifactRoot "minimal_kernel_runtime.session_standing.world.json"
$candidateWorldPath = Join-Path $artifactRoot "minimal_kernel_runtime.session_failure.world.json"

function New-SessionOnlyWorld {
    param(
        [string]$SessionPath
    )

    return [ordered]@{
        schema = "system_compiler.canonical_world/v0"
        kind = "system_compiler.canonical_world"
        name = "minimal_kernel_runtime"
        title = "Minimal Kernel Runtime Session Export Smoke"
        summary = "Session-only canonical world used to prove kernel_runtime_session failure observations survive witness export."
        subject = [ordered]@{
            profile = "debug"
            board = "armv7a_qemu"
            active_facets = @("session", "runtime", "handoff")
        }
        first_class_terms = @("subject", "session", "witness", "transition")
        core_questions = @(
            "Can a kernel runtime session witness be exported as a first-class witness entry?"
        )
        compare_questions = @(
            "If the session drifts, which runtime fact and failure code explain the collapse?"
        )
        contract_refs = @()
        witness_plan = @(
            [ordered]@{
                id = "kernel_runtime_session"
                kind = "kernel_runtime_session"
                label = "minimal-kernel-runtime-session"
                role = "session-only witness export smoke"
                layer = "session"
                required = $true
                witness_focus = @("session", "runtime", "ingress", "continuity")
                case = "minimal_kernel_runtime.armv7a_qemu.debug"
                path = $SessionPath
            }
        )
    }
}

$baselineWorld = New-SessionOnlyWorld -SessionPath $baselineSessionPath
$candidateWorld = New-SessionOnlyWorld -SessionPath $candidateSessionPath
$baselineWorld | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $baselineWorldPath -Encoding utf8
$candidateWorld | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $candidateWorldPath -Encoding utf8

$standingSession = Get-Content -LiteralPath $sampleSessionPath -Raw -Encoding utf8 | ConvertFrom-Json
$standingSession.generated_at = "2026-05-04T00:00:00Z"
$standingSession.artifact_paths.summary = $baselineSessionPath
$standingSession | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $baselineSessionPath -Encoding utf8

$collapsedSession = Get-Content -LiteralPath $sampleSessionPath -Raw -Encoding utf8 | ConvertFrom-Json
$collapsedSession.generated_at = "2026-05-04T00:00:00Z"
$collapsedSession.runtime.handoff_continuity = $false
$collapsedSession.verdict.session_status = "collapsed"
$collapsedSession.verdict.failure_domain = "runtime"
$collapsedSession.failures = @(
    [ordered]@{
        code = "handoff_continuity_broken"
        domain = "runtime"
        layer = "lower_half"
        focus = @("handoff", "continuity", "session")
        required = $true
        phase = "handoff.live"
        message = "handoff launch occurred but landing-side runtime package was not re-consumed"
    }
)
$collapsedSession.artifact_paths.summary = $candidateSessionPath
$collapsedSession | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $candidateSessionPath -Encoding utf8

$python = Resolve-ToolPath -Tool $PythonExe
$witnessExporter = Join-Path $PSScriptRoot "export_system_compiler_witness_bundle.ps1"
$witnessValidator = Join-Path $PSScriptRoot "validate_system_compiler_witness_bundle.py"
$worldCompare = Join-Path $PSScriptRoot "compare_system_compiler_world.py"
$worldCompareValidator = Join-Path $PSScriptRoot "validate_system_compiler_world_compare.py"
$worldCompareGate = Join-Path $PSScriptRoot "check_system_compiler_world_compare_summary.ps1"

$baselineSummaryPath = Join-Path $baselineRoot "summary.json"
$candidateSummaryPath = Join-Path $candidateRoot "summary.json"
$compareSummaryPath = Join-Path $compareRoot "summary.json"

Push-Location $repoRoot
try {
    & $witnessExporter `
        -CanonicalWorld $baselineWorldPath `
        -OutputRoot $baselineRoot `
        -OutputPath $baselineSummaryPath `
        -ReportMarkdownPath (Join-Path $baselineRoot "report.md") `
        -CheckTextPath (Join-Path $baselineRoot "check.txt")

    & $python $witnessValidator --summary $baselineSummaryPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $candidateExportFailedAsExpected = $false
    try {
        & $witnessExporter `
            -CanonicalWorld $candidateWorldPath `
            -OutputRoot $candidateRoot `
            -OutputPath $candidateSummaryPath `
            -ReportMarkdownPath (Join-Path $candidateRoot "report.md") `
            -CheckTextPath (Join-Path $candidateRoot "check.txt")
        throw "candidate witness export was expected to fail because the session witness is collapsed"
    } catch {
        if (-not (Test-Path $candidateSummaryPath)) {
            throw
        }
        $candidateExportFailedAsExpected = $true
        Write-Host ("[WITNESS] candidate export failed as expected: {0}" -f $_.Exception.Message)
    }
    if (-not $candidateExportFailedAsExpected) {
        throw "candidate witness export did not report the expected session failure"
    }

    & $python $witnessValidator --summary $candidateSummaryPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $python $worldCompare `
        --baseline $baselineSummaryPath `
        --candidate $candidateSummaryPath `
        --output-root $compareRoot `
        --summary $compareSummaryPath `
        --report-markdown (Join-Path $compareRoot "report.md") `
        --check-text (Join-Path $compareRoot "check.txt")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $python $worldCompareValidator --summary $compareSummaryPath
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    & $worldCompareGate `
        -Summary $compareSummaryPath `
        -RequireVerdict "collapsed" `
        -RequireSessionDrift "true" `
        -RequireSessionDomain @("session", "runtime") `
        -RequireSessionFocus @("session", "runtime", "handoff", "continuity") `
        -RequireSessionFailureCode @("handoff_continuity_broken") `
        -RequireMissingRuntimeFact @("handoff")
} finally {
    Pop-Location
}

$candidateSummary = Get-Content -LiteralPath $candidateSummaryPath -Raw -Encoding utf8 | ConvertFrom-Json
$sessionEntry = $candidateSummary.witness_entries | Where-Object { [string]$_.kind -eq "kernel_runtime_session" } | Select-Object -First 1
if ($null -eq $sessionEntry) {
    throw "candidate witness bundle did not include a kernel_runtime_session entry"
}
if ([string]$sessionEntry.status -ne "fail") {
    throw "expected candidate session witness status fail, got $($sessionEntry.status)"
}
if (@($sessionEntry.observations) -notcontains "failure=handoff_continuity_broken domain=runtime layer=lower_half phase=handoff.live focus=handoff,continuity,session") {
    throw "candidate session witness did not export the failure observation"
}

$compareSummary = Get-Content -LiteralPath $compareSummaryPath -Raw -Encoding utf8 | ConvertFrom-Json
Write-Host "==> system compiler witness session failure export smoke"
Write-Host ("candidate_session={0}" -f $candidateSessionPath)
Write-Host ("candidate_witness={0}" -f $candidateSummaryPath)
Write-Host ("world_compare={0}" -f $compareSummaryPath)
Write-Host ("world_verdict={0}" -f [string]$compareSummary.world_verdict)
Write-Host ("session_drift_failure_codes={0}" -f (@($compareSummary.collapse_surface.session_drift.failure_codes) -join ","))
Write-Host "ok=1"
