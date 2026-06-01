param(
    [string]$Output = "",
    [string]$PrimaryFont = "",
    [string]$FallbackFont = "",
    [string]$Track = "",
    [string]$Cover = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\player_md3_resources"
}

function Copy-IfProvided {
    param(
        [string]$Source,
        [string]$Destination
    )
    if ([string]::IsNullOrWhiteSpace($Source)) {
        return $false
    }
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Resource file not found: $Source"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    return $true
}

$ResolvedOutput = [System.IO.Path]::GetFullPath($Output)
$FontDir = Join-Path $ResolvedOutput "font"
$MusicDir = Join-Path $ResolvedOutput "music"
New-Item -ItemType Directory -Force -Path $FontDir | Out-Null
New-Item -ItemType Directory -Force -Path $MusicDir | Out-Null

$PrimaryCopied = Copy-IfProvided -Source $PrimaryFont -Destination (Join-Path $FontDir "NotoSansSC-Regular.ttf")
$FallbackCopied = Copy-IfProvided -Source $FallbackFont -Destination (Join-Path $FontDir "NotoSans-Regular.ttf")
$TrackCopied = $false
if (-not [string]::IsNullOrWhiteSpace($Track)) {
    $TrackName = Split-Path -Leaf $Track
    $TrackCopied = Copy-IfProvided -Source $Track -Destination (Join-Path $MusicDir $TrackName)
}
$CoverCopied = $false
if (-not [string]::IsNullOrWhiteSpace($Cover)) {
    $CoverExt = [System.IO.Path]::GetExtension($Cover).ToLowerInvariant()
    $CoverName = if ($CoverExt -eq ".png") { "cover.png" } elseif ($CoverExt -eq ".bmp") { "cover.bmp" } else { "cover.jpg" }
    $CoverCopied = Copy-IfProvided -Source $Cover -Destination (Join-Path $MusicDir $CoverName)
}

Write-Host "H747 Player MD3 resource staging"
Write-Host "  output:   $ResolvedOutput"
Write-Host "  primary:  $PrimaryCopied -> /font/NotoSansSC-Regular.ttf"
Write-Host "  fallback: $FallbackCopied -> /font/NotoSans-Regular.ttf"
Write-Host "  track:    $TrackCopied -> /music/<track>"
Write-Host "  cover:    $CoverCopied -> /music/cover.(jpg|png|bmp)"
Write-Host ""
Write-Host "Copy this directory's contents to the eMMC FAT root."
Write-Host "Expected board paths:"
Write-Host "  /font/NotoSansSC-Regular.ttf"
Write-Host "  /font/NotoSans-Regular.ttf"
Write-Host "  /music/<one mp3/flac/wav>"
Write-Host "  /music/cover.jpg or /music/folder.jpg"
