param(
    [string]$InputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-smoke",
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
    Resolve-FullPath -Path $PythonExe
}

$flowSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_flow_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer.py"
foreach ($requiredPath in @($flowSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $flowSummaryPath = Join-Path $inputRootPath "front-page.entry-opening-flow.summary.json"
    if (Test-Path $flowSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $flowSmokeScript,
                "-OutputRoot",
                $inputRootPath,
                "-PythonExe",
                $resolvedPythonExe
            ) `
            -FailureMessage "front page entry opening flow smoke bootstrap failed"
    }

    $summaryPath = Join-Path $outputRootPath "front-page.entry-opening-flow.consumer.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--flow",
            $flowSummaryPath,
            "--output-root",
            $outputRootPath
        ) `
        -FailureMessage "front page entry opening flow consumer export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $summaryPath) `
        -FailureMessage "front page entry opening flow consumer validation failed"

    $summary = Load-JsonObject -Path $summaryPath
    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace([string]$summary.default_opening.opening_reason.kind)) `
        -Message "default opening must expose opening_reason.kind"
    Assert-Condition `
        -Condition (-not [string]::IsNullOrWhiteSpace([string]$summary.default_opening.projection_headline)) `
        -Message "default opening must expose projection_headline"
    Assert-Condition `
        -Condition (@($summary.default_opening.projection_summary_lines).Count -gt 0) `
        -Message "default opening must expose projection_summary_lines"
    Assert-Condition `
        -Condition (@($summary.readiness_surface.preview_ready_openings).Count -eq [int]$summary.consumer_status.renderable_opening_count) `
        -Message "preview_ready_openings should cover every renderable opening in smoke"
    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SMOKE] openings={0} renderable={1} compare_aware={2} default={3} reason={4}" -f
        [int]$summary.consumer_status.total_opening_count,
        [int]$summary.consumer_status.renderable_opening_count,
        [int]$summary.consumer_status.compare_aware_opening_count,
        [string]$summary.consumer_status.default_opening_name,
        [string]$summary.default_opening.opening_reason.kind
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SMOKE] input_root={0}" -f $inputRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SMOKE] output_root={0}" -f $outputRootPath)
