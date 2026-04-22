param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 30,
    [int]$TailLines = 40,
    [string]$SummaryPath = "",
    [string]$CaseOutputRoot = "",
    [switch]$StopOnFailure
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param([string]$Tool)

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
    }
}

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Copy-ArtifactIfPresent {
    param(
        [string]$SourcePath,
        [string]$ArtifactRoot,
        [string]$CaseName,
        [string]$DestinationFileName
    )

    if ([string]::IsNullOrWhiteSpace($SourcePath) -or -not (Test-Path $SourcePath)) {
        return ""
    }

    $resolvedSourcePath = Resolve-FullPath -Path $SourcePath
    if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
        return $resolvedSourcePath
    }

    $caseRoot = Join-Path $ArtifactRoot $CaseName
    Ensure-Directory -Path $caseRoot

    $destinationPath = Join-Path $caseRoot $DestinationFileName
    Copy-Item -LiteralPath $resolvedSourcePath -Destination $destinationPath -Force
    return $destinationPath
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$repoRoot = Resolve-RepoRoot
$leafRoot = Join-Path $repoRoot "Examples/kernel/armv7a/qemu"
$configurePreset = "debug"
$buildPreset = "debug"
$resolvedSummaryPath = Resolve-FullPath -Path $SummaryPath
$resolvedCaseOutputRoot = Resolve-FullPath -Path $CaseOutputRoot

if (-not [string]::IsNullOrWhiteSpace($resolvedCaseOutputRoot)) {
    Ensure-Directory -Path $resolvedCaseOutputRoot
}

$caseSpecs = @(
    [pscustomobject]@{
        Name = "runtime_trap"
        Label = "runtime-trap"
        Script = "run_qemu_runtime_trap_ci.ps1"
        StdoutLog = "qemu-runtime-trap.log"
        StderrLog = "qemu-runtime-trap.err.log"
    },
    [pscustomobject]@{
        Name = "runtime_live"
        Label = "runtime-live"
        Script = "run_qemu_runtime_live_ci.ps1"
        StdoutLog = "qemu-runtime-live.log"
        StderrLog = "qemu-runtime-live.err.log"
    },
    [pscustomobject]@{
        Name = "task_syscall"
        Label = "task-syscall"
        Script = "run_qemu_task_syscall_ci.ps1"
        StdoutLog = "qemu-task-syscall.log"
        StderrLog = "qemu-task-syscall.err.log"
    }
)

$results = [System.Collections.Generic.List[object]]::new()
$configureMs = 0
$buildMs = 0
$hasFailure = $false
$fatalFailurePhase = ""
$fatalFailureMessage = ""
$currentPhase = "prepare"

Push-Location $leafRoot
try {
    try {
        $currentPhase = "configure"
        $configureStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            & $cmake --preset $configurePreset
            if ($LASTEXITCODE -ne 0) {
                throw "cmake configure failed for preset: $configurePreset"
            }
        } finally {
            $configureStopwatch.Stop()
            $configureMs = $configureStopwatch.ElapsedMilliseconds
        }

        $currentPhase = "build"
        $buildStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            & $cmake --build --preset $buildPreset --parallel $BuildJobs
            if ($LASTEXITCODE -ne 0) {
                throw "cmake build failed for preset: $buildPreset"
            }
        } finally {
            $buildStopwatch.Stop()
            $buildMs = $buildStopwatch.ElapsedMilliseconds
        }

        foreach ($caseSpec in @($caseSpecs)) {
            $currentPhase = ("case:{0}" -f $caseSpec.Name)
            $caseElapsedMs = 0
            $caseStatus = "ok"
            $caseDetail = ""
            $stdoutArtifactPath = ""
            $stderrArtifactPath = ""
            $caseScriptPath = Join-Path $leafRoot $caseSpec.Script
            $caseStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

            try {
                if (-not (Test-Path $caseScriptPath)) {
                    throw "missing lower-half smoke case script: $caseScriptPath"
                }

                & $caseScriptPath `
                    -CMakeExe $cmake `
                    -QemuExe $qemu `
                    -BuildJobs $BuildJobs `
                    -TimeoutSec $TimeoutSec `
                    -TailLines $TailLines `
                    -SkipBuild

                if ($LASTEXITCODE -ne 0) {
                    throw ("lower-half smoke failed: {0} (exit code {1})" -f $caseSpec.Script, $LASTEXITCODE)
                }

                $caseDetail = ("completed: {0}" -f $caseSpec.Label)
            } catch {
                $caseStatus = "fail"
                $caseDetail = $_.Exception.Message
                $hasFailure = $true
            } finally {
                $caseStopwatch.Stop()
                $caseElapsedMs = $caseStopwatch.ElapsedMilliseconds

                $stdoutArtifactPath = Copy-ArtifactIfPresent `
                    -SourcePath (Join-Path $leafRoot $caseSpec.StdoutLog) `
                    -ArtifactRoot $resolvedCaseOutputRoot `
                    -CaseName $caseSpec.Name `
                    -DestinationFileName "stdout.log"

                $stderrArtifactPath = Copy-ArtifactIfPresent `
                    -SourcePath (Join-Path $leafRoot $caseSpec.StderrLog) `
                    -ArtifactRoot $resolvedCaseOutputRoot `
                    -CaseName $caseSpec.Name `
                    -DestinationFileName "stderr.log"

                $results.Add([pscustomobject]@{
                    Case = [string]$caseSpec.Name
                    Label = [string]$caseSpec.Label
                    Script = [string]$caseSpec.Script
                    Status = $caseStatus
                    ElapsedMs = $caseElapsedMs
                    StdoutLogPath = $stdoutArtifactPath
                    StderrLogPath = $stderrArtifactPath
                    Detail = $caseDetail
                }) | Out-Null
            }

            if ($caseStatus -ne "ok" -and $StopOnFailure) {
                break
            }
        }
    } catch {
        $hasFailure = $true
        $fatalFailurePhase = $currentPhase
        $fatalFailureMessage = $_.Exception.Message
    }
} finally {
    Pop-Location
}

if (-not [string]::IsNullOrWhiteSpace($resolvedSummaryPath)) {
    Ensure-ParentDirectory -Path $resolvedSummaryPath

    $summary = [ordered]@{
        schema = "minimal_kernel.runtime_armv7a_qemu_smoke.summary/v1"
        generated_at = (Get-Date).ToString("o")
        smoke = "armv7a_qemu_lower_half"
        case_count = @($caseSpecs).Count
        completed_case_count = @($results).Count
        has_failure = [bool]$hasFailure
        mode = [ordered]@{
            build_jobs = $BuildJobs
            timeout_sec = $TimeoutSec
            tail_lines = $TailLines
        }
        phase_elapsed_ms = [ordered]@{
            configure = $configureMs
            build = $buildMs
        }
        case_artifacts_root = $resolvedCaseOutputRoot
        results = @($results)
    }

    if (-not [string]::IsNullOrWhiteSpace($fatalFailurePhase) -or -not [string]::IsNullOrWhiteSpace($fatalFailureMessage)) {
        $summary.fatal_failure = [ordered]@{
            phase = $fatalFailurePhase
            message = $fatalFailureMessage
        }
    }

    $summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedSummaryPath -Encoding utf8
    Write-Host ("[SUMMARY] {0}" -f $resolvedSummaryPath)
}

if ($hasFailure) {
    if (-not [string]::IsNullOrWhiteSpace($fatalFailureMessage)) {
        throw $fatalFailureMessage
    }

    throw "minimal kernel ARMv7-A QEMU smoke failed"
}

Write-Output "[ok] minimal kernel ARMv7-A QEMU smoke detected"
