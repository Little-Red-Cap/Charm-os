param(
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$OutDir = "$PSScriptRoot/out"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$cc = "${ToolchainPrefix}gcc"
$ldscript = Join-Path $PSScriptRoot "elf_samples.ld"
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
    "-nostartfiles"
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
    "hello",
    "argv_dump",
    "env_dump",
    "stderr_demo",
    "exit_code",
    "getpid",
    "sleep",
    "kill_self",
    "cat_file",
    "write_file",
    "append_file",
    "fd_probe",
    "stat_probe"
)

foreach ($name in $samples) {
    $src = Join-Path $PSScriptRoot "$name.c"
    $elf = Join-Path $OutDir "$name.elf"
    $inc = Join-Path $PSScriptRoot "$name.elf.inc"
    & $cc @cflags $src -o $elf @ldflags
    Write-IncFile -InputPath $elf -OutputPath $inc -Symbol "${name}_elf"
}

Write-Host "[ok] elf samples built at $OutDir"
