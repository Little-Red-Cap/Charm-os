param(
    [Parameter(Mandatory = $true)]
    [string]$FrontPageWorkspaceRoot,
    [string]$OutputRoot = "out/system-compiler-front-page-entry-opening-flow-consumer-workspace",
    [string]$FlowRoot = "",
    [string]$ConsumerRoot = "",
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
$flowRootPath = if ([string]::IsNullOrWhiteSpace($FlowRoot)) {
    Join-Path $outputRootPath "opening_flow"
} else {
    Resolve-FullPath -Path $FlowRoot
}
$consumerRootPath = if ([string]::IsNullOrWhiteSpace($ConsumerRoot)) {
    Join-Path $outputRootPath "consumer"
} else {
    Resolve-FullPath -Path $ConsumerRoot
}

if (-not (Test-Path $frontPageWorkspaceRootPath)) {
    $fixtureBootstrapScript = Join-Path $PSScriptRoot "system_compiler_front_page_smoke_fixture_bootstrap.ps1"
    if (-not (Test-Path $fixtureBootstrapScript)) {
        throw "front-page workspace root not found and fixture bootstrap is missing: $frontPageWorkspaceRootPath"
    }

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $fixtureBootstrapScript,
            "-OutputRoot",
            $frontPageWorkspaceRootPath
        ) `
        -FailureMessage "front page smoke fixture bootstrap failed"
}

if ($Clean) {
    $cleanPaths = @($flowRootPath, $consumerRootPath, $outputRootPath)
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
$workspaceExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_workspace.ps1"
$consumerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer.py"
$consumerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer.py"
foreach ($requiredPath in @($workspaceExportScript, $consumerExportScript, $consumerValidateScript)) {
    if (-not (Test-Path $requiredPath)) {
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
            $workspaceExportScript,
            "-FrontPageWorkspaceRoot",
            $frontPageWorkspaceRootPath,
            "-OutputRoot",
            $flowRootPath,
            "-PythonExe",
            $resolvedPythonExe,
            "-Clean"
        ) `
        -FailureMessage "front page entry opening-flow workspace export failed"

    $flowSummaryPath = Join-Path $flowRootPath "front-page.entry-opening-flow.summary.json"
    if (-not (Test-Path $flowSummaryPath)) {
        throw "opening-flow summary not found: $flowSummaryPath"
    }

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $consumerExportScript,
            "--flow",
            $flowSummaryPath,
            "--output-root",
            $consumerRootPath
        ) `
        -FailureMessage "front page entry opening-flow consumer export failed"

    $consumerSummaryPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($consumerValidateScript, "--summary", $consumerSummaryPath) `
        -FailureMessage "front page entry opening-flow consumer validation failed"
} finally {
    Pop-Location
}

$consumerSummaryPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.summary.json"
$consumerReportPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.report.md"
$consumerCheckPath = Join-Path $consumerRootPath "front-page.entry-opening-flow.consumer.check.txt"

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] front_page_workspace_root={0}" -f $frontPageWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] flow_root={0}" -f $flowRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] consumer_root={0}" -f $consumerRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] summary={0}" -f $consumerSummaryPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] report={0}" -f $consumerReportPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-WORKSPACE] check={0}" -f $consumerCheckPath)
