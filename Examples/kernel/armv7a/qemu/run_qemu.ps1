param(
    [string]$QemuExe = "qemu-system-arm",
    [string]$ElfPath = "out\\build\\debug\\charm-armv7a-qemu",
    [switch]$WaitForGdb,
    [int]$GdbPort = 1234
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param([string]$Tool)

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Resolve-ExamplePath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path $Path).Path
    }

    return (Resolve-Path (Join-Path $PSScriptRoot $Path)).Path
}

$qemu = Resolve-ToolPath -Tool $QemuExe
$elf = Resolve-ExamplePath -Path $ElfPath

$args = @(
    "-machine", "virt",
    "-cpu", "cortex-a7",
    "-nographic",
    "-monitor", "none",
    "-device", "loader,file=$elf,cpu-num=0"
)

if ($WaitForGdb) {
    $args += @(
        "-S",
        "-gdb", "tcp::$GdbPort"
    )
}

& $qemu @args
exit $LASTEXITCODE
