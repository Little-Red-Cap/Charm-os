param(
    [string]$LandingWorkspaceRoot = "",
    [string]$LandingSummaryPath = "",
    [string]$LandingCompareWorkspaceRoot = "",
    [string]$LandingCompareSummaryPath = "",
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opener-workspace",
    [string]$OpenerRoot = "",
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

function Resolve-LandingSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $candidate = Join-Path $WorkspaceRoot "landing\front-page.entry-landing.summary.json"
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-landing.summary.json")
}

function Resolve-LandingCompareSummaryPath {
    param(
        [string]$WorkspaceRoot
    )

    $candidate = Join-Path $WorkspaceRoot "landing-compare\front-page.entry-landing.compare.summary.json"
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }

    return (Join-Path $WorkspaceRoot "front-page.entry-landing.compare.summary.json")
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$landingWorkspaceRootPath = Resolve-FullPath -Path $LandingWorkspaceRoot
$landingSummaryPath = Resolve-FullPath -Path $LandingSummaryPath
$landingCompareWorkspaceRootPath = Resolve-FullPath -Path $LandingCompareWorkspaceRoot
$landingCompareSummaryPath = Resolve-FullPath -Path $LandingCompareSummaryPath
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$openerRootPath = if ([string]::IsNullOrWhiteSpace($OpenerRoot)) {
    Join-Path $outputRootPath "opener"
} else {
    Resolve-FullPath -Path $OpenerRoot
}

if ($Clean) {
    foreach ($path in @($openerRootPath, $outputRootPath)) {
        Assert-CleanPath -Path $path -RootPath $repoRoot
    }
    foreach ($path in @($openerRootPath, $outputRootPath)) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opener.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opener.py"
foreach ($requiredPath in @($exportScript, $validateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    if ([string]::IsNullOrWhiteSpace($landingSummaryPath)) {
        if ([string]::IsNullOrWhiteSpace($landingWorkspaceRootPath)) {
            throw "LandingSummaryPath or LandingWorkspaceRoot is required"
        }
        $landingSummaryPath = Resolve-FullPath -Path (Resolve-LandingSummaryPath -WorkspaceRoot $landingWorkspaceRootPath)
    }
    if (-not (Test-Path -LiteralPath $landingSummaryPath)) {
        throw "front page entry landing summary not found: $landingSummaryPath"
    }

    if ([string]::IsNullOrWhiteSpace($landingCompareSummaryPath) -and -not [string]::IsNullOrWhiteSpace($landingCompareWorkspaceRootPath)) {
        $landingCompareSummaryPath = Resolve-FullPath -Path (Resolve-LandingCompareSummaryPath -WorkspaceRoot $landingCompareWorkspaceRootPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($landingCompareSummaryPath) -and -not (Test-Path -LiteralPath $landingCompareSummaryPath)) {
        throw "front page entry landing compare summary not found: $landingCompareSummaryPath"
    }

    $exportArguments = @(
        $exportScript,
        "--landing",
        $landingSummaryPath,
        "--output-root",
        $openerRootPath
    )
    if (-not [string]::IsNullOrWhiteSpace($landingCompareSummaryPath)) {
        $exportArguments += @("--landing-compare", $landingCompareSummaryPath)
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $exportArguments `
        -FailureMessage "front page entry opener workspace export failed"

    $openerSummaryPath = Join-Path $openerRootPath "front-page.entry-opener.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $openerSummaryPath) `
        -FailureMessage "front page entry opener workspace validation failed"
} finally {
    Pop-Location
}

$openerSummaryPath = Join-Path $openerRootPath "front-page.entry-opener.summary.json"
$openerReportPath = Join-Path $openerRootPath "front-page.entry-opener.report.md"
$openerCheckPath = Join-Path $openerRootPath "front-page.entry-opener.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] landing_workspace_root={0}" -f $landingWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] landing_summary={0}" -f $landingSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] landing_compare_workspace_root={0}" -f $landingCompareWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] landing_compare_summary={0}" -f $landingCompareSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] output_root={0}" -f $outputRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] opener_root={0}" -f $openerRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] summary={0}" -f $openerSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] report={0}" -f $openerReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENER-WORKSPACE] check={0}" -f $openerCheckPath)
