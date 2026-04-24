param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [ValidateSet("f103", "g431", "h503")]
    [string]$Port = "h503",
    [ValidateSet("cmsis-dap", "stlink")]
    [string]$Probe = "cmsis-dap",
    [string]$BuildDir = "",
    [string]$Elf = "",
    [string]$OpenOcd = "",
    [string]$OpenOcdScripts = "",
    [switch]$PrintOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-FirstExistingPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Get-PortDefaults {
    param(
        [string]$ProjectRoot,
        [string]$PortName,
        [string]$ProbeName
    )

    switch ("$PortName/$ProbeName") {
        "f103/cmsis-dap" {
            return @{
                BuildDirCandidates = @(
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-daplink-f103-debug"),
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-f103-debug")
                )
                BoardCfg = Join-Path $ProjectRoot "Examples\project\daplink\f103\stm32f103c8_devboard_cmsis_dap.cfg"
            }
        }
        "g431/cmsis-dap" {
            return @{
                BuildDirCandidates = @(
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-daplink-g431-debug"),
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-g431-debug")
                )
                BoardCfg = Join-Path $ProjectRoot "Examples\project\daplink\g431\stm32g431cb_devboard_cmsis_dap.cfg"
            }
        }
        "h503/cmsis-dap" {
            return @{
                BuildDirCandidates = @(
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-daplink-h503-debug"),
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-h503-debug")
                )
                BoardCfg = Join-Path $ProjectRoot "Examples\project\daplink\h503\stm32h503cb_devboard_cmsis_dap.cfg"
            }
        }
        "h503/stlink" {
            return @{
                BuildDirCandidates = @(
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-daplink-h503-debug"),
                    (Join-Path $ProjectRoot "Examples\project\daplink\cmake-build-h503-debug")
                )
                BoardCfg = Join-Path $ProjectRoot "Examples\project\daplink\h503\stm32h503cb_devboard_stlink.cfg"
            }
        }
        default {
            throw "Unsupported DAPLink port/probe combo: $PortName / $ProbeName"
        }
    }
}

function Get-OpenOcdBundle {
    param(
        [string]$PortName,
        [string]$OpenOcdExe,
        [string]$ScriptsDir
    )

    if (($OpenOcdExe -and -not $ScriptsDir) -or (-not $OpenOcdExe -and $ScriptsDir)) {
        throw "OpenOcd and OpenOcdScripts must be provided together."
    }

    if ($OpenOcdExe -and $ScriptsDir) {
        if (-not (Test-Path -LiteralPath $OpenOcdExe)) {
            throw "OpenOCD executable not found: $OpenOcdExe"
        }
        if (-not (Test-Path -LiteralPath $ScriptsDir)) {
            throw "OpenOCD scripts directory not found: $ScriptsDir"
        }
        return @{
            Exe = (Resolve-Path -LiteralPath $OpenOcdExe).Path
            Scripts = (Resolve-Path -LiteralPath $ScriptsDir).Path
        }
    }

    $toolchainExeCandidates = @(
        "D:\Toolchains\OpenOCD\bin\openocd.exe"
    )
    $toolchainScriptsCandidates = @(
        "D:\Toolchains\OpenOCD\share\openocd\scripts"
    )
    $stExeCandidates = @(
        "D:\ST\STM32CubeIDE\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.3.0.202305091550\tools\bin\openocd.exe",
        "D:\ST\STM32CubeIDE\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.2.300.202301161003\tools\bin\openocd.exe"
    )
    $stScriptsCandidates = @(
        "D:\ST\STM32CubeIDE\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.1.0.202306221132\resources\openocd\st_scripts",
        "D:\ST\STM32CubeIDE\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.0.500.202302091318\resources\openocd\st_scripts"
    )

    if ($PortName -eq "h503") {
        $exe = Get-FirstExistingPath -Candidates $stExeCandidates
        $scripts = Get-FirstExistingPath -Candidates $stScriptsCandidates
    } else {
        $exe = Get-FirstExistingPath -Candidates ($toolchainExeCandidates + $stExeCandidates)
        $scripts = Get-FirstExistingPath -Candidates ($toolchainScriptsCandidates + $stScriptsCandidates)
    }

    if (-not $exe) {
        throw "Unable to find a usable OpenOCD executable."
    }
    if (-not $scripts) {
        throw "Unable to find a usable OpenOCD scripts directory."
    }

    return @{
        Exe = $exe
        Scripts = $scripts
    }
}

function Convert-ToOpenOcdPath {
    param(
        [string]$Path
    )

    return ($Path -replace "\\", "/")
}

$rootPath = (Resolve-Path -LiteralPath $Root).Path
$defaults = Get-PortDefaults -ProjectRoot $rootPath -PortName $Port -ProbeName $Probe
$defaultBuildDir = Get-FirstExistingPath -Candidates $defaults.BuildDirCandidates
if (-not $defaultBuildDir) {
    $defaultBuildDir = $defaults.BuildDirCandidates[0]
}
$buildDirPath = if ($BuildDir) { (Resolve-Path -LiteralPath $BuildDir).Path } else { $defaultBuildDir }
$elfPath = if ($Elf) { (Resolve-Path -LiteralPath $Elf).Path } else { Join-Path $buildDirPath "daplink.elf" }
$boardCfgPath = (Resolve-Path -LiteralPath $defaults.BoardCfg).Path
$openOcdBundle = Get-OpenOcdBundle -PortName $Port -OpenOcdExe $OpenOcd -ScriptsDir $OpenOcdScripts

if (-not (Test-Path -LiteralPath $boardCfgPath)) {
    throw "Board config not found: $boardCfgPath"
}
if (-not (Test-Path -LiteralPath $elfPath)) {
    throw "ELF not found: $elfPath"
}

$openOcdElfPath = Convert-ToOpenOcdPath -Path $elfPath
$openOcdArgs = @(
    "-s", $openOcdBundle.Scripts,
    "-f", $boardCfgPath,
    "-c", "tcl_port disabled",
    "-c", "gdb_port disabled",
    "-c", "program {$openOcdElfPath} verify reset exit"
)

Write-Host "[daplink_flash] port      : $Port"
Write-Host "[daplink_flash] probe     : $Probe"
Write-Host "[daplink_flash] build dir : $buildDirPath"
Write-Host "[daplink_flash] elf       : $elfPath"
Write-Host "[daplink_flash] openocd   : $($openOcdBundle.Exe)"
Write-Host "[daplink_flash] scripts   : $($openOcdBundle.Scripts)"
Write-Host "[daplink_flash] board cfg : $boardCfgPath"

if ($PrintOnly) {
    Write-Host "[daplink_flash] command:"
    Write-Host "& '$($openOcdBundle.Exe)' ``"
    Write-Host "  -s '$($openOcdBundle.Scripts)' ``"
    Write-Host "  -f '$boardCfgPath' ``"
    Write-Host "  -c `"tcl_port disabled`" ``"
    Write-Host "  -c `"gdb_port disabled`" ``"
    Write-Host "  -c `"program {$openOcdElfPath} verify reset exit`""
    exit 0
}

& $openOcdBundle.Exe @openOcdArgs
if ($LASTEXITCODE -ne 0) {
    throw "OpenOCD failed with exit code $LASTEXITCODE"
}

Write-Host "[daplink_flash] done"
