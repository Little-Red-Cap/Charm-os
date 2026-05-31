param(
    [string]$Compiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$OutDir = "$PSScriptRoot/out"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$cc = "${ToolchainPrefix}gcc"
$objcopy = "${ToolchainPrefix}objcopy"
$src = Join-Path $PSScriptRoot "modulex_hello_app.S"
$obj = Join-Path $OutDir "modulex_hello_app.o"
$text = Join-Path $OutDir "modulex_hello_app.text.bin"
$ro = Join-Path $OutDir "modulex_hello_app.rodata.bin"
$modulex = Join-Path $OutDir "modulex_hello_app.modulex"

& $cc -mcpu=cortex-m7 -mthumb -ffreestanding -fno-builtin -Os -c $src -o $obj
& $objcopy -O binary -j .text.charm_app_main $obj $text
& $objcopy -O binary -j .rodata.modulex_hello $obj $ro

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../../..")
$packToolSource = Join-Path $repoRoot "Examples/system/app_abi_modulex_pack_tool"
$packToolBuild = Join-Path $OutDir "cmake-build-app-abi-modulex-pack-tool"
cmake -S $packToolSource -B $packToolBuild -G Ninja -DCMAKE_CXX_COMPILER="$Compiler"
cmake --build $packToolBuild

$packTool = Join-Path $packToolBuild "app-abi-modulex-pack.exe"
& $packTool $text $ro $modulex

Write-Host "[ok] modulex sample built at $modulex"
