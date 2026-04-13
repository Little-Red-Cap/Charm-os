param(
    [string]$Source = "Examples/init/materialize_observe_demo",
    [string]$BuildDir = "cmake-build-init-observe-demo-clang",
    [string]$CCompiler = "clang",
    [string]$CxxCompiler = "clang++",
    [string]$BuildTarget = "init-materialize-observe-demo",
    [string]$ExportTarget = "export_materialized_graph_demo",
    [string]$Dot = "",
    [string]$Json = "",
    [int]$Jobs = 8,
    [switch]$Clean,
    [switch]$ConfigureOnly,
    [string[]]$Case = @(),
    [switch]$AllCases,
    [switch]$ListCases
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-ExportCases {
    return @(
        @{
            Name = 'materialize-observe-demo'
            Source = 'Examples/init/materialize_observe_demo'
            BuildDir = 'cmake-build-init-observe-demo-clang'
            BuildTarget = 'init-materialize-observe-demo'
            ExportTarget = 'export_materialized_graph_demo'
            DotCache = 'MATERIALIZE_OBSERVE_DOT_PATH'
            JsonCache = 'MATERIALIZE_OBSERVE_JSON_PATH'
            DefaultDot = 'materialized_graph.dot'
            DefaultJson = 'materialized_graph.sample.json'
            ExtraCache = @()
        },
        @{
            Name = 'bringup-block-observe-demo'
            Source = 'Examples/init/bringup_block_observe_demo'
            BuildDir = 'cmake-build-init-bringup-block-observe-clang'
            BuildTarget = 'init-bringup-block-observe-demo'
            ExportTarget = 'export_bringup_block_materialized_graph'
            DotCache = 'BRINGUP_BLOCK_OBSERVE_DOT_PATH'
            JsonCache = 'BRINGUP_BLOCK_OBSERVE_JSON_PATH'
            DefaultDot = 'bringup_block_materialized_graph.dot'
            DefaultJson = 'bringup_block_materialized_graph.sample.json'
            ExtraCache = @()
        }
    )
}

function Invoke-LegacyExport {
    $sourceDir = Join-Path $repoRoot $Source
    $buildDir = Join-Path $repoRoot $BuildDir

    if (-not (Test-Path $sourceDir)) {
        throw "source not found: $sourceDir"
    }

    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "[CFG] $sourceDir"
    cmake -S $sourceDir -B $buildDir -G Ninja -D CMAKE_C_COMPILER=$CCompiler -D CMAKE_CXX_COMPILER=$CxxCompiler
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed"
    }

    if ($ConfigureOnly) {
        Write-Host "[OK] configure finished: $buildDir"
        return
    }

    if ([string]::IsNullOrWhiteSpace($Dot) -and [string]::IsNullOrWhiteSpace($Json)) {
        Write-Host "[EXPORT] target=$ExportTarget"
        cmake --build $buildDir --target $ExportTarget -j $Jobs
        if ($LASTEXITCODE -ne 0) {
            throw "export target failed"
        }

        Write-Host "[OK] exported via target"
        Write-Host "[DOT]  $(Join-Path $buildDir 'materialized_graph.dot')"
        Write-Host "[JSON] $(Join-Path $buildDir 'materialized_graph.sample.json')"
        return
    }

    Write-Host "[BUILD] target=$BuildTarget"
    cmake --build $buildDir --target $BuildTarget -j $Jobs
    if ($LASTEXITCODE -ne 0) {
        throw "build failed"
    }

    $exePath = Join-Path $buildDir "$BuildTarget.exe"
    if (-not (Test-Path $exePath)) {
        throw "executable not found: $exePath"
    }

    $args = @()
    if (-not [string]::IsNullOrWhiteSpace($Dot)) {
        $args += @("--dot", $Dot)
    }
    if (-not [string]::IsNullOrWhiteSpace($Json)) {
        $args += @("--json", $Json)
    }

    Write-Host "[RUN] $exePath $($args -join ' ')"
    & $exePath @args
    if ($LASTEXITCODE -ne 0) {
        throw "export run failed"
    }

    Write-Host "[OK] exported via direct run"
}

function Invoke-ManifestCase {
    param(
        [hashtable]$Entry,
        [string]$DotOverride = '',
        [string]$JsonOverride = ''
    )

    $sourceDir = Join-Path $repoRoot $Entry.Source
    $buildDir = Join-Path $repoRoot $Entry.BuildDir
    if (-not (Test-Path $sourceDir)) {
        throw "source not found: $sourceDir"
    }

    if ($Clean -and (Test-Path $buildDir)) {
        Remove-Item -Recurse -Force $buildDir
    }

    $dotPath = if ([string]::IsNullOrWhiteSpace($DotOverride)) {
        Join-Path $buildDir $Entry.DefaultDot
    } else {
        $DotOverride
    }
    $jsonPath = if ([string]::IsNullOrWhiteSpace($JsonOverride)) {
        Join-Path $buildDir $Entry.DefaultJson
    } else {
        $JsonOverride
    }

    $configureArgs = @(
        '-S', $sourceDir,
        '-B', $buildDir,
        '-G', 'Ninja',
        '-D', "CMAKE_C_COMPILER=$CCompiler",
        '-D', "CMAKE_CXX_COMPILER=$CxxCompiler"
    )
    if (-not [string]::IsNullOrWhiteSpace($Entry.DotCache)) {
        $configureArgs += @('-D', "$($Entry.DotCache)=$dotPath")
    }
    if (-not [string]::IsNullOrWhiteSpace($Entry.JsonCache)) {
        $configureArgs += @('-D', "$($Entry.JsonCache)=$jsonPath")
    }
    foreach ($cacheArg in $Entry.ExtraCache) {
        $configureArgs += @('-D', $cacheArg)
    }

    Write-Host "[CFG][$($Entry.Name)] $sourceDir"
    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "configure failed: $($Entry.Name)"
    }

    if ($ConfigureOnly) {
        Write-Host "[OK][$($Entry.Name)] configure finished: $buildDir"
        return
    }

    if (-not [string]::IsNullOrWhiteSpace($Entry.ExportTarget)) {
        Write-Host "[EXPORT][$($Entry.Name)] target=$($Entry.ExportTarget)"
        cmake --build $buildDir --target $Entry.ExportTarget -j $Jobs
        if ($LASTEXITCODE -ne 0) {
            throw "export target failed: $($Entry.Name)"
        }
    } else {
        Write-Host "[BUILD][$($Entry.Name)] target=$($Entry.BuildTarget)"
        cmake --build $buildDir --target $Entry.BuildTarget -j $Jobs
        if ($LASTEXITCODE -ne 0) {
            throw "build failed: $($Entry.Name)"
        }
    }

    Write-Host "[DOT][$($Entry.Name)]  $dotPath"
    Write-Host "[JSON][$($Entry.Name)] $jsonPath"
}

$manifestCases = Get-ExportCases

if ($ListCases) {
    foreach ($entry in $manifestCases) {
        Write-Host "$($entry.Name) -> source=$($entry.Source) target=$($entry.ExportTarget)"
    }
    exit 0
}

$useManifest = $AllCases -or $Case.Count -gt 0
if (-not $useManifest) {
    Invoke-LegacyExport
    exit 0
}

if (($AllCases -or $Case.Count -gt 1) -and (-not [string]::IsNullOrWhiteSpace($Dot) -or -not [string]::IsNullOrWhiteSpace($Json))) {
    throw "-Dot/-Json can only be used with a single selected case"
}

$selected = @()
if ($AllCases) {
    $selected = $manifestCases
} else {
    foreach ($caseName in $Case) {
        $match = $manifestCases | Where-Object { $_.Name -eq $caseName }
        if (-not $match) {
            throw "unknown case: $caseName"
        }
        $selected += $match
    }
}

foreach ($entry in $selected) {
    $dotOverride = if ($selected.Count -eq 1) { $Dot } else { '' }
    $jsonOverride = if ($selected.Count -eq 1) { $Json } else { '' }
    Invoke-ManifestCase -Entry $entry -DotOverride $dotOverride -JsonOverride $jsonOverride
}

if ($ConfigureOnly) {
    Write-Host '[OK] configure finished for selected cases'
} else {
    Write-Host '[OK] materialized graph export finished for selected cases'
}
