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

$pattern = '^[\s]*(export\s+)?import\s+charm\.system\.(bringup(\.console|\.win_stub|\.stm32_stub)?|app_host)\b'

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

$lines | Set-Content -Path $Out -Encoding UTF8
Write-Host "[demo_entry_scan] wrote $Out"
