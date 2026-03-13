param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\.." ).Path,
    [string]$Out = (Join-Path $Root "reports\demo_entry_scan.txt")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$exampleRoot = Join-Path $Root "Examples"
$files = Get-ChildItem -Path $exampleRoot -Recurse -File -Include *.cpp, *.cppm
$files = $files | Where-Object { $_.FullName -notmatch 'cmake-build-' }

function Get-RelativePath([string]$Base, [string]$Path) {
    $uriBase = [Uri]("$Base" + [IO.Path]::DirectorySeparatorChar)
    $uriPath = [Uri]$Path
    return $uriBase.MakeRelativeUri($uriPath).ToString().Replace("/", "\")
}

$lines = @()
$lines += "Demo entry scan"
$lines += "Root: $Root"
$lines += "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$lines += ""
$lines += "Entry rules (minimal)"
$lines += "- console-only demo: import charm.system.bringup.console"
$lines += "- app/demo runtime: import charm.system.app_host"
$lines += "- full bringup: import charm.system.bringup"
$lines += ""

$pattern = '^[\s\uFEFF]*(export\s+)?import\s+charm\.system\.(bringup(\.console|\.win_stub|\.stm32_stub)?|app_host)\b'

foreach ($file in $files) {
    $hits = Select-String -Path $file.FullName -Pattern $pattern
    if (-not $hits) { continue }
    $rel = Get-RelativePath -Base $Root -Path $file.FullName
    $imports = $hits | ForEach-Object { $_.Line.Trim() } | Sort-Object -Unique
    $lines += "$rel"
    foreach ($imp in $imports) {
        $lines += "  $imp"
    }
    $lines += ""
}

$agg = Join-Path $Root "Modules\system\charm.system.cppm"
if (Test-Path $agg) {
    $lines += "Aggregate entry exports (charm.system.cppm)"
    $exports = Select-String -Path $agg -Pattern "^[\s]*export\s+import\s+.*" |
        ForEach-Object { $_.Line.Trim() }
    foreach ($line in $exports) {
        $lines += "  $line"
    }
}

$lines += ""
$lines += "BoardCaps usage in Examples (should be empty for demos)"
$boardcapsPattern = '\bBoardCaps\b|\bmake_board_caps\s*\('
$boardcapsHits = @()
foreach ($file in $files) {
    $hits = Select-String -Path $file.FullName -Pattern $boardcapsPattern
    if ($hits) {
        $boardcapsHits += (Get-RelativePath -Base $Root -Path $file.FullName)
    }
}
if ($boardcapsHits.Count -eq 0) {
    $lines += "  (none)"
} else {
    foreach ($rel in ($boardcapsHits | Sort-Object -Unique)) {
        $lines += "  $rel"
    }
}

$lines += ""
$lines += "Baseline metrics (imports)"
$metricTargets = @(
    "Examples\\io\\out\\windows\\main.cpp",
    "Examples\\io\\input_pump_win_demo\\main.cpp",
    "Examples\\fs\\fs_block_vfs_demo\\main.cpp",
    "Examples\\usb\\usb_msc_block_demo\\main.cpp"
)
foreach ($rel in $metricTargets) {
    $path = Join-Path $Root $rel
    if (-not (Test-Path $path)) {
        $lines += "  ${rel}: missing"
        continue
    }
    $imports = Select-String -Path $path -Pattern "import " -SimpleMatch
    $importCount = @($imports).Count
    $systemCount = @(Select-String -Path $path -Pattern "charm.system." -SimpleMatch).Count
    $lines += "  ${rel}: imports=$importCount charm.system=$systemCount"
}

$lines | Set-Content -Path $Out -Encoding UTF8
Write-Host "[demo_entry_scan] wrote $Out"
