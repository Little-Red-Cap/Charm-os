param(
    [string]$ArmGpp = "D:\Toolchains\Arm GNU Toolchain arm-none-eabi\latest\bin\arm-none-eabi-g++.exe",
    [string]$OutputRoot = "out/compiler-static-reflection-three-stage-probe",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Resolve-FullPath {
    param(
        [string]$Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Ensure-Directory {
    param(
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Invoke-Tool {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} (exit code {1})" -f $FailureMessage, $LASTEXITCODE)
    }
}

$armGppPath = Resolve-FullPath -Path $ArmGpp
$outputRootPath = Resolve-FullPath -Path $OutputRoot

if (-not (Test-Path -LiteralPath $armGppPath -PathType Leaf)) {
    throw "arm-none-eabi-g++ not found: $armGppPath"
}

if ($Clean -and (Test-Path -LiteralPath $outputRootPath)) {
    Remove-Item -LiteralPath $outputRootPath -Recurse -Force
}

$extractRoot = Join-Path $outputRootPath "extract"
$generatedRoot = Join-Path $outputRootPath "generated"
$firmwareRoot = Join-Path $outputRootPath "firmware"
Ensure-Directory -Path $extractRoot
Ensure-Directory -Path $generatedRoot
Ensure-Directory -Path $firmwareRoot

$extractSource = Join-Path $extractRoot "hosted_reflection_extract.cpp"
$extractObject = Join-Path $extractRoot "hosted_reflection_extract.o"
$generatedHeader = Join-Path $generatedRoot "charm_world_residue.hpp"
$firmwareSource = Join-Path $firmwareRoot "freestanding_consumer.cpp"
$firmwareObject = Join-Path $firmwareRoot "freestanding_consumer.o"

$extractCode = @'
#include <meta>

struct Uart1 {};
struct Pa9 {};
struct Pa10 {};

struct Uart1Binding {
    using resource = Uart1;
    using tx = Pa9;
    using rx = Pa10;
    static constexpr unsigned baud = 115200;
};

static_assert(std::meta::is_type(^^Uart1));
static_assert(std::meta::is_type(^^Uart1Binding));

extern "C" int charm_hosted_reflection_extract_probe() {
    return static_cast<int>(Uart1Binding::baud / 115200);
}
'@

$generatedCode = @'
#pragma once

namespace charm_generated_world {

enum class ResourceId : unsigned {
    Uart1 = 1,
};

enum class PinId : unsigned {
    Pa9 = 9,
    Pa10 = 10,
};

struct UartBinding {
    ResourceId resource;
    PinId tx;
    PinId rx;
    unsigned baud;
};

inline constexpr UartBinding uart1_binding{
    ResourceId::Uart1,
    PinId::Pa9,
    PinId::Pa10,
    115200,
};

static_assert(uart1_binding.baud == 115200);

} // namespace charm_generated_world
'@

$firmwareCode = @'
#include "charm_world_residue.hpp"

static_assert(charm_generated_world::uart1_binding.baud == 115200);

extern "C" unsigned charm_freestanding_world_residue_probe() {
    return charm_generated_world::uart1_binding.baud;
}
'@

Set-Content -LiteralPath $extractSource -Encoding utf8 -Value ($extractCode + [Environment]::NewLine)
Set-Content -LiteralPath $generatedHeader -Encoding utf8 -Value ($generatedCode + [Environment]::NewLine)
Set-Content -LiteralPath $firmwareSource -Encoding utf8 -Value ($firmwareCode + [Environment]::NewLine)

Write-Host "[COMPILER-STATIC-REFLECTION-THREE-STAGE-PROBE] hosted_reflection=building"
Invoke-Tool -Executable $armGppPath -Arguments @(
    "-std=c++26",
    "-freflection",
    "-mcpu=cortex-a7",
    "-marm",
    "-c",
    "-x",
    "c++",
    $extractSource,
    "-o",
    $extractObject
) -FailureMessage "hosted reflection extraction compile failed"

Write-Host "[COMPILER-STATIC-REFLECTION-THREE-STAGE-PROBE] freestanding_residue=building"
Invoke-Tool -Executable $armGppPath -Arguments @(
    "-std=c++26",
    "-mcpu=cortex-a7",
    "-marm",
    "-ffreestanding",
    "-fno-exceptions",
    "-fno-rtti",
    "-I",
    $generatedRoot,
    "-c",
    "-x",
    "c++",
    $firmwareSource,
    "-o",
    $firmwareObject
) -FailureMessage "freestanding residue compile failed"

if (-not (Test-Path -LiteralPath $extractObject -PathType Leaf)) {
    throw "missing hosted reflection object: $extractObject"
}

if (-not (Test-Path -LiteralPath $firmwareObject -PathType Leaf)) {
    throw "missing freestanding residue object: $firmwareObject"
}

Write-Host "[COMPILER-STATIC-REFLECTION-THREE-STAGE-PROBE] result=ok"
Write-Host ("extract_object={0}" -f $extractObject)
Write-Host ("generated_header={0}" -f $generatedHeader)
Write-Host ("firmware_object={0}" -f $firmwareObject)
