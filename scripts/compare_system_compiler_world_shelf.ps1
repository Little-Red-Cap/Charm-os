param(
    [string]$BaselineSummary = "",
    [string]$BaselineShelfRoot = "",
    [string]$CandidateSummary = "",
    [string]$CandidateShelfRoot = "",
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$ValidationLogPath = "",
    [string]$CompareLogPath = "",
    [string]$GateLogPath = "",
    [string]$PythonExe = "",
    [switch]$Clean,
    [switch]$SkipGate,
    [string]$RequireResult = "ok",
    [string]$RequireVerdict = "",
    [int]$MaxRegressions = -1,
    [int]$RequireAddedEntries = -1,
    [int]$RequireRemovedEntries = -1,
    [int]$RequireChangedEntries = -1,
    [int]$RequireImprovementCount = -1,
    [int]$RequireAddedWorlds = -1,
    [int]$RequireRemovedWorlds = -1,
    [int]$MaxAddedFailedEntries = -1
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

function Ensure-ParentDirectory {
    param(
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        Ensure-Directory -Path $parent
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

function Get-OutputPath {
    param(
        [string]$ExplicitPath,
        [string]$OutputRootPath,
        [string]$DefaultFileName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return Resolve-FullPath -Path $ExplicitPath
    }

    return Join-Path $OutputRootPath $DefaultFileName
}

function Resolve-ShelfSummaryPath {
    param(
        [string]$ExplicitSummary,
        [string]$ShelfRoot,
        [string]$Label
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitSummary)) {
        return Resolve-FullPath -Path $ExplicitSummary
    }

    if (-not [string]::IsNullOrWhiteSpace($ShelfRoot)) {
        $resolvedShelfRoot = Resolve-FullPath -Path $ShelfRoot
        return Join-Path $resolvedShelfRoot "biography.index.summary.json"
    }

    throw "$Label summary or shelf root is required"
}

function Add-ToolArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add($Value) | Out-Null
}

function Write-Utf8Text {
    param(
        [string]$Path,
        [string]$Text
    )

    Ensure-ParentDirectory -Path $Path
    Set-Content -LiteralPath $Path -Encoding utf8 -Value $Text
}

function Invoke-ExternalTool {
    param(
        [string]$Executable,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage,
        [switch]$AppendLog
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($Executable))

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        if ($AppendLog) {
            & $Executable @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath -Append | Out-Host
        } else {
            & $Executable @ArgumentList 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        }
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($exitCode -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $exitCode)
    }
}

function Invoke-PowerShellFile {
    param(
        [string]$PowerShellExe,
        [string]$ScriptPath,
        [string[]]$ArgumentList,
        [string]$LogPath,
        [string]$FailureMessage,
        [switch]$AppendLog
    )

    Ensure-ParentDirectory -Path $LogPath
    Write-Host ("==> {0}" -f [System.IO.Path]::GetFileName($ScriptPath))

    $commandArgs = [System.Collections.Generic.List[string]]::new()
    $commandArgs.Add("-NoProfile") | Out-Null
    $commandArgs.Add("-ExecutionPolicy") | Out-Null
    $commandArgs.Add("Bypass") | Out-Null
    $commandArgs.Add("-File") | Out-Null
    $commandArgs.Add($ScriptPath) | Out-Null
    foreach ($entry in @($ArgumentList)) {
        $commandArgs.Add([string]$entry) | Out-Null
    }

    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        if ($AppendLog) {
            & $PowerShellExe @commandArgs 2>&1 | Tee-Object -FilePath $LogPath -Append | Out-Host
        } else {
            & $PowerShellExe @commandArgs 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
        }
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
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/system-compiler-world-shelf-compare" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "check.txt"
$baselineSummarySidecarPathResolved = Join-Path $outputRootPath "baseline_biography_index.txt"
$candidateSummarySidecarPathResolved = Join-Path $outputRootPath "candidate_biography_index.txt"
$validationLogPathResolved = Get-OutputPath -ExplicitPath $ValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "validate.log"
$compareLogPathResolved = Get-OutputPath -ExplicitPath $CompareLogPath -OutputRootPath $outputRootPath -DefaultFileName "compare.log"
$gateLogPathResolved = Get-OutputPath -ExplicitPath $GateLogPath -OutputRootPath $outputRootPath -DefaultFileName "gate.log"

$compareScript = Join-Path $PSScriptRoot "compare_system_compiler_biography_index.py"
$validateShelfScript = Join-Path $PSScriptRoot "validate_system_compiler_biography_index.py"
$validateCompareScript = Join-Path $PSScriptRoot "validate_system_compiler_biography_index_compare.py"
$gateCompareScript = Join-Path $PSScriptRoot "check_system_compiler_biography_index_compare_summary.ps1"

foreach ($requiredPath in @($compareScript, $validateShelfScript, $validateCompareScript, $gateCompareScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
    }
}

$baselineSummaryPath = Resolve-ShelfSummaryPath `
    -ExplicitSummary $BaselineSummary `
    -ShelfRoot $BaselineShelfRoot `
    -Label "baseline shelf"
$candidateSummaryPath = Resolve-ShelfSummaryPath `
    -ExplicitSummary $CandidateSummary `
    -ShelfRoot $CandidateShelfRoot `
    -Label "candidate shelf"

foreach ($summaryPath in @($baselineSummaryPath, $candidateSummaryPath)) {
    if (-not (Test-Path $summaryPath)) {
        throw "shelf summary not found: $summaryPath"
    }
}

$resolvedPythonExe = if ([string]::IsNullOrWhiteSpace($PythonExe)) {
    Resolve-ToolPath -Candidates @("python.exe", "python")
} else {
    Resolve-FullPath -Path $PythonExe
}
$powerShellExe = Resolve-ToolPath -Candidates @("powershell.exe", "pwsh.exe", "powershell", "pwsh")

Push-Location $repoRoot
try {
    Write-Utf8Text -Path $baselineSummarySidecarPathResolved -Text $baselineSummaryPath
    Write-Utf8Text -Path $candidateSummarySidecarPathResolved -Text $candidateSummaryPath

    foreach ($inputSummary in @($baselineSummaryPath, $candidateSummaryPath)) {
        $validateShelfArgs = @(
            $validateShelfScript,
            "--summary",
            $inputSummary
        )
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $validateShelfArgs `
            -LogPath $validationLogPathResolved `
            -FailureMessage ("world shelf validation failed: {0}" -f $inputSummary) `
            -AppendLog
    }

    $compareArgs = [System.Collections.Generic.List[string]]::new()
    $compareArgs.Add($compareScript) | Out-Null
    Add-ToolArgument -Arguments $compareArgs -Name "--baseline" -Value $baselineSummaryPath
    Add-ToolArgument -Arguments $compareArgs -Name "--candidate" -Value $candidateSummaryPath
    Add-ToolArgument -Arguments $compareArgs -Name "--output-root" -Value $outputRootPath
    Add-ToolArgument -Arguments $compareArgs -Name "--summary" -Value $summaryPathResolved
    Add-ToolArgument -Arguments $compareArgs -Name "--report-markdown" -Value $reportMarkdownPathResolved
    Add-ToolArgument -Arguments $compareArgs -Name "--check-text" -Value $checkTextPathResolved
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $compareArgs.ToArray() `
        -LogPath $compareLogPathResolved `
        -FailureMessage "world shelf compare failed"

    $validateCompareArgs = @(
        $validateCompareScript,
        "--summary",
        $summaryPathResolved
    )
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $validateCompareArgs `
        -LogPath $validationLogPathResolved `
        -FailureMessage "world shelf compare validation failed" `
        -AppendLog

    if (-not $SkipGate) {
        $gateArgs = [System.Collections.Generic.List[string]]::new()
        Add-ToolArgument -Arguments $gateArgs -Name "-Summary" -Value $summaryPathResolved
        if (-not [string]::IsNullOrWhiteSpace($RequireResult)) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireResult" -Value $RequireResult
        }
        if (-not [string]::IsNullOrWhiteSpace($RequireVerdict)) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireVerdict" -Value $RequireVerdict
        }
        if ($MaxRegressions -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-MaxRegressions" -Value ([string]$MaxRegressions)
        }
        if ($RequireAddedEntries -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireAddedEntries" -Value ([string]$RequireAddedEntries)
        }
        if ($RequireRemovedEntries -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireRemovedEntries" -Value ([string]$RequireRemovedEntries)
        }
        if ($RequireChangedEntries -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireChangedEntries" -Value ([string]$RequireChangedEntries)
        }
        if ($RequireImprovementCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireImprovementCount" -Value ([string]$RequireImprovementCount)
        }
        if ($RequireAddedWorlds -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireAddedWorlds" -Value ([string]$RequireAddedWorlds)
        }
        if ($RequireRemovedWorlds -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireRemovedWorlds" -Value ([string]$RequireRemovedWorlds)
        }
        if ($MaxAddedFailedEntries -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-MaxAddedFailedEntries" -Value ([string]$MaxAddedFailedEntries)
        }

        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $gateCompareScript `
            -ArgumentList $gateArgs.ToArray() `
            -LogPath $gateLogPathResolved `
            -FailureMessage "world shelf compare gate failed"
    }

    $summaryData = Load-JsonObject -Path $summaryPathResolved
    Write-Host "==> system compiler world shelf compare"
    Write-Host ("baseline_summary={0}" -f $baselineSummaryPath)
    Write-Host ("candidate_summary={0}" -f $candidateSummaryPath)
    Write-Host ("output_root={0}" -f $outputRootPath)
    Write-Host ("summary={0}" -f $summaryPathResolved)
    Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
    Write-Host ("check_text={0}" -f $checkTextPathResolved)
    Write-Host ("baseline_summary_sidecar={0}" -f $baselineSummarySidecarPathResolved)
    Write-Host ("candidate_summary_sidecar={0}" -f $candidateSummarySidecarPathResolved)
    Write-Host ("compare_log={0}" -f $compareLogPathResolved)
    Write-Host ("validation_log={0}" -f $validationLogPathResolved)
    if (-not $SkipGate) {
        Write-Host ("gate_log={0}" -f $gateLogPathResolved)
    }
    Write-Host ("shelf_verdict={0}" -f [string]$summaryData.shelf_verdict)
    Write-Host ("changed_entry_count={0}" -f [int]$summaryData.entry_summary.changed_entry_count)
    Write-Host ("regression_count={0}" -f [int]$summaryData.entry_summary.regression_count)
    Write-Host ("improvement_count={0}" -f [int]$summaryData.entry_summary.improvement_count)
} finally {
    Pop-Location
}
