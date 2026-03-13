param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Command([string]$Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Normalize-Glob([string]$Glob) {
    return $Glob -replace "\\*\\*", "*"
}

function Match-Any([string]$Value, [string[]]$Globs) {
    foreach ($glob in $Globs) {
        $g = Normalize-Glob $glob
        if ($Value -like $g) { return $true }
    }
    return $false
}

function Get-RelativePath([string]$Base, [string]$Path) {
    $uriBase = [Uri]("$Base" + [IO.Path]::DirectorySeparatorChar)
    $uriPath = [Uri]$Path
    return $uriBase.MakeRelativeUri($uriPath).ToString().Replace("/", "\")
}

function Find-Matches {
    param(
        [string]$Pattern,
        [string[]]$IncludeGlobs,
        [string[]]$ExcludeGlobs
    )

    $matches = @()
    if (Test-Command "rg") {
        $args = @("-n", "--pcre2", $Pattern, $Root)
        foreach ($glob in $IncludeGlobs) { $args += @("--glob", $glob) }
        foreach ($glob in $ExcludeGlobs) { $args += @("--glob", "!$glob") }
        $output = & rg @args 2>$null
        if ($LASTEXITCODE -eq 0) {
            $matches = $output
        } elseif ($LASTEXITCODE -ne 1) {
            throw "rg failed with exit code $LASTEXITCODE"
        }
        return $matches
    }

    $files = Get-ChildItem -Path $Root -Recurse -File
    foreach ($file in $files) {
        $rel = Get-RelativePath -Base $Root -Path $file.FullName
        if ($IncludeGlobs.Count -gt 0 -and -not (Match-Any $rel $IncludeGlobs)) { continue }
        if ($ExcludeGlobs.Count -gt 0 -and (Match-Any $rel $ExcludeGlobs)) { continue }
        $hits = Select-String -Path $file.FullName -Pattern $Pattern -AllMatches
        foreach ($hit in $hits) {
            $matches += "${rel}:$($hit.LineNumber):$($hit.Line)"
        }
    }
    return $matches
}

function Fail-Rule {
    param(
        [string]$Name,
        [string]$Message,
        [string[]]$Matches
    )
    Write-Host ""
    Write-Host "[arch_lint] $Name"
    Write-Host "  $Message"
    foreach ($m in $Matches) {
        Write-Host "  $m"
    }
}

$failed = $false

# Rule: core modules must not use std::chrono (PC-only).
$matches = Find-Matches `
    -Pattern "std::chrono|<chrono>" `
    -IncludeGlobs @("Modules/**") `
    -ExcludeGlobs @(
        "Modules/thirdparty/**",
        "Modules/platform/win/**",
        "Modules/platform/boards/win_stub/**",
        "Modules/platform/win_stub/**",
        "Modules/io/hal/hal_win.cppm"
    )
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "core-no-std-chrono" `
        -Message "core modules must use charm.system.clock; std::chrono is PC-only." `
        -Matches $matches
    $failed = $true
}

# Rule: input.sampler must be removed (RawSampler + Router only).
$matches = Find-Matches `
    -Pattern "input\\.sampler" `
    -IncludeGlobs @("Modules/**", "Examples/**") `
    -ExcludeGlobs @("Modules/thirdparty/**")
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "no-input-sampler" `
        -Message "input.sampler is removed; use input.raw_sampler + router." `
        -Matches $matches
    $failed = $true
}

# Rule: examples should not import full bringup entry.
$matches = Find-Matches `
    -Pattern "^[\\s]*import[\\s]+charm\\.system\\.bringup[\\s]*;" `
    -IncludeGlobs @("Examples/**") `
    -ExcludeGlobs @(
        "Examples/project/**",
        "Examples/system/**",
        "Examples/**/cmake-build-*/**",
        "Draft/**"
    )
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "examples-no-full-bringup" `
        -Message "examples should use bringup.console or app_host; full bringup is reserved for project/system examples." `
        -Matches $matches
    $failed = $true
}

# Rule: protocol modules must not import platform/hal.
$matches = Find-Matches `
    -Pattern "^[\\s]*import[\\s]+(platform\\.|hal_)" `
    -IncludeGlobs @("Modules/io/at/**", "Modules/io/proto/**") `
    -ExcludeGlobs @()
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "proto-no-platform-hal-import" `
        -Message "protocol modules must not import platform or HAL." `
        -Matches $matches
    $failed = $true
}

# Rule: protocol busy-spin (loop + time wait in same file).
$loopFiles = Find-Matches `
    -Pattern "(while\\s*\\(true\\)|for\\s*\\(;;\\))" `
    -IncludeGlobs @("Modules/io/at/**", "Modules/io/proto/**") `
    -ExcludeGlobs @()
$spinMatches = @()
if (@($loopFiles).Count -gt 0) {
    $loopFileSet = @{}
    foreach ($m in $loopFiles) {
        $path = $m.Split(":", 2)[0]
        $loopFileSet[$path] = $true
    }
    foreach ($path in $loopFileSet.Keys) {
        $full = Join-Path $Root $path
        $hits = Select-String -Path $full -Pattern "now_ms|sleep_for|sleep\\(|delay_ms|delay_us|wait_timeout" -AllMatches
        if ($hits) {
            foreach ($hit in $hits) {
                $spinMatches += "${path}:$($hit.LineNumber):$($hit.Line)"
            }
        }
    }
}
if (@($spinMatches).Count -gt 0) {
    Fail-Rule -Name "proto-no-busy-spin" `
        -Message "protocol modules must not busy-spin; use reactor/timeouts." `
        -Matches $spinMatches
    $failed = $true
}

# Rule: forbid imports of removed/deprecated module names.
$deletedModules = @(
    "input.gesture",
    "charm.ui.vivid.full",
    "service.fifo",
    "gui.ui_input_router_bridge",
    "charm.widgets.text",
    "input_router_bridge"
)
$deprecatedModules = @(
    "charm.font.font_noto_ascii_12",
    "charm.font.font_noto_sc_12"
)

function Build-Import-Pattern([string[]]$Modules) {
    $escaped = $Modules | ForEach-Object { [Regex]::Escape($_) }
    return "^[\\s]*(export\\s+)?import\\s+(" + ($escaped -join "|") + ")\\b"
}

$deletedPattern = Build-Import-Pattern $deletedModules
$matches = Find-Matches `
    -Pattern $deletedPattern `
    -IncludeGlobs @("Modules/**", "Examples/**") `
    -ExcludeGlobs @(
        "Modules/thirdparty/**",
        "Modules/**/*.bak",
        "Examples/**/cmake-build-*/**",
        "Draft/**"
    )
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "no-removed-modules" `
        -Message "removed modules must not be imported." `
        -Matches $matches
    $failed = $true
}

$deprecatedPattern = Build-Import-Pattern $deprecatedModules
$matches = Find-Matches `
    -Pattern $deprecatedPattern `
    -IncludeGlobs @("Modules/**", "Examples/**") `
    -ExcludeGlobs @(
        "Modules/thirdparty/**",
        "Modules/gfx/font/font_defaults_noto.cppm",
        "Modules/**/*.bak",
        "Examples/**/cmake-build-*/**",
        "Draft/**"
    )
if (@($matches).Count -gt 0) {
    Fail-Rule -Name "no-deprecated-module-imports" `
        -Message "deprecated module imports are forbidden; use gfx/font defaults or updated APIs." `
        -Matches $matches
    $failed = $true
}

# Rule: public aggregates must not re-export internal/bridge/compat modules.
$publicAggregates = @(
    "Modules/ui/vivid/charm.ui.vivid.cppm",
    "Modules/ui/ink/charm.ui.ink.cppm"
)
$internalExportPattern = "^[\\s]*export\\s+import\\s+(charm\\.core\\.soa_|gui\\.ui_semantics_bridge|charm\\.widgets\\.)"
$bridgeExportPattern = "^[\\s]*export\\s+import\\s+\\S*(bridge|compat|alias)\\S*"
$defaultsNotoExportPattern = "^[\\s]*export\\s+import\\s+charm\\.font\\.defaults_noto\\b"

foreach ($file in $publicAggregates) {
    $full = Join-Path $Root $file
    if (-not (Test-Path $full)) { continue }
    $hits = Select-String -Path $full -Pattern $internalExportPattern
    if ($hits) {
        $matches = $hits | ForEach-Object { "${file}:$($_.LineNumber):$($_.Line)" }
        Fail-Rule -Name "public-export-internal" `
            -Message "public aggregates must not export internal modules." `
            -Matches $matches
        $failed = $true
    }
    $hits = Select-String -Path $full -Pattern $bridgeExportPattern
    if ($hits) {
        $matches = $hits | ForEach-Object { "${file}:$($_.LineNumber):$($_.Line)" }
        Fail-Rule -Name "public-export-bridge" `
            -Message "public aggregates must not export bridge/compat/alias modules." `
            -Matches $matches
        $failed = $true
    }
    $hits = Select-String -Path $full -Pattern $defaultsNotoExportPattern
    if ($hits) {
        $matches = $hits | ForEach-Object { "${file}:$($_.LineNumber):$($_.Line)" }
        Fail-Rule -Name "public-export-defaults-noto" `
            -Message "defaults_noto is an optional resource module; do not re-export from public aggregates." `
            -Matches $matches
        $failed = $true
    }
}

# Rule: bridge/compat/alias modules must declare lifecycle.
$bridgeFiles = Get-ChildItem -Path $Root -Recurse -File -Include "*bridge*.cppm", "*compat*.cppm", "*alias*.cppm"
$bridgeExclude = @("Modules/thirdparty/**", "Modules/**/*.bak", "Draft/**")
foreach ($file in $bridgeFiles) {
    $rel = Get-RelativePath -Base $Root -Path $file.FullName
    if (Match-Any $rel $bridgeExclude) { continue }
    $hit = Select-String -Path $file.FullName -Pattern "lifecycle" -Quiet
    if (-not $hit) {
        Fail-Rule -Name "bridge-missing-lifecycle" `
            -Message "bridge/compat/alias modules must declare lifecycle." `
            -Matches @($rel)
        $failed = $true
    }
}

if ($failed) {
    Write-Host ""
    Write-Host "[arch_lint] FAILED"
    exit 1
}

Write-Host "[arch_lint] OK"


