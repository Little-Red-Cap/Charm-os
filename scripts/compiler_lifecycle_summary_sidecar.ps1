param(
    [string]$OutputRoot = "out/compiler-lifecycle-summary",
    [string[]]$ArtifactReportIndex = @(),
    [string[]]$ArtifactReport = @(),
    [string]$KernelRuntimeSession = "",
    [string]$RuntimeLedger = "",
    [string]$WitnessBundle = "",
    [string]$WorldCompare = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$GateTextPath = "",
    [string]$SummaryReportPath = "",
    [string]$PythonExe = "python",
    [switch]$Clean,
    [switch]$SkipGate,
    [switch]$SkipReport
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

function Resolve-OutputPath {
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

function Add-PathArguments {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string[]]$Values
    )

    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace([string]$value)) {
            continue
        }

        $Arguments.Add($Name) | Out-Null
        $Arguments.Add((Resolve-FullPath -Path ([string]$value))) | Out-Null
    }
}

function Add-PathArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return
    }

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add((Resolve-FullPath -Path $Value)) | Out-Null
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if ($Clean -and (Test-Path $outputRootPath)) {
    Remove-Item -LiteralPath $outputRootPath -Recurse -Force
}
Ensure-Directory -Path $outputRootPath

$exportScript = Join-Path $PSScriptRoot "export_compiler_lifecycle_summary.py"
$checkScript = Join-Path $PSScriptRoot "check_compiler_lifecycle_summary.ps1"
$reportScript = Join-Path $PSScriptRoot "report_compiler_lifecycle_summary.ps1"

foreach ($requiredPath in @($exportScript, $checkScript, $reportScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "required script not found: $requiredPath"
    }
}

$summaryPathResolved = Resolve-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "compiler_lifecycle.summary.json"
$reportMarkdownPathResolved = Resolve-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "compiler_lifecycle.report.md"
$checkTextPathResolved = Resolve-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "compiler_lifecycle.check.txt"
$gateTextPathResolved = Resolve-OutputPath -ExplicitPath $GateTextPath -OutputRootPath $outputRootPath -DefaultFileName "compiler_lifecycle.gate.txt"
$summaryReportPathResolved = Resolve-OutputPath -ExplicitPath $SummaryReportPath -OutputRootPath $outputRootPath -DefaultFileName "compiler_lifecycle.summary_report.md"

Ensure-ParentDirectory -Path $summaryPathResolved

$exportArgs = [System.Collections.Generic.List[string]]::new()
Add-PathArguments -Arguments $exportArgs -Name "--artifact-report-index" -Values $ArtifactReportIndex
Add-PathArguments -Arguments $exportArgs -Name "--artifact-report" -Values $ArtifactReport
Add-PathArgument -Arguments $exportArgs -Name "--kernel-runtime-session" -Value $KernelRuntimeSession
Add-PathArgument -Arguments $exportArgs -Name "--runtime-ledger" -Value $RuntimeLedger
Add-PathArgument -Arguments $exportArgs -Name "--witness-bundle" -Value $WitnessBundle
Add-PathArgument -Arguments $exportArgs -Name "--world-compare" -Value $WorldCompare
$exportArgs.Add("--output") | Out-Null
$exportArgs.Add($summaryPathResolved) | Out-Null
$exportArgs.Add("--report-markdown") | Out-Null
$exportArgs.Add($reportMarkdownPathResolved) | Out-Null
$exportArgs.Add("--check-text") | Out-Null
$exportArgs.Add($checkTextPathResolved) | Out-Null

Push-Location $repoRoot
try {
    & $PythonExe $exportScript @($exportArgs.ToArray())
    if ($LASTEXITCODE -ne 0) {
        throw ("compiler lifecycle summary exporter failed with exit code {0}" -f $LASTEXITCODE)
    }
} finally {
    Pop-Location
}

if (-not $SkipGate) {
    & $checkScript `
        -Summary $summaryPathResolved `
        -OutputPath $gateTextPathResolved
}

if (-not $SkipReport) {
    & $reportScript `
        -Summary $summaryPathResolved `
        -OutputPath $summaryReportPathResolved
}

Write-Host "[COMPILER-LIFECYCLE-SUMMARY-SIDECAR] result=ok"
Write-Host ("output_root={0}" -f $outputRootPath)
Write-Host ("summary={0}" -f $summaryPathResolved)
Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
Write-Host ("check_text={0}" -f $checkTextPathResolved)
if (-not $SkipGate) {
    Write-Host ("gate_text={0}" -f $gateTextPathResolved)
}
if (-not $SkipReport) {
    Write-Host ("summary_report={0}" -f $summaryReportPathResolved)
}
