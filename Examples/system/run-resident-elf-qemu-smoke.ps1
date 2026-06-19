param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$BuildDir = "",
    [string]$AppOutDir = "",
    [string]$FrameSignatureOut = "",
    [string]$GoldenFrameSignatures = "",
    [string]$FrameDumpOut = "",
    [string]$FramePpmOut = "",
    [string]$InputTraceOut = "",
    [string]$GoldenInputTrace = "",
    [string]$StorageTraceOut = "",
    [string]$GoldenStorageTrace = "",
    [string]$DomainSummaryOut = "",
    [string]$GoldenDomainSummary = "",
    [string]$ElfBase = "0x20080000",
    [int]$TimeoutSec = 15,
    [int]$TailLines = 80,
    [string]$ValidateLog = "",
    [string]$ValidateFrameSignatures = "",
    [string]$CompareFrameSignatures = "",
    [string]$ActualFrameSignatures = "",
    [string]$ValidateFrameDumps = "",
    [string]$ValidateFramePpm = "",
    [string]$ValidateInputTrace = "",
    [string]$CompareInputTrace = "",
    [string]$ActualInputTrace = "",
    [string]$ValidateStorageTrace = "",
    [string]$CompareStorageTrace = "",
    [string]$ActualStorageTrace = "",
    [string]$ValidateDomainSummary = "",
    [string]$CompareDomainSummary = "",
    [string]$ActualDomainSummary = "",
    [string]$CompareFrameDumps = "",
    [string]$ActualFrameDumps = "",
    [switch]$SkipGoldenFrameSignatures,
    [switch]$SkipGoldenInputTrace,
    [switch]$SkipGoldenStorageTrace,
    [switch]$SkipGoldenDomainSummary,
    [switch]$ValidateEvidenceBundle,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$Script = Join-Path $PSScriptRoot "resident_elf_qemu_smoke\run_qemu_ci.ps1"
if (-not (Test-Path -LiteralPath $Script)) {
    throw "missing_smoke: $Script"
}

function Add-OptionalStringArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [string]$Value
    )

    if (-not [string]::IsNullOrWhiteSpace($Value)) {
        [void]$Arguments.Add($Name)
        [void]$Arguments.Add($Value)
    }
}

function Add-OptionalSwitchArgument {
    param(
        [System.Collections.Generic.List[string]]$Arguments,
        [string]$Name,
        [bool]$Enabled
    )

    if ($Enabled) {
        [void]$Arguments.Add($Name)
    }
}

function New-QemuSmokeArguments {
    param([bool]$ForceSelfTest = $false)

    $Arguments = New-Object System.Collections.Generic.List[string]
    foreach ($Argument in @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $Script,
        "-CMakeExe",
        $CMakeExe,
        "-QemuExe",
        $QemuExe,
        "-ToolchainPrefix",
        $ToolchainPrefix,
        "-HostCompiler",
        $HostCompiler,
        "-ElfBase",
        $ElfBase,
        "-TimeoutSec",
        ([string]$TimeoutSec),
        "-TailLines",
        ([string]$TailLines)
    )) {
        [void]$Arguments.Add($Argument)
    }

    Add-OptionalStringArgument -Arguments $Arguments -Name "-BuildDir" -Value $BuildDir
    Add-OptionalStringArgument -Arguments $Arguments -Name "-AppOutDir" -Value $AppOutDir
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FrameSignatureOut" -Value $FrameSignatureOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenFrameSignatures" -Value $GoldenFrameSignatures
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FrameDumpOut" -Value $FrameDumpOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FramePpmOut" -Value $FramePpmOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-InputTraceOut" -Value $InputTraceOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenInputTrace" -Value $GoldenInputTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-StorageTraceOut" -Value $StorageTraceOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenStorageTrace" -Value $GoldenStorageTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-DomainSummaryOut" -Value $DomainSummaryOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenDomainSummary" -Value $GoldenDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateLog" -Value $ValidateLog
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateFrameSignatures" -Value $ValidateFrameSignatures
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareFrameSignatures" -Value $CompareFrameSignatures
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualFrameSignatures" -Value $ActualFrameSignatures
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateFrameDumps" -Value $ValidateFrameDumps
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateFramePpm" -Value $ValidateFramePpm
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateInputTrace" -Value $ValidateInputTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareInputTrace" -Value $CompareInputTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualInputTrace" -Value $ActualInputTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateStorageTrace" -Value $ValidateStorageTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareStorageTrace" -Value $CompareStorageTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualStorageTrace" -Value $ActualStorageTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateDomainSummary" -Value $ValidateDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareDomainSummary" -Value $CompareDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualDomainSummary" -Value $ActualDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareFrameDumps" -Value $CompareFrameDumps
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualFrameDumps" -Value $ActualFrameDumps
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenFrameSignatures" -Enabled $SkipGoldenFrameSignatures.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenInputTrace" -Enabled $SkipGoldenInputTrace.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenStorageTrace" -Enabled $SkipGoldenStorageTrace.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenDomainSummary" -Enabled $SkipGoldenDomainSummary.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-ValidateEvidenceBundle" -Enabled $ValidateEvidenceBundle.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-DryRun" -Enabled $DryRun.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SelfTest" -Enabled ($SelfTest.IsPresent -or $ForceSelfTest)

    return ,$Arguments.ToArray()
}

function Test-ArgumentPresent {
    param(
        [string[]]$Arguments,
        [string]$Name
    )

    return ($Arguments -contains $Name)
}

function Test-ArgumentValue {
    param(
        [string[]]$Arguments,
        [string]$Name,
        [string]$Expected
    )

    for ($i = 0; $i -lt ($Arguments.Count - 1); ++$i) {
        if ($Arguments[$i] -eq $Name) {
            return ($Arguments[$i + 1] -eq $Expected)
        }
    }
    return $false
}

function Invoke-WrapperSelfTest {
    if (-not (Test-Path -LiteralPath $Script)) {
        throw "selftest_failed: resident ELF QEMU script is missing: $Script"
    }
    foreach ($Required in @(
        (Join-Path $PSScriptRoot "resident_elf_qemu_smoke\CMakeLists.txt"),
        (Join-Path $PSScriptRoot "resident_elf_qemu_smoke\README.md")
    )) {
        if (-not (Test-Path -LiteralPath $Required)) {
            throw "selftest_failed: required QEMU smoke file is missing: $Required"
        }
    }
    $EvidenceBundle = Join-Path $PSScriptRoot "..\project\h747-lab\tools\capture-resident-platform-evidence-bundle.ps1"
    if (-not (Test-Path -LiteralPath $EvidenceBundle)) {
        throw "selftest_failed: resident platform evidence bundle is missing: $EvidenceBundle"
    }
    $EvidenceBundleText = Get-Content -LiteralPath $EvidenceBundle -Raw -Encoding UTF8
    foreach ($RequiredBundleToken in @(
        "[switch]`$QemuElf",
        "[switch]`$QemuElfValidateOnly",
        "[int]`$QemuElfTimeoutSec",
        "[int]`$QemuElfTailLines",
        "qemu_elf_timeout_sec=",
        "qemu_elf_tail_lines=",
        '"-TimeoutSec"',
        '"-TailLines"'
    )) {
        if (-not $EvidenceBundleText.Contains($RequiredBundleToken)) {
            throw "selftest_failed: evidence bundle does not expose QEMU wrapper token $RequiredBundleToken"
        }
    }

    $Forwarded = New-QemuSmokeArguments -ForceSelfTest $true
    foreach ($RequiredArg in @("-File", $Script, "-CMakeExe", "-QemuExe", "-ToolchainPrefix", "-HostCompiler", "-ElfBase", "-SelfTest")) {
        if (-not (Test-ArgumentPresent -Arguments $Forwarded -Name $RequiredArg)) {
            throw "selftest_failed: wrapper did not forward $RequiredArg"
        }
    }
    foreach ($ExpectedForward in @(
        @{ Name = "-TimeoutSec"; Value = "15" },
        @{ Name = "-TailLines"; Value = "80" },
        @{ Name = "-ElfBase"; Value = "0x20080000" }
    )) {
        if (-not (Test-ArgumentValue -Arguments $Forwarded -Name $ExpectedForward.Name -Expected $ExpectedForward.Value)) {
            throw "selftest_failed: wrapper did not forward $($ExpectedForward.Name)=$($ExpectedForward.Value)"
        }
    }
    if ($SkipGoldenFrameSignatures -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenFrameSignatures")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenFrameSignatures"
    }
    if ($SkipGoldenInputTrace -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenInputTrace")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenInputTrace"
    }
    if ($SkipGoldenStorageTrace -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenStorageTrace")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenStorageTrace"
    }
    if ($SkipGoldenDomainSummary -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenDomainSummary")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenDomainSummary"
    }
    if ($ValidateEvidenceBundle -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-ValidateEvidenceBundle")) {
        throw "selftest_failed: wrapper did not forward -ValidateEvidenceBundle"
    }

    $OriginalValidateFrameSignatures = $ValidateFrameSignatures
    $OriginalCompareFrameSignatures = $CompareFrameSignatures
    $OriginalActualFrameSignatures = $ActualFrameSignatures
    $OriginalValidateDomainSummary = $ValidateDomainSummary
    $OriginalCompareDomainSummary = $CompareDomainSummary
    $OriginalActualDomainSummary = $ActualDomainSummary
    $script:ValidateFrameSignatures = "frame-signatures.json"
    $script:CompareFrameSignatures = "frame-signatures.golden.json"
    $script:ActualFrameSignatures = "frame-signatures.json"
    $script:ValidateDomainSummary = "domain-summary.json"
    $script:CompareDomainSummary = "domain-summary.golden.json"
    $script:ActualDomainSummary = "domain-summary.json"
    try {
        $ProbeEvidence = New-QemuSmokeArguments
        foreach ($ExpectedForward in @(
            @{ Name = "-ValidateFrameSignatures"; Value = "frame-signatures.json" },
            @{ Name = "-CompareFrameSignatures"; Value = "frame-signatures.golden.json" },
            @{ Name = "-ActualFrameSignatures"; Value = "frame-signatures.json" },
            @{ Name = "-ValidateDomainSummary"; Value = "domain-summary.json" },
            @{ Name = "-CompareDomainSummary"; Value = "domain-summary.golden.json" },
            @{ Name = "-ActualDomainSummary"; Value = "domain-summary.json" }
        )) {
            if (-not (Test-ArgumentValue -Arguments $ProbeEvidence -Name $ExpectedForward.Name -Expected $ExpectedForward.Value)) {
                throw "selftest_failed: wrapper did not forward $($ExpectedForward.Name)"
            }
        }
    } finally {
        $script:ValidateFrameSignatures = $OriginalValidateFrameSignatures
        $script:CompareFrameSignatures = $OriginalCompareFrameSignatures
        $script:ActualFrameSignatures = $OriginalActualFrameSignatures
        $script:ValidateDomainSummary = $OriginalValidateDomainSummary
        $script:CompareDomainSummary = $OriginalCompareDomainSummary
        $script:ActualDomainSummary = $OriginalActualDomainSummary
    }

    & powershell @Forwarded
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    Write-Host "[resident-elf-qemu-wrapper] selftest ok"
}

if ($SelfTest) {
    Invoke-WrapperSelfTest
    exit 0
}

$Arguments = New-QemuSmokeArguments
& powershell @Arguments
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
