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
$nm = "${ToolchainPrefix}nm"
$src = Join-Path $PSScriptRoot "modulex_hello_app.S"
$obj = Join-Path $OutDir "modulex_hello_app.o"
$text = Join-Path $OutDir "modulex_hello_app.text.bin"
$ro = Join-Path $OutDir "modulex_hello_app.rodata.bin"
$modulex = Join-Path $OutDir "modulex_hello_app.modulex"

& $cc -mcpu=cortex-m7 -mthumb -ffreestanding -fno-builtin -Os -c $src -o $obj
& $objcopy -O binary -j .text.charm_app_main $obj $text
& $objcopy -O binary -j .rodata.modulex_hello $obj $ro

function Get-ObjectSymbolOffset {
    param(
        [string]$ObjectPath,
        [string]$SymbolName,
        [switch]$ThumbFunction
    )

    $Lines = & $nm -S $ObjectPath
    foreach ($Line in $Lines) {
        $Parts = ($Line -split '\s+') | Where-Object { $_ -ne "" }
        if ($Parts.Count -lt 3) {
            continue
        }
        if ($Parts[-1] -ne $SymbolName) {
            continue
        }
        $Value = [Convert]::ToUInt32($Parts[0], 16)
        if ($ThumbFunction) {
            $Value = $Value -band 0xfffffffe
        }
        return $Value
    }
    throw "Object symbol not found: $SymbolName"
}

$entryOff = Get-ObjectSymbolOffset -ObjectPath $obj -SymbolName "charm_app_main" -ThumbFunction
$messagePtrOff = Get-ObjectSymbolOffset -ObjectPath $obj -SymbolName "modulex_message_ptr"
$messageOff = Get-ObjectSymbolOffset -ObjectPath $obj -SymbolName "modulex_message"
$textBytes = [System.IO.File]::ReadAllBytes($text)
$messageLen = $textBytes.Length - $messageOff
if ($messageLen -le 0) {
    throw "Invalid ModuleX message symbol offset: $messageOff of $($textBytes.Length)"
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../../..")
$packToolSource = Join-Path $repoRoot "Examples/system/app_abi_modulex_pack_tool"
$packToolBuild = Join-Path $OutDir "cmake-build-app-abi-modulex-pack-tool"
cmake -S $packToolSource -B $packToolBuild -G Ninja -DCMAKE_CXX_COMPILER="$Compiler"
cmake --build $packToolBuild

$packTool = Join-Path $packToolBuild "app-abi-modulex-pack.exe"
& $packTool $text $ro $modulex `
    --entry $entryOff `
    --global "charm_app_main:${entryOff}:$($textBytes.Length - $entryOff)" `
    --global "modulex_message:${messageOff}:$messageLen" `
    --reloc-abs "${messagePtrOff}:modulex_message:0"

Write-Host "[ok] modulex sample built at $modulex"
