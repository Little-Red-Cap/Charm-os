param(
    [string]$Compiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$OutDir = "$PSScriptRoot/out",
    [string]$StorePath = "$PSScriptRoot/out/appstore.bin",
    [string]$ElfBase = "0x24070000"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../../..")
$packToolSource = Join-Path $repoRoot "Examples/system/app_abi_store_pack_tool"
$packToolBuild = Join-Path $packToolSource "cmake-build-app-abi-store-pack-tool"

& (Join-Path $PSScriptRoot "build_app_elf_samples.ps1") `
    -ToolchainPrefix $ToolchainPrefix `
    -OutDir $OutDir `
    -IncDir $OutDir `
    -ElfBase $ElfBase

cmake -S $packToolSource -B $packToolBuild -G Ninja -DCMAKE_CXX_COMPILER="$Compiler"
cmake --build $packToolBuild

$packTool = Join-Path $packToolBuild "app-abi-store-pack.exe"
$hello = Join-Path $OutDir "hello_app.elf"
$player = Join-Path $OutDir "player_min.elf"
& $packTool $StorePath "hello_app=$hello" "player_min=$player"

Write-Host "[ok] app store built at $StorePath"
