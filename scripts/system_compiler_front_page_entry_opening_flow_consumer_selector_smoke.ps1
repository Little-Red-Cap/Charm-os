param(
    [string]$FrontPageWorkspaceRoot = "cmake-build-codex-system-compiler-front-page-smoke",
    [string]$ConsumerWorkspaceRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-workspace-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke",
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

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$frontPageWorkspaceRootPath = Resolve-FullPath -Path $FrontPageWorkspaceRoot
$consumerWorkspaceRootPath = Resolve-FullPath -Path $ConsumerWorkspaceRoot
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

$consumerWorkspaceScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_workspace.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$validateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
foreach ($requiredPath in @($consumerWorkspaceScript, $exportScript, $validateScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $consumerSummaryPath = Join-Path $consumerWorkspaceRootPath "consumer\front-page.entry-opening-flow.consumer.summary.json"
    if (Test-Path $consumerSummaryPath) {
        Write-Host "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-SMOKE] bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable "powershell.exe" `
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
            -FailureMessage "front page entry opening-flow consumer workspace bootstrap failed"
    }

    $summaryPath = Join-Path $outputRootPath "front-page.entry-opening-flow.consumer.selector.summary.json"
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--consumer",
            $consumerSummaryPath,
            "--output-root",
            $outputRootPath
        ) `
        -FailureMessage "front page entry opening-flow consumer selector export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($validateScript, "--summary", $summaryPath) `
        -FailureMessage "front page entry opening-flow consumer selector validation failed"

    $summary = Load-JsonObject -Path $summaryPath
    Write-Host (
        "[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-SMOKE] selected={0} default={1} compare={2} fallback={3}" -f
        [int]$summary.selector_status.selected_entry_count,
        [string]$summary.selector_status.default_entry_name,
        [string]$summary.selector_status.compare_entry_name,
        [int]$summary.selector_status.fallback_entry_count
    )
} finally {
    Pop-Location
}

Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-SMOKE] consumer_workspace_root={0}" -f $consumerWorkspaceRootPath)
Write-Host ("[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-SMOKE] output_root={0}" -f $outputRootPath)
