param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$Preset = "player-cm7-usb-audio",
    [string]$Target = "stn32h747_hqzy_CM7",
    [string]$Config = "Debug",
    [string]$OpenOcd = "openocd",
    [string]$BoardConfig = "Examples/project/player/stn32h747_HQZY/openocd_swd.cfg",
    [switch]$ConfigureOnly,
    [switch]$BuildOnly,
    [switch]$FlashOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-Command([string]$Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Resolve-BuildDir([string]$ProjectRoot, [string]$PresetName) {
    switch ($PresetName) {
        "player-cm7-usb-audio" { return Join-Path $ProjectRoot "Examples/project/player/stn32h747_HQZY/CM7/cmake-build-player-usb-audio" }
        "player-cm7-usb-self-msc" { return Join-Path $ProjectRoot "Examples/project/player/stn32h747_HQZY/CM7/cmake-build-player-usb-self-msc" }
        "player-cm7-usb-storage" { return Join-Path $ProjectRoot "Examples/project/player/stn32h747_HQZY/CM7/cmake-build-player-usb-storage" }
        default { throw "不支持的 preset: $PresetName" }
    }
}

if (-not (Test-Command "cmake")) {
    throw "未找到 cmake"
}

$projectDir = Join-Path $Root "Examples/project/player/stn32h747_HQZY/CM7"
$buildDir = Resolve-BuildDir -ProjectRoot $Root -PresetName $Preset
$elfPath = Join-Path $buildDir "$Target.elf"
$boardCfgPath = Join-Path $Root $BoardConfig

if (-not $FlashOnly) {
    Write-Host "[player_usb_audio_flash] configure preset: $Preset"
    & cmake --preset $Preset -S $projectDir
    if ($LASTEXITCODE -ne 0) { throw "cmake configure 失败" }
}

if ($ConfigureOnly) {
    Write-Host "[player_usb_audio_flash] configure done: $buildDir"
    exit 0
}

if (-not $FlashOnly) {
    Write-Host "[player_usb_audio_flash] build target: $Target"
    & cmake --build $buildDir --config $Config --target $Target -j 4
    if ($LASTEXITCODE -ne 0) { throw "cmake build 失败" }
}

if ($BuildOnly) {
    Write-Host "[player_usb_audio_flash] build done: $elfPath"
    exit 0
}

if (-not (Test-Path $elfPath)) {
    throw "未找到 elf: $elfPath"
}

if (-not (Test-Command $OpenOcd)) {
    throw "未找到 OpenOCD: $OpenOcd"
}

if (-not (Test-Path $boardCfgPath)) {
    throw "未找到 OpenOCD 板级配置: $boardCfgPath"
}

Write-Host "[player_usb_audio_flash] flash elf: $elfPath"
& $OpenOcd -f $boardCfgPath -c "program $elfPath verify reset exit"
if ($LASTEXITCODE -ne 0) { throw "OpenOCD 烧录失败" }

Write-Host "[player_usb_audio_flash] done"
