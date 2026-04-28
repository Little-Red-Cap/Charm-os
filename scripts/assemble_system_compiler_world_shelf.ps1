param(
    [string[]]$BiographySummary = @(),
    [string[]]$BiographyRoot = @(),
    [string[]]$SearchRoot = @(),
    [string]$OutputRoot = "",
    [string]$SummaryPath = "",
    [string]$ReportMarkdownPath = "",
    [string]$CheckTextPath = "",
    [string]$DiscoveredBiographiesPath = "",
    [string]$ExportLogPath = "",
    [string]$ValidationLogPath = "",
    [string]$GateLogPath = "",
    [string]$PythonExe = "",
    [string]$Profile = "system-compiler-world-shelf",
    [switch]$Clean,
    [switch]$SkipGate,
    [string]$RequireResult = "ok",
    [int]$RequireBiographyCount = -1,
    [int]$RequireUniqueWorldCount = -1,
    [int]$RequireOkCount = -1,
    [int]$MaxFailCount = -1,
    [int]$RequireCompareAttachedCount = -1,
    [int]$RequireNotAttachedCount = -1,
    [int]$RequireStandingCount = -1,
    [int]$RequireImprovedCount = -1,
    [int]$RequireDriftedCount = -1,
    [int]$RequireCollapsedCount = -1
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

function Add-ToolArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    $Arguments.Add($Name) | Out-Null
    $Arguments.Add($Value) | Out-Null
}

function Add-ToolArgumentsFromArray {
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
        $Arguments.Add([string]$value) | Out-Null
    }
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

function Add-UniquePath {
    param(
        [System.Collections.Generic.List[string]]$Paths,
        [hashtable]$Seen,
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }

    $resolvedPath = Resolve-FullPath -Path $Path
    if (-not $Seen.ContainsKey($resolvedPath)) {
        $Seen[$resolvedPath] = $true
        $Paths.Add($resolvedPath) | Out-Null
    }
}

function Resolve-BiographyPathsFromRoots {
    param(
        [string[]]$Roots
    )

    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($root in @($Roots)) {
        if ([string]::IsNullOrWhiteSpace([string]$root)) {
            continue
        }

        $resolvedRoot = Resolve-FullPath -Path ([string]$root)
        $candidate = Join-Path $resolvedRoot "biography.summary.json"
        if (-not (Test-Path $candidate)) {
            throw "biography summary not found under biography root: $resolvedRoot"
        }

        $paths.Add([System.IO.Path]::GetFullPath($candidate)) | Out-Null
    }

    return @($paths)
}

function Resolve-BiographyPathsFromSearchRoots {
    param(
        [string[]]$Roots
    )

    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($root in @($Roots)) {
        if ([string]::IsNullOrWhiteSpace([string]$root)) {
            continue
        }

        $resolvedRoot = Resolve-FullPath -Path ([string]$root)
        if (-not (Test-Path $resolvedRoot)) {
            throw "search root not found: $resolvedRoot"
        }

        $matches = @(
            Get-ChildItem -LiteralPath $resolvedRoot -Filter "biography.summary.json" -File -Recurse |
                Sort-Object FullName
        )

        foreach ($match in $matches) {
            $paths.Add($match.FullName) | Out-Null
        }
    }

    return @($paths)
}

function Get-DiscoveredBiographyPaths {
    $paths = [System.Collections.Generic.List[string]]::new()
    $seen = @{}

    foreach ($summaryPath in @($BiographySummary)) {
        Add-UniquePath -Paths $paths -Seen $seen -Path ([string]$summaryPath)
    }

    foreach ($summaryPath in @(Resolve-BiographyPathsFromRoots -Roots $BiographyRoot)) {
        Add-UniquePath -Paths $paths -Seen $seen -Path $summaryPath
    }

    foreach ($summaryPath in @(Resolve-BiographyPathsFromSearchRoots -Roots $SearchRoot)) {
        Add-UniquePath -Paths $paths -Seen $seen -Path $summaryPath
    }

    return @($paths)
}

function Write-PathList {
    param(
        [string]$Path,
        [string[]]$Values
    )

    Ensure-ParentDirectory -Path $Path
    $content = @($Values) -join [Environment]::NewLine
    Set-Content -LiteralPath $Path -Encoding utf8 $content
}

function Load-JsonObject {
    param(
        [string]$Path
    )

    return (Get-Content -LiteralPath $Path -Raw -Encoding utf8 | ConvertFrom-Json)
}

function Get-BooleanSwitchValue {
    param(
        [bool]$Value
    )

    if ($Value) {
        return '$true'
    }

    return '$false'
}

$repoRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$resolvedOutputRoot = if ([string]::IsNullOrWhiteSpace($OutputRoot)) { "out/system-compiler-world-shelf" } else { $OutputRoot }
$outputRootPath = Resolve-FullPath -Path $resolvedOutputRoot

if ($Clean) {
    Remove-PathIfExists -Path $outputRootPath
}
Ensure-Directory -Path $outputRootPath

$summaryPathResolved = Get-OutputPath -ExplicitPath $SummaryPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.summary.json"
$reportMarkdownPathResolved = Get-OutputPath -ExplicitPath $ReportMarkdownPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.report.md"
$checkTextPathResolved = Get-OutputPath -ExplicitPath $CheckTextPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.check.txt"
$discoveredBiographiesPathResolved = Get-OutputPath -ExplicitPath $DiscoveredBiographiesPath -OutputRootPath $outputRootPath -DefaultFileName "discovered_biographies.txt"
$exportLogPathResolved = Get-OutputPath -ExplicitPath $ExportLogPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.export.log"
$validationLogPathResolved = Get-OutputPath -ExplicitPath $ValidationLogPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.validate.log"
$gateLogPathResolved = Get-OutputPath -ExplicitPath $GateLogPath -OutputRootPath $outputRootPath -DefaultFileName "biography.index.gate.log"

$exportScript = Join-Path $PSScriptRoot "export_system_compiler_biography_index.py"
$validateBiographyScript = Join-Path $PSScriptRoot "validate_system_compiler_biography.py"
$validateShelfScript = Join-Path $PSScriptRoot "validate_system_compiler_biography_index.py"
$gateShelfScript = Join-Path $PSScriptRoot "check_system_compiler_biography_index_summary.ps1"

foreach ($requiredPath in @($exportScript, $validateBiographyScript, $validateShelfScript, $gateShelfScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing path: $requiredPath"
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
    $biographyPaths = @(Get-DiscoveredBiographyPaths)
    if ($biographyPaths.Count -eq 0) {
        throw "no biography summaries were discovered"
    }

    foreach ($biographyPath in $biographyPaths) {
        if (-not (Test-Path $biographyPath)) {
            throw "biography summary not found: $biographyPath"
        }
    }

    Write-PathList -Path $discoveredBiographiesPathResolved -Values $biographyPaths

    if (Test-Path $validationLogPathResolved) {
        Remove-Item -LiteralPath $validationLogPathResolved -Force
    }

    foreach ($biographyPath in $biographyPaths) {
        $validateBiographyArgs = @(
            $validateBiographyScript,
            "--summary",
            $biographyPath
        )
        Invoke-ExternalTool `
            -Executable $resolvedPythonExe `
            -ArgumentList $validateBiographyArgs `
            -LogPath $validationLogPathResolved `
            -FailureMessage ("biography validation failed: {0}" -f $biographyPath) `
            -AppendLog
    }

    $exportArgs = [System.Collections.Generic.List[string]]::new()
    $exportArgs.Add($exportScript) | Out-Null
    Add-ToolArgumentsFromArray -Arguments $exportArgs -Name "--biography" -Values $biographyPaths
    Add-ToolArgument -Arguments $exportArgs -Name "--output-root" -Value $outputRootPath
    Add-ToolArgument -Arguments $exportArgs -Name "--summary" -Value $summaryPathResolved
    Add-ToolArgument -Arguments $exportArgs -Name "--report-markdown" -Value $reportMarkdownPathResolved
    Add-ToolArgument -Arguments $exportArgs -Name "--check-text" -Value $checkTextPathResolved
    Add-ToolArgument -Arguments $exportArgs -Name "--profile" -Value $Profile
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $exportArgs.ToArray() `
        -LogPath $exportLogPathResolved `
        -FailureMessage "world shelf export failed"

    $validateShelfArgs = @(
        $validateShelfScript,
        "--summary",
        $summaryPathResolved
    )
    Invoke-ExternalTool `
        -Executable $resolvedPythonExe `
        -ArgumentList $validateShelfArgs `
        -LogPath $validationLogPathResolved `
        -FailureMessage "world shelf validation failed" `
        -AppendLog

    if (-not $SkipGate) {
        $gateArgs = [System.Collections.Generic.List[string]]::new()
        Add-ToolArgument -Arguments $gateArgs -Name "-Summary" -Value $summaryPathResolved
        if (-not [string]::IsNullOrWhiteSpace($RequireResult)) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireResult" -Value $RequireResult
        }
        if ($RequireBiographyCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireBiographyCount" -Value ([string]$RequireBiographyCount)
        }
        if ($RequireUniqueWorldCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireUniqueWorldCount" -Value ([string]$RequireUniqueWorldCount)
        }
        if ($RequireOkCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireOkCount" -Value ([string]$RequireOkCount)
        }
        if ($MaxFailCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-MaxFailCount" -Value ([string]$MaxFailCount)
        }
        if ($RequireCompareAttachedCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireCompareAttachedCount" -Value ([string]$RequireCompareAttachedCount)
        }
        if ($RequireNotAttachedCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireNotAttachedCount" -Value ([string]$RequireNotAttachedCount)
        }
        if ($RequireStandingCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireStandingCount" -Value ([string]$RequireStandingCount)
        }
        if ($RequireImprovedCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireImprovedCount" -Value ([string]$RequireImprovedCount)
        }
        if ($RequireDriftedCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireDriftedCount" -Value ([string]$RequireDriftedCount)
        }
        if ($RequireCollapsedCount -ge 0) {
            Add-ToolArgument -Arguments $gateArgs -Name "-RequireCollapsedCount" -Value ([string]$RequireCollapsedCount)
        }

        Invoke-PowerShellFile `
            -PowerShellExe $powerShellExe `
            -ScriptPath $gateShelfScript `
            -ArgumentList $gateArgs.ToArray() `
            -LogPath $gateLogPathResolved `
            -FailureMessage "world shelf gate failed"
    }

    $summaryData = Load-JsonObject -Path $summaryPathResolved
    Write-Host "==> system compiler world shelf"
    Write-Host ("profile={0}" -f [string]$summaryData.profile)
    Write-Host ("output_root={0}" -f $outputRootPath)
    Write-Host ("summary={0}" -f $summaryPathResolved)
    Write-Host ("report_markdown={0}" -f $reportMarkdownPathResolved)
    Write-Host ("check_text={0}" -f $checkTextPathResolved)
    Write-Host ("discovered_biographies={0}" -f $discoveredBiographiesPathResolved)
    Write-Host ("export_log={0}" -f $exportLogPathResolved)
    Write-Host ("validation_log={0}" -f $validationLogPathResolved)
    if (-not $SkipGate) {
        Write-Host ("gate_log={0}" -f $gateLogPathResolved)
    }
    Write-Host ("biography_count={0}" -f [int]$summaryData.summary.biography_count)
    Write-Host ("unique_world_count={0}" -f [int]$summaryData.summary.unique_world_count)
    Write-Host ("compare_attached_count={0}" -f [int]$summaryData.summary.compare_attached_count)
    Write-Host ("not_attached_count={0}" -f [int]$summaryData.summary.not_attached_count)
} finally {
    Pop-Location
}
