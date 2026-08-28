[CmdletBinding()]
param(
    [string]$BuildDir = '',
    [int]$TimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $scriptRoot 'cmake-build-charm-capability-mvp-qemu'
}
$buildDirFull = [IO.Path]::GetFullPath($BuildDir)

function Resolve-Tool([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "tool_missing: $Name"
    }
    return $command.Source
}

$cxx = Resolve-Tool -Name 'arm-none-eabi-g++'
$asm = Resolve-Tool -Name 'arm-none-eabi-gcc'
$qemu = Resolve-Tool -Name 'qemu-system-arm'

& cmake --fresh -S $scriptRoot -B $buildDirFull -G Ninja `
    "-DCMAKE_SYSTEM_NAME=Generic" `
    "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY" `
    "-DCMAKE_CXX_COMPILER=$cxx" `
    "-DCMAKE_ASM_COMPILER=$asm"
if ($LASTEXITCODE -ne 0) {
    throw "configure_failed: exit=$LASTEXITCODE"
}
& cmake --build $buildDirFull --target charm-capability-mvp-qemu
if ($LASTEXITCODE -ne 0) {
    throw "build_failed: exit=$LASTEXITCODE"
}

$firmware = Join-Path $buildDirFull 'charm-capability-mvp-qemu.elf'
if (-not (Test-Path -LiteralPath $firmware -PathType Leaf)) {
    throw "firmware_missing: $firmware"
}

$startInfo = [Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $qemu
$startInfo.Arguments = '-M virt -cpu cortex-a15 -nographic ' +
    '-semihosting-config enable=on,target=native -kernel "' + $firmware + '"'
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

$process = [Diagnostics.Process]::new()
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw 'qemu_start_failed'
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    $process.Kill()
    throw "qemu_timeout: seconds=$TimeoutSeconds"
}
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
$output = $stdout + $stderr
Write-Output $output.TrimEnd()
if ($process.ExitCode -ne 0) {
    throw "qemu_failed: exit=$($process.ExitCode)"
}

$expected = @(
    'charm-mvp: ok',
    '[charm-capability-mvp-qemu] positive=ok timestamp=424242 checksum=0x49b880f0',
    '[charm-capability-mvp-qemu] missing=missing_binding start_count=0',
    '[charm-capability-mvp-qemu] ok'
)
foreach ($token in $expected) {
    if (-not $output.Contains($token)) {
        throw "qemu_evidence_missing: $token"
    }
}
Write-Output '[charm-capability-mvp-qemu-ci] ok'
