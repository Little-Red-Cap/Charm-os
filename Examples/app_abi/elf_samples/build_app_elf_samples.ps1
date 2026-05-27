param(
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$OutDir = "$PSScriptRoot/out",
    [string]$IncDir = "$PSScriptRoot",
    [string]$ElfBase = "0x24070000",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$PackToolBuildDir = "",
    [string]$StorePath = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}
if (-not (Test-Path $IncDir)) {
    New-Item -ItemType Directory -Path $IncDir | Out-Null
}
if ([string]::IsNullOrWhiteSpace($StorePath)) {
    $StorePath = Join-Path $OutDir "appstore.bin"
}
if ([string]::IsNullOrWhiteSpace($PackToolBuildDir)) {
    $PackToolBuildDir = Join-Path $OutDir "cmake-build-app-abi-store-pack-tool"
}

$cc = "${ToolchainPrefix}gcc"
$ldscriptTemplate = Join-Path $PSScriptRoot "app_elf.ld"
$ldscript = Join-Path $OutDir "app_elf.generated.ld"
$ldtext = Get-Content -Path $ldscriptTemplate -Raw -Encoding UTF8
$ldtext = $ldtext -replace 'ELF_BASE = 0x[0-9A-Fa-f]+;', "ELF_BASE = $ElfBase;"
[System.IO.File]::WriteAllText($ldscript, $ldtext, [System.Text.Encoding]::ASCII)

$includeRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$cflags = @(
    "-mcpu=cortex-m7",
    "-mthumb",
    "-ffreestanding",
    "-fno-builtin",
    "-fno-pic",
    "-fno-pie",
    "-fdata-sections",
    "-ffunction-sections",
    "-Os",
    "-nostdlib",
    "-nostartfiles",
    "-I$includeRoot"
)
$ldflags = @(
    "-Wl,--gc-sections",
    "-Wl,-T$ldscript"
)

function Write-IncFile {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [string]$Symbol
    )
    $bytes = [System.IO.File]::ReadAllBytes($InputPath)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("static const unsigned char $Symbol[] = {`n")
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        if (($i % 12) -eq 0) { [void]$sb.Append("    ") }
        [void]$sb.AppendFormat("0x{0:X2}, ", $bytes[$i])
        if (($i % 12) -eq 11) { [void]$sb.Append("`n") }
    }
    if (($bytes.Length % 12) -ne 0) { [void]$sb.Append("`n") }
    [void]$sb.Append("};`n")
    [void]$sb.Append("static const unsigned int ${Symbol}_len = $($bytes.Length);`n")
    [System.IO.File]::WriteAllText($OutputPath, $sb.ToString(), [System.Text.Encoding]::ASCII)
}

$samples = @(
    "hello_app",
    "player_min"
)

foreach ($name in $samples) {
    $src = Join-Path $PSScriptRoot "$name.c"
    $elf = Join-Path $OutDir "$name.elf"
    $inc = Join-Path $IncDir "$name.elf.inc"
    & $cc @cflags $src -o $elf @ldflags
    Write-IncFile -InputPath $elf -OutputPath $inc -Symbol "${name}_elf"
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../../..")
$packToolSource = Join-Path $repoRoot "Examples/system/app_abi_store_pack_tool"
cmake -S $packToolSource -B $PackToolBuildDir -G Ninja -DCMAKE_CXX_COMPILER="$HostCompiler"
cmake --build $PackToolBuildDir

$packTool = Join-Path $PackToolBuildDir "app-abi-store-pack.exe"
$hello = Join-Path $OutDir "hello_app.elf"
$player = Join-Path $OutDir "player_min.elf"
& $packTool $StorePath "hello_app=$hello" "player_min=$player"
Write-IncFile -InputPath $StorePath -OutputPath (Join-Path $IncDir "appstore.bin.inc") -Symbol "appstore_bin"

Write-Host "[ok] app elf samples and app store built at $OutDir"
