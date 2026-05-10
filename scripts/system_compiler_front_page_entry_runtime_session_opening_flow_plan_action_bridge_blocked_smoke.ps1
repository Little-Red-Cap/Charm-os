param(
    [string]$ConsumerRoot = "cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-plan-action-bridge-blocked-smoke",
    [string]$PythonExe = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    if (-not (Test-Path -LiteralPath $Path)) { New-Item -ItemType Directory -Path $Path -Force | Out-Null }
}

function Remove-PathIfExists {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return }
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
}

function Resolve-ToolPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($null -ne $command) { return $command.Source }
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
    if ($exitCode -ne 0) { throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode) }
}

function Load-JsonObject {
    param([string]$Path)
    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Write-JsonFile {
    param([string]$Path, $Value)
    $json = $Value | ConvertTo-Json -Depth 100
    [System.IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
}

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$consumerRootPath = Resolve-FullPath -Path $ConsumerRoot
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean) { Remove-PathIfExists -Path $outputRootPath }
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

$consumerSmokeScript = Join-Path $PSScriptRoot "system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1"
$exportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py"
foreach ($requiredPath in @($consumerSmokeScript, $exportScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "missing path: $requiredPath" }
}

Push-Location $repoRoot
try {
    $consumerSummaryPath = Join-Path $consumerRootPath "session-witness.inspect.compare.consumer.summary.json"
    if ((-not $Clean) -and (Test-Path -LiteralPath $consumerSummaryPath)) {
        Write-Host "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-BLOCKED-SMOKE] consumer_bootstrap=reuse-existing"
    } else {
        Invoke-ExternalTool `
            -Executable $powerShellExe `
            -ArgumentList @(
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                $consumerSmokeScript,
                "-OutputRoot",
                $consumerRootPath,
                "-PythonExe",
                $resolvedPythonExe,
                "-Clean"
            ) `
            -FailureMessage "runtime session inspect compare consumer smoke bootstrap failed"
    }

    $consumer = Load-JsonObject -Path $consumerSummaryPath
    $consumer.default_explain_hop.artifact_ref.path = ""
    $mutatedConsumerPath = Join-Path $outputRootPath "mutated.consumer.summary.json"
    Write-JsonFile -Path $mutatedConsumerPath -Value $consumer

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $exportScript,
            "--consumer",
            $mutatedConsumerPath,
            "--output-root",
            $outputRootPath
        ) `
        -FailureMessage "runtime session opening bridge blocked export failed"

    $summaryPath = Join-Path $outputRootPath "front-page.entry-runtime-session-opening-flow.plan-action.summary.json"
    $summary = Load-JsonObject -Path $summaryPath
    Assert-Condition `
        -Condition ([string]$summary.open_action.status -eq "blocked") `
        -Message ("expected blocked status but got '{0}'" -f $summary.open_action.status)
    Assert-Condition `
        -Condition (@($summary.violations).Count -ge 1) `
        -Message "expected violations to explain blocked bridge"
    Assert-Condition `
        -Condition (@($summary.violations) -contains "default explain hop artifact ref path is missing") `
        -Message "expected missing artifact ref path violation"

    Write-Host (
        "[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-BLOCKED-SMOKE] status={0} violations={1}" -f
        [string]$summary.open_action.status,
        (@($summary.violations) -join "; ")
    )
} finally {
    Pop-Location
}

Write-Host ("[RUNTIME-SESSION-OPENING-FLOW-PLAN-ACTION-BRIDGE-BLOCKED-SMOKE] output_root={0}" -f $outputRootPath)
