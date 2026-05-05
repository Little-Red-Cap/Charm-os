param(
    [string]$SampleSummary = "schemas/examples/system_compiler.witness_bundle.v0.sample.json",
    [string]$OutputRoot = "cmake-build-system-compiler-front-page-entry-runtime-session-opening-flow-consumer-selector-sample-smoke",
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

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return
    }

    Remove-Item -LiteralPath $Path -Recurse -Force
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
$sampleSummaryPath = Resolve-FullPath -Path $SampleSummary
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path $sampleSummaryPath)) {
    throw "sample summary not found: $sampleSummaryPath"
}

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}

$flowSmokeScript = Join-Path $PSScriptRoot "system_compiler_front_page_entry_runtime_session_opening_flow_sample_smoke.ps1"
$consumerExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer.py"
$consumerValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer.py"
$selectorExportScript = Join-Path $PSScriptRoot "export_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
$selectorValidateScript = Join-Path $PSScriptRoot "validate_system_compiler_front_page_entry_opening_flow_consumer_selector.py"
foreach ($requiredPath in @(
    $flowSmokeScript,
    $consumerExportScript,
    $consumerValidateScript,
    $selectorExportScript,
    $selectorValidateScript
)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing script: $requiredPath"
    }
}

Push-Location $repoRoot
try {
    $flowRoot = Join-Path $outputRootPath "opening-flow"
    $consumerRoot = Join-Path $outputRootPath "consumer"
    $selectorRoot = Join-Path $outputRootPath "selector"
    $flowSummaryPath = Join-Path $flowRoot "front-page.entry-opening-flow.summary.json"
    $consumerSummaryPath = Join-Path $consumerRoot "front-page.entry-opening-flow.consumer.summary.json"
    $selectorSummaryPath = Join-Path $selectorRoot "front-page.entry-opening-flow.consumer.selector.summary.json"

    $flowArgs = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $flowSmokeScript,
        "-SampleSummary",
        $sampleSummaryPath,
        "-OutputRoot",
        $flowRoot,
        "-PythonExe",
        $resolvedPythonExe
    )
    if ($Clean) {
        $flowArgs += "-Clean"
    }

    Invoke-ExternalTool `
        -Executable "powershell.exe" `
        -ArgumentList $flowArgs `
        -FailureMessage "runtime session opening flow sample smoke failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $consumerExportScript,
            "--flow",
            $flowSummaryPath,
            "--output-root",
            $consumerRoot
        ) `
        -FailureMessage "runtime session opening flow consumer sample export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($consumerValidateScript, "--summary", $consumerSummaryPath) `
        -FailureMessage "runtime session opening flow consumer sample validation failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @(
            $selectorExportScript,
            "--consumer",
            $consumerSummaryPath,
            "--output-root",
            $selectorRoot
        ) `
        -FailureMessage "runtime session opening flow consumer selector sample export failed"

    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList @($selectorValidateScript, "--summary", $selectorSummaryPath) `
        -FailureMessage "runtime session opening flow consumer selector sample validation failed"

    $consumerSummary = Load-JsonObject -Path $consumerSummaryPath
    Assert-Condition `
        -Condition ([string]$consumerSummary.consumer_status.default_opening_name -eq "runtime-session-sample") `
        -Message ("expected consumer default runtime-session-sample but got '{0}'" -f $consumerSummary.consumer_status.default_opening_name)
    Assert-Condition `
        -Condition ([int]$consumerSummary.consumer_status.renderable_opening_count -eq 1) `
        -Message ("expected one renderable opening but got {0}" -f $consumerSummary.consumer_status.renderable_opening_count)
    Assert-Condition `
        -Condition ([string]$consumerSummary.default_opening.selected_tab_id -eq "runtime_session") `
        -Message ("expected consumer selected_tab_id runtime_session but got '{0}'" -f $consumerSummary.default_opening.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$consumerSummary.default_opening.target_summary_schema -eq "minimal_kernel.kernel_runtime_session/v0") `
        -Message ("expected consumer target schema minimal_kernel.kernel_runtime_session/v0 but got '{0}'" -f $consumerSummary.default_opening.target_summary_schema)
    Assert-Condition `
        -Condition ([string]$consumerSummary.default_opening.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected consumer projection kernel_runtime_session_overview but got '{0}'" -f $consumerSummary.default_opening.projection_kind)

    $selectorSummary = Load-JsonObject -Path $selectorSummaryPath
    Assert-Condition `
        -Condition ([string]$selectorSummary.selector_status.default_entry_name -eq "runtime-session-sample") `
        -Message ("expected selector default runtime-session-sample but got '{0}'" -f $selectorSummary.selector_status.default_entry_name)
    Assert-Condition `
        -Condition ([int]$selectorSummary.selector_status.selected_entry_count -eq 1) `
        -Message ("expected selector selected_entry_count=1 but got {0}" -f $selectorSummary.selector_status.selected_entry_count)
    Assert-Condition `
        -Condition ([string]$selectorSummary.open_plan.default_entry.selected_tab_id -eq "runtime_session") `
        -Message ("expected selector selected_tab_id runtime_session but got '{0}'" -f $selectorSummary.open_plan.default_entry.selected_tab_id)
    Assert-Condition `
        -Condition ([string]$selectorSummary.open_plan.default_entry.projection_kind -eq "kernel_runtime_session_overview") `
        -Message ("expected selector projection kernel_runtime_session_overview but got '{0}'" -f $selectorSummary.open_plan.default_entry.projection_kind)

    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-CONSUMER-SELECTOR-SAMPLE-SMOKE] flow={0}" -f $flowSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-CONSUMER-SELECTOR-SAMPLE-SMOKE] consumer={0}" -f $consumerSummaryPath)
    Write-Host ("[FRONT-PAGE-ENTRY-RUNTIME-SESSION-CONSUMER-SELECTOR-SAMPLE-SMOKE] selector={0}" -f $selectorSummaryPath)
    Write-Host (
        "[FRONT-PAGE-ENTRY-RUNTIME-SESSION-CONSUMER-SELECTOR-SAMPLE-SMOKE] default={0} projection={1}" -f
        [string]$selectorSummary.selector_status.default_entry_name,
        [string]$selectorSummary.open_plan.default_entry.projection_kind
    )
    Write-Host "ok=1"
} finally {
    Pop-Location
}
