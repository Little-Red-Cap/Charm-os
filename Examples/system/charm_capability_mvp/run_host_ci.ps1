[CmdletBinding()]
param(
    [ValidateSet('clang-debug', 'gcc-debug', 'clang-sanitize', 'all')]
    [string]$Profile = 'clang-debug',
    [string]$BuildDir = '',
    [switch]$DryRun,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceDir = $scriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $scriptRoot 'cmake-build-charm-capability-mvp'
}
$buildDirFull = [System.IO.Path]::GetFullPath($BuildDir)

function Get-ProfileSpec {
    param([string]$Name)

    switch ($Name) {
        'clang-debug' {
            return [pscustomobject]@{
                Name = $Name
                CompilerCommand = 'clang++'
                BuildType = 'Debug'
                CxxFlags = ''
                LinkerFlags = ''
                RuntimeLibrary = ''
            }
        }
        'gcc-debug' {
            return [pscustomobject]@{
                Name = $Name
                CompilerCommand = 'g++'
                BuildType = 'Debug'
                CxxFlags = ''
                LinkerFlags = ''
                RuntimeLibrary = ''
            }
        }
        'clang-sanitize' {
            return [pscustomobject]@{
                Name = $Name
                CompilerCommand = 'clang++'
                BuildType = 'Release'
                CxxFlags = '-fsanitize=address,undefined -fno-omit-frame-pointer'
                LinkerFlags = '-fsanitize=address,undefined'
                RuntimeLibrary = 'MultiThreaded'
            }
        }
        default {
            throw "unknown_profile: $Name"
        }
    }
}

function Get-ProfileSequence {
    param([string]$RequestedProfile)

    if ($RequestedProfile -eq 'all') {
        # Restore the small canonical Debug build after the high-overhead sanitizer pass.
        return @('clang-debug', 'gcc-debug', 'clang-sanitize', 'clang-debug')
    }
    return @($RequestedProfile)
}

function Resolve-Compiler {
    param([string]$CommandName)

    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "compiler_missing: $CommandName"
    }
    return $command.Source
}

function Invoke-Profile {
    param([object]$Spec)

    $compiler = Resolve-Compiler -CommandName $Spec.CompilerCommand
    $configureArguments = @(
        '--fresh',
        '-S', $sourceDir,
        '-B', $buildDirFull,
        '-G', 'Ninja',
        "-DCMAKE_BUILD_TYPE=$($Spec.BuildType)",
        "-DCMAKE_CXX_COMPILER=$compiler",
        "-DCMAKE_CXX_FLAGS=$($Spec.CxxFlags)",
        "-DCMAKE_EXE_LINKER_FLAGS=$($Spec.LinkerFlags)"
    )
    if (-not [string]::IsNullOrWhiteSpace($Spec.RuntimeLibrary)) {
        $configureArguments += "-DCMAKE_MSVC_RUNTIME_LIBRARY=$($Spec.RuntimeLibrary)"
    }

    Write-Output "[charm-capability-mvp-host-ci] profile=$($Spec.Name) build_dir=$buildDirFull"
    if ($DryRun) {
        Write-Output "  compiler=$compiler"
        Write-Output "  build_type=$($Spec.BuildType)"
        return
    }

    & cmake @configureArguments
    if ($LASTEXITCODE -ne 0) {
        throw "configure_failed: profile=$($Spec.Name) exit=$LASTEXITCODE"
    }
    & cmake --build $buildDirFull
    if ($LASTEXITCODE -ne 0) {
        throw "build_failed: profile=$($Spec.Name) exit=$LASTEXITCODE"
    }
    & ctest --test-dir $buildDirFull --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "ctest_failed: profile=$($Spec.Name) exit=$LASTEXITCODE"
    }
    Write-Output "[charm-capability-mvp-host-ci] profile=$($Spec.Name) status=ok"
}

if ($SelfTest) {
    $allProfiles = Get-ProfileSequence -RequestedProfile 'all'
    if (($allProfiles -join ',') -ne 'clang-debug,gcc-debug,clang-sanitize,clang-debug') {
        throw 'self_test_failed: profile_sequence'
    }
    foreach ($name in @('clang-debug', 'gcc-debug', 'clang-sanitize')) {
        $spec = Get-ProfileSpec -Name $name
        if ($spec.Name -ne $name) {
            throw "self_test_failed: profile_spec=$name"
        }
    }
    Write-Output '[charm-capability-mvp-host-ci-self-test] ok'
    exit 0
}

foreach ($name in (Get-ProfileSequence -RequestedProfile $Profile)) {
    Invoke-Profile -Spec (Get-ProfileSpec -Name $name)
}

if (-not $DryRun) {
    $size = (Get-ChildItem -LiteralPath $buildDirFull -File -Recurse -Force |
        Measure-Object -Property Length -Sum).Sum
    Write-Output ('[charm-capability-mvp-host-ci] ok profile={0} build_mib={1:N1}' -f
        $Profile, ($size / 1MB))
}
