param(
    [string]$OpeningTestimonyExplainEntryCompareRouteRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-compare-route-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-compare-route-handoff-smoke",
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

    if (-not (Test-Path -LiteralPath $Path)) {
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

    if (Test-Path -LiteralPath $Path) {
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
$openingTestimonyExplainEntryCompareRouteRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryCompareRouteRoot
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
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$compareRouteSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_compare_route_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
foreach ($requiredPath in @($compareRouteSmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $compareRouteExplainEntryPath = Join-Path $openingTestimonyExplainEntryCompareRouteRootPath "drifted-compare-route-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $compareRouteExplainEntryPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-ROUTE-HANDOFF-SMOKE] compare_route_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $compareRouteSmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryCompareRouteRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony explain-entry compare route smoke bootstrap failed"
    }

    $handoffRoot = Join-Path $outputRootPath "drifted-compare-route-explain-entry-handoff"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--source-summary",
            $compareRouteExplainEntryPath,
            "--output-root",
            $handoffRoot
        ) `
        -FailureMessage "opening testimony explain-entry compare route handoff export failed"

    $handoffPath = Join-Path $handoffRoot "front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $handoffPath) `
        -FailureMessage "opening testimony explain-entry compare route handoff validation failed"

    $summary = Load-JsonObject -Path $handoffPath
    Assert-Condition `
        -Condition ([string]$summary.result -eq "ok") `
        -Message ("expected result ok but got '{0}'" -f $summary.result)
    Assert-Condition `
        -Condition ([string]$summary.handoff_decision.status -eq "ready") `
        -Message ("expected handoff ready but got '{0}'" -f $summary.handoff_decision.status)
    Assert-Condition `
        -Condition ([string]$summary.open_target.surface_id -eq "candidate_opening_testimony_explain_entry") `
        -Message ("expected open target candidate_opening_testimony_explain_entry but got '{0}'" -f $summary.open_target.surface_id)
    Assert-Condition `
        -Condition ([string]$summary.source_explain_entry_ref.selection_kind -eq "route_explain_entry_compare_default") `
        -Message ("expected source selection route_explain_entry_compare_default but got '{0}'" -f $summary.source_explain_entry_ref.selection_kind)

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-ROUTE-HANDOFF-SMOKE] status={0} target={1} selection={2}" -f
        [string]$summary.handoff_decision.status,
        [string]$summary.open_target.surface_id,
        [string]$summary.source_explain_entry_ref.selection_kind
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-COMPARE-ROUTE-HANDOFF-SMOKE] output_root={0}" -f $outputRootPath)
