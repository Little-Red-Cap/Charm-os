param(
    [string]$BaselineOpenerWorkspaceRoot = "",
    [string]$CandidateOpenerWorkspaceRoot = "",
    [string]$BaselineOpenerSummaryPath = "",
    [string]$CandidateOpenerSummaryPath = "",
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opener-workspace-compare",
    [string]$CompareRoot = "",
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

function Assert-CleanPath {
    param(
        [string]$Path,
        [string]$RootPath
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    $resolvedRoot = Resolve-FullPath -Path $RootPath
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $rootPrefix = $resolvedRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    if ($resolvedPath.Equals($resolvedRoot, $comparison)) {
        throw "refusing to clean repo root: $resolvedPath"
    }
    if (-not $resolvedPath.StartsWith($rootPrefix, $comparison)) {
        throw "refusing to clean outside repo root: $resolvedPath"
    }
}

function Remove-PathIfExists {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
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

function Resolve-OpenerSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $candidate = Join-Path $WorkspaceRoot "opener\front-page.entry-opener.summary.json"
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-opener.summary.json")
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$baselineOpenerWorkspaceRootPath = Resolve-FullPath -Path $BaselineOpenerWorkspaceRoot
$candidateOpenerWorkspaceRootPath = Resolve-FullPath -Path $CandidateOpenerWorkspaceRoot
$baselineOpenerSummaryPath = Resolve-FullPath -Path $BaselineOpenerSummaryPath
$candidateOpenerSummaryPath = Resolve-FullPath -Path $CandidateOpenerSummaryPath
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$compareRootPath = if ([string]::IsNullOrWhiteSpace($CompareRoot)) {
    Join-Path $outputRootPath "compare"
} else {
    Resolve-FullPath -Path $CompareRoot
}

if ($Clean) {
    foreach ($path in @($compareRootPath, $outputRootPath)) {
        Assert-CleanPath -Path $path -RootPath $repoRoot
    }
    foreach ($path in @($compareRootPath, $outputRootPath)) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener_compare.py"
foreach ($requiredPath in @($compareScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($baselineOpenerSummaryPath)) {
        if ([string]::IsNullOrWhiteSpace($baselineOpenerWorkspaceRootPath)) {
            throw "BaselineOpenerSummaryPath or BaselineOpenerWorkspaceRoot is required"
        }
        $baselineOpenerSummaryPath = Resolve-FullPath -Path (Resolve-OpenerSummaryPath -WorkspaceRoot $baselineOpenerWorkspaceRootPath)
    }
    if ([string]::IsNullOrWhiteSpace($candidateOpenerSummaryPath)) {
        if ([string]::IsNullOrWhiteSpace($candidateOpenerWorkspaceRootPath)) {
            throw "CandidateOpenerSummaryPath or CandidateOpenerWorkspaceRoot is required"
        }
        $candidateOpenerSummaryPath = Resolve-FullPath -Path (Resolve-OpenerSummaryPath -WorkspaceRoot $candidateOpenerWorkspaceRootPath)
    }

    foreach ($summaryPath in @($baselineOpenerSummaryPath, $candidateOpenerSummaryPath)) {
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            throw "front page entry opener summary not found: $summaryPath"
        }
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $compareScript,
            "--baseline",
            $baselineOpenerSummaryPath,
            "--candidate",
            $candidateOpenerSummaryPath,
            "--output-root",
            $compareRootPath
        ) `
        -FailureMessage "front page entry opener workspace compare export failed"

    $compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opener.compare.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $compareSummaryPath) `
        -FailureMessage "front page entry opener workspace compare validation failed"
} finally {
    Pop-Location
}

$compareSummaryPath = Join-Path $compareRootPath "front-page.entry-opener.compare.summary.json"
$compareReportPath = Join-Path $compareRootPath "front-page.entry-opener.compare.report.md"
$compareCheckPath = Join-Path $compareRootPath "front-page.entry-opener.compare.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] baseline_opener_workspace_root={0}" -f $baselineOpenerWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] candidate_opener_workspace_root={0}" -f $candidateOpenerWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] baseline_opener_summary={0}" -f $baselineOpenerSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] candidate_opener_summary={0}" -f $candidateOpenerSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] compare_root={0}" -f $compareRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] summary={0}" -f $compareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] report={0}" -f $compareReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE-COMPARE] check={0}" -f $compareCheckPath)
