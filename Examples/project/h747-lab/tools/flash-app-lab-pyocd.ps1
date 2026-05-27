param(
    [string]$Image = "",
    [string]$Bin = "",
    [string]$Elf = "",
    [string]$BaseAddress = "0x08000000",
    [ValidateSet("auto", "bin", "elf")]
    [string]$Format = "auto",
    [string]$Probe = "0001",
    [string]$Target = "stm32h747xihx",
    [string]$Frequency = "1000k",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$DefaultBin = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_app_lab.bin"
$DefaultElf = Join-Path $ProjectRoot "cmake-build-h747-lab-debug\h747_lab_app_lab.elf"

function Test-NonEmptyString {
    param([string]$Value)
    return -not [string]::IsNullOrWhiteSpace($Value)
}

function Get-ImageSelection {
    $Specified = New-Object System.Collections.Generic.List[string]
    if (Test-NonEmptyString $Image) {
        [void]$Specified.Add("Image")
    }
    if (Test-NonEmptyString $Bin) {
        [void]$Specified.Add("Bin")
    }
    if (Test-NonEmptyString $Elf) {
        [void]$Specified.Add("Elf")
    }

    if ($Specified.Count -gt 1) {
        throw "Specify only one of -Image, -Bin, or -Elf."
    }

    $RequestedFormat = $Format.ToLowerInvariant()
    if ($Specified.Count -eq 0) {
        if ($RequestedFormat -eq "elf") {
            return @{
                Path = $DefaultElf
                Format = "elf"
                Source = "default-elf"
            }
        }
        return @{
            Path = $DefaultBin
            Format = "bin"
            Source = "default-bin"
        }
    }

    if (Test-NonEmptyString $Bin) {
        if ($RequestedFormat -eq "elf") {
            throw "-Bin cannot be used with -Format elf."
        }
        return @{
            Path = $Bin
            Format = "bin"
            Source = "bin"
        }
    }

    if (Test-NonEmptyString $Elf) {
        if ($RequestedFormat -eq "bin") {
            throw "-Elf cannot be used with -Format bin."
        }
        return @{
            Path = $Elf
            Format = "elf"
            Source = "elf"
        }
    }

    if ($RequestedFormat -ne "auto") {
        return @{
            Path = $Image
            Format = $RequestedFormat
            Source = "image"
        }
    }

    $Extension = [System.IO.Path]::GetExtension($Image).ToLowerInvariant()
    if ($Extension -eq ".bin") {
        return @{
            Path = $Image
            Format = "bin"
            Source = "image"
        }
    }
    if ($Extension -eq ".elf") {
        return @{
            Path = $Image
            Format = "elf"
            Source = "image"
        }
    }
    throw "Cannot infer image format from '$Image'. Use -Format bin or -Format elf."
}

$Selection = Get-ImageSelection

if ([string]::IsNullOrWhiteSpace($BaseAddress)) {
    throw "BaseAddress must not be empty."
}

if (-not (Test-Path -LiteralPath $Selection.Path)) {
    throw "Image not found: $($Selection.Path)"
}

$ResolvedImage = (Resolve-Path -LiteralPath $Selection.Path).Path
$LoadArgs = @(
    "load",
    "-u", $Probe,
    "-t", $Target,
    "-f", $Frequency
)

if ($Selection.Format -eq "bin") {
    $LoadArgs += @("--format", "bin", "-a", $BaseAddress, $ResolvedImage)
} else {
    $LoadArgs += @("--format", "elf", $ResolvedImage)
}

$PyOcdSource = "pyocd"
if (-not $DryRun) {
    $PyOcd = Get-Command pyocd -ErrorAction Stop
    $PyOcdSource = $PyOcd.Source
}

Write-Host "Flashing H747 Lab App Lab"
Write-Host "  pyocd:     $PyOcdSource"
Write-Host "  probe:     $Probe"
Write-Host "  target:    $Target"
Write-Host "  frequency: $Frequency"
Write-Host "  image:     $ResolvedImage"
Write-Host "  source:    $($Selection.Source)"
Write-Host "  format:    $($Selection.Format)"
if ($Selection.Format -eq "bin") {
    Write-Host "  address:   $BaseAddress"
}
Write-Host ""
if ($DryRun) {
    Write-Host "Dry run: pyOCD was not invoked."
    Write-Host "Command:"
    Write-Host "  pyocd $($LoadArgs -join ' ')"
    exit 0
}

Write-Host "Note: pyOCD may print 'Exception reading AP#2 IDR: Memory transfer fault' on this board."
Write-Host "Treat the flash as successful only when pyOCD exits with 0 and prints the final erase/program summary."
Write-Host ""

& $PyOcd.Source @LoadArgs

if ($LASTEXITCODE -ne 0) {
    throw "pyocd load failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Flash command completed. Serial smoke: COM16, 115200 8N1."
