param(
    [Parameter(Mandatory = $true)]
    [string]$FrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-consumer-selector-workspace",
    [string]$ConsumerWorkspaceRoot = "",
    [string]$SelectorRoot = "",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot
$consumerWorkspaceRootPath = if ([string]::IsNullOrWhiteSpace($ConsumerWorkspaceRoot)) {
    Join-Path $outputRootPath "consumer_workspace"
} else {
    Resolve-FullPath -Path $ConsumerWorkspaceRoot
}
$selectorRootPath = if ([string]::IsNullOrWhiteSpace($SelectorRoot)) {
    Join-Path $outputRootPath "selector"
} else {
    Resolve-FullPath -Path $SelectorRoot
}

if (-not (Test-Path -LiteralPath $frontPageWorkspaceRootPath)) {
    throw "front-page workspace root not found: $frontPageWorkspaceRootPath"
}

if ($Clean) {
    $cleanPaths = @($consumerWorkspaceRootPath, $selectorRootPath, $outputRootPath)
    foreach ($path in $cleanPaths) {
        Assert-CleanPath -Path $path -RootPath $repoRoot
    }
    foreach ($path in $cleanPaths) {
        Remove-PathIfExists -Path $path
    }
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")
$consumerWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_workspace.ps1"
$selectorExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$selectorValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
foreach ($requiredPath in @($consumerWorkspaceScript, $selectorExportScript, $selectorValidateScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    Invoke-ExternalTool `
        -Executable $powerShellExe `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $consumerWorkspaceScript,
            "-FrontPageWorkspaceRoot",
            $frontPageWorkspaceRootPath,
            "-OutputRoot",
            $consumerWorkspaceRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow consumer workspace export failed"

    $consumerSummaryPath = Join-Path $consumerWorkspaceRootPath "consumer\front-page.entry-opening-flow.consumer.summary.json"
    if (-not (Test-Path -LiteralPath $consumerSummaryPath)) {
        throw "opening-flow consumer summary not found: $consumerSummaryPath"
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $selectorExportScript,
            "--consumer",
            $consumerSummaryPath,
            "--output-root",
            $selectorRootPath
        ) `
        -FailureMessage "front page entry opening-flow consumer selector export failed"

    $selectorSummaryPath = Join-Path $selectorRootPath "front-page.entry-opening-flow.consumer.selector.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($selectorValidateScript, "--summary", $selectorSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer selector validation failed"
} finally {
    Pop-Location
}

$selectorSummaryPath = Join-Path $selectorRootPath "front-page.entry-opening-flow.consumer.selector.summary.json"
$selectorReportPath = Join-Path $selectorRootPath "front-page.entry-opening-flow.consumer.selector.report.md"
$selectorCheckPath = Join-Path $selectorRootPath "front-page.entry-opening-flow.consumer.selector.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] consumer_workspace_root={0}" -f $consumerWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] selector_root={0}" -f $selectorRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] summary={0}" -f $selectorSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] report={0}" -f $selectorReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-WORKSPACE] check={0}" -f $selectorCheckPath)
