param(
    [string]$InputRoot = "cmake-build-minimal-kernel-runtime-session-witness-compare-summary-smoke",
    [string]$OutputRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$PythonExe = "",
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

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    if (Test-Path $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Resolve-ToolPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
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
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$inputRootPath = Resolve-FullPath -Path $InputRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    $pythonCommand = Get-Command $PythonExe -ErrorAction SilentlyContinue
    if ($null -ne $pythonCommand) {
        $pythonCommand.Source
    } elseif (Test-Path $PythonExe) {
        Resolve-FullPath -Path $PythonExe
    } else {
        throw "tool not found: $PythonExe"
    }
}

$compareSummarySmokeScript = Join-Path $PSScriptRoot "inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py"
$validateScript = Join-Path $PSScriptRoot "validate_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py"
foreach ($requiredPath in @($compareSummarySmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $compareSummaryPath = Join-Path $inputRootPath "session-witness.inspect.compare.summary.json"
    if (Test-Path $compareSummaryPath) {
        Write-Host "[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $compareSummarySmokeScript,
                "-OutputRoot",
                $inputRootPath,
                "-PythonExe",
                $resolvedPythonExe
            ) `
            -FailureMessage "runtime session witness inspect compare summary smoke bootstrap failed"
    }

    $summaryPath = Join-Path $outputRootPath "session-witness.inspect.compare.consumer.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--compare",
            $compareSummaryPath,
            "--output-root",
            $outputRootPath
        ) `
        -FailureMessage "runtime session witness inspect compare consumer export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $summaryPath) `
        -FailureMessage "runtime session witness inspect compare consumer validation failed"

    $summary = Load-JsonObject -Path $summaryPath
    Assert-Condition `
        -Condition ([string]$summary.consumer_status.default_focus_id -eq "session-state-drift") `
        -Message "default focus should be session-state-drift in smoke"
    Assert-Condition `
        -Condition ([string]$summary.consumer_status.highest_severity -eq "critical") `
        -Message "highest severity should be critical in smoke"
    Assert-Condition `
        -Condition (@($summary.default_focus.runtime_regressions) -contains "handoff_continuity") `
        -Message "default focus should include runtime regression handoff_continuity"
    Assert-Condition `
        -Condition (@($summary.readiness_surface.changed_focus_ids).Count -ge 4) `
        -Message "changed_focus_ids should expose multiple actionable drifts"
    Assert-Condition `
        -Condition ([string]$summary.default_explain_hop.focus_id -eq "session-state-drift") `
        -Message "default explain hop should follow the default focus"
    Assert-Condition `
        -Condition ([string]$summary.default_explain_hop.artifact_ref.id -eq "session-report") `
        -Message "default explain hop should prefer the session report artifact"
    Assert-Condition `
        -Condition ([int]@($summary.fallback_explain_hops).Count -ge 1) `
        -Message "fallback explain hops should expose alternative explain entry points"
    Write-Host (
        "[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] focuses={0} changed={1} default={2} severity={3} next={4}" -f
        [int]$summary.consumer_status.total_focus_count,
        [int]$summary.consumer_status.changed_focus_count,
        [string]$summary.consumer_status.default_focus_id,
        [string]$summary.consumer_status.highest_severity,
        [string]$summary.default_explain_hop.artifact_ref.id
    )
} finally {
    Pop-Location
}

Write-Host ("[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] input_root={0}" -f $inputRootPath)
Write-Host ("[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] output_root={0}" -f $outputRootPath)
