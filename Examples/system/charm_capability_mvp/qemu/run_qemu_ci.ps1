[CmdletBinding()]
param(
    [string]$QemuExe = 'D:\Toolchains\qemu\qemu-system-arm.exe',
    [string]$BuildDir = (Join-Path $PSScriptRoot 'cmake-build-charm-capability-mvp-qemu'),
    [int]$TimeoutSec = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()

if (-not (Test-Path -LiteralPath $QemuExe -PathType Leaf)) {
    throw "qemu_missing: $QemuExe"
}
if ($TimeoutSec -lt 1) {
    throw 'invalid_timeout'
}

$sourceDir = $PSScriptRoot
$toolchain = Join-Path $sourceDir 'arm-none-eabi-m7.cmake'
$cache = Join-Path $BuildDir 'CMakeCache.txt'
$elf = Join-Path $BuildDir 'charm-capability-mvp-qemu.elf'
$stdoutLog = Join-Path $BuildDir 'qemu-ci.log'
$stderrLog = Join-Path $BuildDir 'qemu-ci.err.log'

$configureArguments = @('-S', $sourceDir, '-B', $BuildDir, '-G', 'Ninja')
if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) {
    $configureArguments += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
} else {
    $cacheText = Get-Content -Raw -Encoding utf8 $cache
    if (-not $cacheText.Contains('arm-none-eabi-m7.cmake')) {
        throw "toolchain_mismatch: $cache"
    }
}

& cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "configure_failed: $LASTEXITCODE"
}
& cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) {
    throw "build_failed: $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $elf -PathType Leaf)) {
    throw "firmware_missing: $elf"
}

foreach ($path in @($stdoutLog, $stderrLog)) {
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
    }
}

$arguments = @(
    '-M', 'mps2-an500',
    '-cpu', 'cortex-m7',
    '-nographic',
    '-kernel', $elf
)

$process = $null
try {
    $process = Start-Process -FilePath $QemuExe `
        -ArgumentList $arguments `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -WindowStyle Hidden `
        -PassThru
    Start-Sleep -Seconds $TimeoutSec
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}

$text = ''
if (Test-Path -LiteralPath $stdoutLog) {
    $text += Get-Content -Raw -Encoding utf8 $stdoutLog
}
if (Test-Path -LiteralPath $stderrLog) {
    $text += Get-Content -Raw -Encoding utf8 $stderrLog
}

$requiredTokens = @(
    'charm-mvp: ok',
    '[charm-capability-mvp-qemu] positive=ok timestamp=424242 checksum=0x49b880f0',
    '[charm-capability-mvp-qemu] missing=missing_binding start_count=0',
    '[charm-capability-mvp-qemu] duplicate=duplicate_binding start_count=0',
    '[charm-capability-mvp-qemu] invalid_index=invalid_provision_index start_count=0',
    '[charm-capability-mvp-qemu] mismatch=contract_mismatch start_count=0',
    '[charm-capability-mvp-qemu] invalid=invalid_provision start_count=0',
    '[charm-capability-mvp-qemu] app_failure_cases=12 failures=0',
    '[charm-capability-mvp-qemu] ok'
)
foreach ($token in $requiredTokens) {
    if (-not $text.Contains($token)) {
        throw "token_missing: $token`n$text"
    }
}
if ($text.Contains('[charm-capability-mvp-qemu] failed') -or
    $text.Contains('[charm-capability-mvp-qemu] error=')) {
    throw "qemu_runtime_failed`n$text"
}

Write-Output '[charm-capability-mvp-qemu-ci] ok'
