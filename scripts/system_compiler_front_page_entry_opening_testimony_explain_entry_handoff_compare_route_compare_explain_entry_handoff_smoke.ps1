param(
    [string]$OpeningTestimonyExplainEntryHandoffCompareRouteCompareExplainEntryRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-compare-explain-entry-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-testimony-explain-entry-handoff-compare-route-compare-explain-entry-handoff-smoke",
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
$openingTestimonyExplainEntryHandoffCompareRouteCompareExplainEntryRootPath = Resolve-FullPath -Path $OpeningTestimonyExplainEntryHandoffCompareRouteCompareExplainEntryRoot
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

$routeCompareExplainEntrySmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_opening_testimony_explain_entry_handoff_compare_route_compare_explain_entry_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_testimony_explain_entry_handoff.py"
foreach ($requiredPath in @($routeCompareExplainEntrySmokeScript, $exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $improvedExplainEntryPath = Join-Path $openingTestimonyExplainEntryHandoffCompareRouteCompareExplainEntryRootPath "improved-handoff-compare-route-compare-explain-entry\front-page.entry-opening-testimony.explain-entry.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $improvedExplainEntryPath)) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-HANDOFF-SMOKE] explain_entry_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $routeCompareExplainEntrySmokeScript,
                "-OutputRoot",
                $openingTestimonyExplainEntryHandoffCompareRouteCompareExplainEntryRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "opening testimony handoff-compare route-compare explain-entry smoke bootstrap failed"
    }

    $caseOutputRoot = Join-Path $outputRootPath "improved-handoff-compare-route-compare-explain-entry-handoff"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--source-summary",
            $improvedExplainEntryPath,
            "--output-root",
            $caseOutputRoot
        ) `
        -FailureMessage "opening testimony handoff-compare route-compare explain-entry handoff export failed"

    $summaryPath = Join-Path $caseOutputRoot "front-page.entry-opening-testimony.explain-entry.handoff.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $summaryPath) `
        -FailureMessage "opening testimony handoff-compare route-compare explain-entry handoff validation failed"

    $summary = Load-JsonObject -Path $summaryPath
    Assert-Condition `
        -Condition ([string]$summary.handoff_decision.status -eq "ready") `
        -Message ("expected handoff status ready but got '{0}'" -f $summary.handoff_decision.status)
    Assert-Condition `
        -Condition ([string]$summary.open_target.surface_id -eq "candidate_opening_testimony_explain_entry_handoff") `
        -Message ("expected open target candidate_opening_testimony_explain_entry_handoff but got '{0}'" -f $summary.open_target.surface_id)
    Assert-Condition `
        -Condition ([string]$summary.open_target.summary_schema -eq "system_compiler.front_page_entry_opening_testimony_explain_entry_handoff/v0") `
        -Message "expected open target to be a handoff summary"
    Assert-Condition `
        -Condition ([string]$summary.handoff_action.expected_consumer_operation -eq "open-selected-summary") `
        -Message "expected operation open-selected-summary"

    $serialized = $summary | ConvertTo-Json -Depth 100 -Compress
    foreach ($forbiddenText in @("runtime_session_summary", "world_compare_summary", "session_witness_inspect_compare_consumer")) {
        Assert-Condition `
            -Condition (-not $serialized.Contains($forbiddenText)) `
            -Message ("handoff should not contain forbidden raw evidence field '{0}'" -f $forbiddenText)
    }

    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-HANDOFF-SMOKE] status={0} target={1} selection={2}" -f
        [string]$summary.handoff_decision.status,
        [string]$summary.open_target.surface_id,
        [string]$summary.source_explain_entry_ref.selection_kind
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-TESTIMONY-EXPLAIN-ENTRY-HANDOFF-COMPARE-ROUTE-COMPARE-EXPLAIN-ENTRY-HANDOFF-SMOKE] output_root={0}" -f $outputRootPath)
