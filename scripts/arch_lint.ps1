param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [switch]$EnableSsuSubmitGate,
    [switch]$OnlyVividImportBoundary
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$script:ArchLintUseRg = $null

function Test-Command([string]$Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-RgAvailable {
    if ($null -ne $script:ArchLintUseRg) {
        return $script:ArchLintUseRg
    }

    $script:ArchLintUseRg = $false
    if (-not (Test-Command "rg")) {
        return $script:ArchLintUseRg
    }

    try {
        & rg --version >$null 2>$null
        if ($LASTEXITCODE -eq 0) {
            $script:ArchLintUseRg = $true
        }
    } catch {
        Write-Host ("[arch_lint] rg unavailable; falling back to Select-String ({0})" -f $_.Exception.Message)
    }

    return $script:ArchLintUseRg
}

function Normalize-Glob([string]$Glob) {
    $g = $Glob.Replace("/", "\")
    return $g -replace "\*\*", "*"
}

function Match-Any([string]$Value, [string[]]$Globs) {
    foreach ($glob in $Globs) {
        $g = Normalize-Glob $glob
        if ($Value -like $g) { return $true }
    }
    return $false
}

function Should-PruneDirectory([string]$RelativePath, [string[]]$ExcludeGlobs) {
    if ([string]::IsNullOrWhiteSpace($RelativePath)) {
        return $false
    }

    if (Match-Any $RelativePath $ExcludeGlobs -or Match-Any ($RelativePath + "\*") $ExcludeGlobs) {
        return $true
    }

    $parts = $RelativePath.Split([char[]]@("\", "/"), [System.StringSplitOptions]::RemoveEmptyEntries)
    foreach ($part in $parts) {
        if ($part -like "cmake-build-*" -or $part -eq "generated") {
            return $true
        }
    }

    return $false
}

function Get-RelativePath([string]$Base, [string]$Path) {
    $uriBase = [Uri]("$Base" + [IO.Path]::DirectorySeparatorChar)
    $uriPath = [Uri]$Path
    return $uriBase.MakeRelativeUri($uriPath).ToString().Replace("/", "\")
}

function Get-StaticGlobPrefix([string]$Glob) {
    $g = $Glob.Replace("/", "\")
    $wildcardIndex = $g.IndexOfAny([char[]]@("*", "?", "["))
    if ($wildcardIndex -ge 0) {
        $g = $g.Substring(0, $wildcardIndex)
    }
    return $g.TrimEnd("\")
}

function Get-FallbackSearchRoots([string[]]$IncludeGlobs) {
    if ($IncludeGlobs.Count -eq 0) {
        return @($Root)
    }

    $roots = @()
    foreach ($glob in $IncludeGlobs) {
        $prefix = Get-StaticGlobPrefix $glob
        $candidate = $Root
        if (-not [string]::IsNullOrWhiteSpace($prefix)) {
            $candidate = Join-Path $Root $prefix
        }
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate
            if ($item.PSIsContainer) {
                $roots += $item.FullName
            } else {
                $roots += $item.DirectoryName
            }
        }
    }

    if ($roots.Count -eq 0) {
        return @()
    }

    return @($roots | Sort-Object -Unique)
}

function Get-FallbackFiles {
    param(
        [string[]]$SearchRoots,
        [string[]]$ExcludeGlobs
    )

    foreach ($searchRoot in $SearchRoots) {
        if (-not (Test-Path -LiteralPath $searchRoot)) { continue }

        $stack = New-Object System.Collections.Generic.Stack[string]
        $stack.Push($searchRoot)
        while ($stack.Count -gt 0) {
            $dir = $stack.Pop()
            $dirRel = Get-RelativePath -Base $Root -Path $dir
            if (Should-PruneDirectory $dirRel $ExcludeGlobs) {
                continue
            }

            foreach ($childDir in Get-ChildItem -LiteralPath $dir -Directory -ErrorAction SilentlyContinue) {
                $childRel = Get-RelativePath -Base $Root -Path $childDir.FullName
                if (Should-PruneDirectory $childRel $ExcludeGlobs) {
                    continue
                }
                $stack.Push($childDir.FullName)
            }

            foreach ($file in Get-ChildItem -LiteralPath $dir -File -ErrorAction SilentlyContinue) {
                $file
            }
        }
    }
}

function Find-Matches {
    param(
        [string]$Pattern,
        [string[]]$IncludeGlobs,
        [string[]]$ExcludeGlobs
    )

    $matches = @()
    if (Test-RgAvailable) {
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

    $searchRoots = Get-FallbackSearchRoots $IncludeGlobs
    foreach ($file in Get-FallbackFiles -SearchRoots $searchRoots -ExcludeGlobs $ExcludeGlobs) {
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

function Invoke-VividImportBoundaryRules {
    $vividRestrictedImportPattern =
        "^[\\s]*(export\\s+)?import\\s+(" +
        "charm\\.ui\\.vivid_internal" +
        "|charm\\.core\\.soa_(kernel|factory|gui|payload)" +
        "|charm\\.gfx\\.draw_cmd(:[A-Za-z0-9_]+)?" +
        "|charm\\.gfx\\.host_tools" +
        "|charm\\.ui\\.scene\\.(builder_support|layer_support)" +
        "|charm\\.ui\\.scene:render_detail" +
        ")\\b"

    $vividBoundaryExcludes = @(
        "Modules/thirdparty/**",
        "Draft/**",
        "**/cmake-build-*/**",
        "generated/**",
        "**/generated/**",
        "Modules/ui/vivid/charm.ui.vivid_internal.cppm",
        "Modules/ui/vivid/core/**",
        "Modules/ui/vivid/gfx/**",
        "Modules/ui/vivid/widgets/**",
        "Examples/ui/vivid/soa_demo/**",
        "Examples/ui/vivid/dropdown_popup_demo/**",
        "Examples/ui/vivid/menu_tree_demo/**",
        "Examples/project/player/win/main.ui_ci.object_tree.cpp",
        "Examples/project/player/win/main.host_module.cppm"
    )

    $matches = Find-Matches `
        -Pattern $vividRestrictedImportPattern `
        -IncludeGlobs @(
            "Modules/ui/vivid/**",
            "Examples/ui/vivid/**",
            "Examples/project/player/**"
        ) `
        -ExcludeGlobs $vividBoundaryExcludes

    if (@($matches).Count -gt 0) {
        Fail-Rule -Name "vivid-import-boundary" `
            -Message "product-facing Vivid paths must import public scene/vivid surfaces instead of vivid_internal, SoA, DrawCmd, host tools, or scene support internals." `
            -Matches $matches
        return $true
    }

    $productRestrictedImportPattern =
        "^[\\s]*(export\\s+)?import\\s+(" +
        "charm\\.gfx\\.snapshot" +
        "|charm\\.font\\.provider_freetype" +
        ")\\b"

    $productRestrictedExcludes = @(
        "Draft/**",
        "**/cmake-build-*/**",
        "generated/**",
        "**/generated/**",
        "Examples/project/player/win/**"
    )

    $matches = Find-Matches `
        -Pattern $productRestrictedImportPattern `
        -IncludeGlobs @("Examples/project/h747-lab/**") `
        -ExcludeGlobs $productRestrictedExcludes

    if (@($matches).Count -gt 0) {
        Fail-Rule -Name "vivid-product-host-import" `
            -Message "PRODUCT/H747 Vivid paths must not import snapshot or FreeType provider directly; admit host/resource capabilities through product gates first." `
            -Matches $matches
        return $true
    }

    return $false
}

if ($OnlyVividImportBoundary) {
    if (Invoke-VividImportBoundaryRules) {
        Write-Host ""
        Write-Host "[arch_lint] FAILED"
        exit 1
    }

    Write-Host "[arch_lint] OK"
    exit 0
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

# Rule: product-facing Vivid paths must not import internal runtime surfaces.
if (Invoke-VividImportBoundaryRules) {
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



# Optional gate: SSU submit mapping documentation.
if ($EnableSsuSubmitGate) {
    $gate = Join-Path $Root "scripts/ssu_submit_gate.ps1"
    if (Test-Path $gate) {
        & $gate -Root $Root
        if ($LASTEXITCODE -ne 0) {
            $failed = $true
        }
    } else {
        Write-Host "[arch_lint] ssu_submit_gate.ps1 not found"
        $failed = $true
    }
}
if ($failed) {
    Write-Host ""
    Write-Host "[arch_lint] FAILED"
    exit 1
}

Write-Host "[arch_lint] OK"



