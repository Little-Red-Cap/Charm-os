param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "D:\Toolchains\qemu\qemu-system-arm.exe",
    [string]$ToolchainPrefix = "arm-none-eabi-",
    [string]$HostCompiler = "D:/Toolchains/w64devkit/bin/g++.exe",
    [string]$BuildDir = "",
    [string]$AppOutDir = "",
    [string]$EvidenceDir = "",
    [string]$FrameSignatureOut = "",
    [string]$GoldenFrameSignatures = "",
    [string]$FrameDumpOut = "",
    [string]$GoldenFrameDumps = "",
    [string]$FramePpmOut = "",
    [string]$InputTraceOut = "",
    [string]$GoldenInputTrace = "",
    [string]$StorageTraceOut = "",
    [string]$GoldenStorageTrace = "",
    [string]$DomainSummaryOut = "",
    [string]$BackendContractOut = "",
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
    [string]$ValidateBackendContract = "",
    [string]$CompareDomainSummary = "",
    [string]$ActualDomainSummary = "",
    [string]$CompareFrameDumps = "",
    [string]$ActualFrameDumps = "",
    [switch]$SkipGoldenFrameSignatures,
    [switch]$SkipGoldenFrameDumps,
    [switch]$SkipGoldenInputTrace,
    [switch]$SkipGoldenStorageTrace,
    [switch]$SkipGoldenDomainSummary,
    [switch]$ValidateEvidenceBundle,
    [switch]$Doctor,
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
    Add-OptionalStringArgument -Arguments $Arguments -Name "-EvidenceDir" -Value $EvidenceDir
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FrameSignatureOut" -Value $FrameSignatureOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenFrameSignatures" -Value $GoldenFrameSignatures
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FrameDumpOut" -Value $FrameDumpOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenFrameDumps" -Value $GoldenFrameDumps
    Add-OptionalStringArgument -Arguments $Arguments -Name "-FramePpmOut" -Value $FramePpmOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-InputTraceOut" -Value $InputTraceOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenInputTrace" -Value $GoldenInputTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-StorageTraceOut" -Value $StorageTraceOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-GoldenStorageTrace" -Value $GoldenStorageTrace
    Add-OptionalStringArgument -Arguments $Arguments -Name "-DomainSummaryOut" -Value $DomainSummaryOut
    Add-OptionalStringArgument -Arguments $Arguments -Name "-BackendContractOut" -Value $BackendContractOut
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
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ValidateBackendContract" -Value $ValidateBackendContract
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareDomainSummary" -Value $CompareDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualDomainSummary" -Value $ActualDomainSummary
    Add-OptionalStringArgument -Arguments $Arguments -Name "-CompareFrameDumps" -Value $CompareFrameDumps
    Add-OptionalStringArgument -Arguments $Arguments -Name "-ActualFrameDumps" -Value $ActualFrameDumps
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenFrameSignatures" -Enabled $SkipGoldenFrameSignatures.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenFrameDumps" -Enabled $SkipGoldenFrameDumps.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenInputTrace" -Enabled $SkipGoldenInputTrace.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenStorageTrace" -Enabled $SkipGoldenStorageTrace.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-SkipGoldenDomainSummary" -Enabled $SkipGoldenDomainSummary.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-ValidateEvidenceBundle" -Enabled $ValidateEvidenceBundle.IsPresent
    Add-OptionalSwitchArgument -Arguments $Arguments -Name "-Doctor" -Enabled $Doctor.IsPresent
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
    $SystemReadme = Join-Path $PSScriptRoot "README.md"
    if (-not (Test-Path -LiteralPath $SystemReadme)) {
        throw "selftest_failed: system README is missing: $SystemReadme"
    }
    $SystemReadmeText = Get-Content -LiteralPath $SystemReadme -Raw -Encoding UTF8
    foreach ($RequiredReadmeToken in @(
        "run-resident-elf-qemu-smoke.ps1 -Doctor",
        "run-resident-elf-qemu-smoke.ps1 -ValidateEvidenceBundle",
        "capture-resident-platform-evidence-bundle.ps1 -QemuElf -QemuElfValidateOnly -SkipH747Build",
        "qemu_elf_backend_scope=",
        "qemu_elf_doctor_scope=",
        "qemu_elf_scope_match=1",
        "qemu_elf_runtime_domain_profile=",
        "qemu_elf_gui_contract=",
        "qemu_elf_storage_contract=",
        "qemu_elf_failure_transport=",
        "qemu_elf_failure_stage=",
        "qemu_elf_failure_load=",
        "qemu_elf_failure_runtime=",
        "scope_match=required",
        "H747 USB CDC",
        "QSPI",
        "eMMC"
    )) {
        if (-not $SystemReadmeText.Contains($RequiredReadmeToken)) {
            throw "selftest_failed: system README does not expose QEMU ELF route token $RequiredReadmeToken"
        }
    }
    $EvidenceBundleText = Get-Content -LiteralPath $EvidenceBundle -Raw -Encoding UTF8
    foreach ($RequiredBundleToken in @(
        "[switch]`$QemuElf",
        "[switch]`$QemuElfValidateOnly",
        "[int]`$QemuElfTimeoutSec",
        "[int]`$QemuElfTailLines",
        "[string]`$QemuElfEvidenceDir",
        "qemu_elf_timeout_sec=",
        "qemu_elf_tail_lines=",
        "qemu_elf_doctor_scope=",
        "qemu_elf_evidence_dir=",
        "qemu_elf_backend_scope=",
        "qemu_elf_runtime_domain_profile=",
        "qemu_elf_failure_taxonomy=",
        "qemu_elf_failure_transport=",
        "qemu_elf_failure_stage=",
        "qemu_elf_failure_load=",
        "qemu_elf_failure_runtime=",
        "qemu_elf_gui_contract=",
        "qemu_elf_storage_contract=",
        '"-EvidenceDir"',
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
        @{ Name = "-TimeoutSec"; Value = ([string]$TimeoutSec) },
        @{ Name = "-TailLines"; Value = ([string]$TailLines) },
        @{ Name = "-ElfBase"; Value = $ElfBase }
    )) {
        if (-not (Test-ArgumentValue -Arguments $Forwarded -Name $ExpectedForward.Name -Expected $ExpectedForward.Value)) {
            throw "selftest_failed: wrapper did not forward $($ExpectedForward.Name)=$($ExpectedForward.Value)"
        }
    }
    if ($SkipGoldenFrameSignatures -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenFrameSignatures")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenFrameSignatures"
    }
    if ($SkipGoldenFrameDumps -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-SkipGoldenFrameDumps")) {
        throw "selftest_failed: wrapper did not forward -SkipGoldenFrameDumps"
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
    if ($Doctor -and -not (Test-ArgumentPresent -Arguments $Forwarded -Name "-Doctor")) {
        throw "selftest_failed: wrapper did not forward -Doctor"
    }

    $OriginalBuildDir = $BuildDir
    $OriginalAppOutDir = $AppOutDir
    $OriginalEvidenceDir = $EvidenceDir
    $OriginalFrameSignatureOut = $FrameSignatureOut
    $OriginalGoldenFrameSignatures = $GoldenFrameSignatures
    $OriginalFrameDumpOut = $FrameDumpOut
    $OriginalGoldenFrameDumps = $GoldenFrameDumps
    $OriginalFramePpmOut = $FramePpmOut
    $OriginalInputTraceOut = $InputTraceOut
    $OriginalGoldenInputTrace = $GoldenInputTrace
    $OriginalStorageTraceOut = $StorageTraceOut
    $OriginalGoldenStorageTrace = $GoldenStorageTrace
    $OriginalDomainSummaryOut = $DomainSummaryOut
    $OriginalBackendContractOut = $BackendContractOut
    $OriginalGoldenDomainSummary = $GoldenDomainSummary
    $OriginalValidateLog = $ValidateLog
    $OriginalValidateFrameSignatures = $ValidateFrameSignatures
    $OriginalCompareFrameSignatures = $CompareFrameSignatures
    $OriginalActualFrameSignatures = $ActualFrameSignatures
    $OriginalValidateFrameDumps = $ValidateFrameDumps
    $OriginalValidateFramePpm = $ValidateFramePpm
    $OriginalValidateInputTrace = $ValidateInputTrace
    $OriginalCompareInputTrace = $CompareInputTrace
    $OriginalActualInputTrace = $ActualInputTrace
    $OriginalValidateStorageTrace = $ValidateStorageTrace
    $OriginalCompareStorageTrace = $CompareStorageTrace
    $OriginalActualStorageTrace = $ActualStorageTrace
    $OriginalCompareFrameDumps = $CompareFrameDumps
    $OriginalActualFrameDumps = $ActualFrameDumps
    $OriginalValidateDomainSummary = $ValidateDomainSummary
    $OriginalValidateBackendContract = $ValidateBackendContract
    $OriginalCompareDomainSummary = $CompareDomainSummary
    $OriginalActualDomainSummary = $ActualDomainSummary
    $OriginalValidateEvidenceBundle = $ValidateEvidenceBundle
    $OriginalDoctor = $Doctor
    $OriginalDryRun = $DryRun
    $script:BuildDir = "selftest-build"
    $script:AppOutDir = "selftest-apps"
    $script:EvidenceDir = "selftest-evidence"
    $script:FrameSignatureOut = "frame-signatures.out.json"
    $script:GoldenFrameSignatures = "frame-signatures.golden.json"
    $script:FrameDumpOut = "frame-dumps.out.json"
    $script:GoldenFrameDumps = "frame-dumps.golden.json"
    $script:FramePpmOut = "frame-ppm-out"
    $script:InputTraceOut = "input-trace.out.json"
    $script:GoldenInputTrace = "input-trace.golden.json"
    $script:StorageTraceOut = "storage-trace.out.json"
    $script:GoldenStorageTrace = "storage-trace.golden.json"
    $script:DomainSummaryOut = "domain-summary.out.json"
    $script:BackendContractOut = "backend-contract.out.json"
    $script:GoldenDomainSummary = "domain-summary.golden.json"
    $script:ValidateLog = "qemu-ci.log"
    $script:ValidateFrameSignatures = "frame-signatures.json"
    $script:CompareFrameSignatures = "frame-signatures.golden.json"
    $script:ActualFrameSignatures = "frame-signatures.json"
    $script:ValidateFrameDumps = "frame-dumps.json"
    $script:ValidateFramePpm = "frame-ppm"
    $script:ValidateInputTrace = "input-trace.json"
    $script:CompareInputTrace = "input-trace.golden.json"
    $script:ActualInputTrace = "input-trace.json"
    $script:ValidateStorageTrace = "storage-trace.json"
    $script:CompareStorageTrace = "storage-trace.golden.json"
    $script:ActualStorageTrace = "storage-trace.json"
    $script:CompareFrameDumps = "frame-dumps.golden.json"
    $script:ActualFrameDumps = "frame-dumps.json"
    $script:ValidateDomainSummary = "domain-summary.json"
    $script:ValidateBackendContract = "backend-contract.json"
    $script:CompareDomainSummary = "domain-summary.golden.json"
    $script:ActualDomainSummary = "domain-summary.json"
    $script:ValidateEvidenceBundle = [System.Management.Automation.SwitchParameter]::Present
    $script:DryRun = $true
    try {
        $ProbeEvidence = New-QemuSmokeArguments
        foreach ($ExpectedForward in @(
            @{ Name = "-BuildDir"; Value = "selftest-build" },
            @{ Name = "-AppOutDir"; Value = "selftest-apps" },
            @{ Name = "-EvidenceDir"; Value = "selftest-evidence" },
            @{ Name = "-FrameSignatureOut"; Value = "frame-signatures.out.json" },
            @{ Name = "-GoldenFrameSignatures"; Value = "frame-signatures.golden.json" },
            @{ Name = "-FrameDumpOut"; Value = "frame-dumps.out.json" },
            @{ Name = "-GoldenFrameDumps"; Value = "frame-dumps.golden.json" },
            @{ Name = "-FramePpmOut"; Value = "frame-ppm-out" },
            @{ Name = "-InputTraceOut"; Value = "input-trace.out.json" },
            @{ Name = "-GoldenInputTrace"; Value = "input-trace.golden.json" },
            @{ Name = "-StorageTraceOut"; Value = "storage-trace.out.json" },
            @{ Name = "-GoldenStorageTrace"; Value = "storage-trace.golden.json" },
            @{ Name = "-DomainSummaryOut"; Value = "domain-summary.out.json" },
            @{ Name = "-BackendContractOut"; Value = "backend-contract.out.json" },
            @{ Name = "-GoldenDomainSummary"; Value = "domain-summary.golden.json" },
            @{ Name = "-ValidateLog"; Value = "qemu-ci.log" },
            @{ Name = "-ValidateFrameSignatures"; Value = "frame-signatures.json" },
            @{ Name = "-CompareFrameSignatures"; Value = "frame-signatures.golden.json" },
            @{ Name = "-ActualFrameSignatures"; Value = "frame-signatures.json" },
            @{ Name = "-ValidateFrameDumps"; Value = "frame-dumps.json" },
            @{ Name = "-ValidateFramePpm"; Value = "frame-ppm" },
            @{ Name = "-ValidateInputTrace"; Value = "input-trace.json" },
            @{ Name = "-CompareInputTrace"; Value = "input-trace.golden.json" },
            @{ Name = "-ActualInputTrace"; Value = "input-trace.json" },
            @{ Name = "-ValidateStorageTrace"; Value = "storage-trace.json" },
            @{ Name = "-CompareStorageTrace"; Value = "storage-trace.golden.json" },
            @{ Name = "-ActualStorageTrace"; Value = "storage-trace.json" },
            @{ Name = "-CompareFrameDumps"; Value = "frame-dumps.golden.json" },
            @{ Name = "-ActualFrameDumps"; Value = "frame-dumps.json" },
            @{ Name = "-ValidateDomainSummary"; Value = "domain-summary.json" },
            @{ Name = "-ValidateBackendContract"; Value = "backend-contract.json" },
            @{ Name = "-CompareDomainSummary"; Value = "domain-summary.golden.json" },
            @{ Name = "-ActualDomainSummary"; Value = "domain-summary.json" }
        )) {
            if (-not (Test-ArgumentValue -Arguments $ProbeEvidence -Name $ExpectedForward.Name -Expected $ExpectedForward.Value)) {
                throw "selftest_failed: wrapper did not forward $($ExpectedForward.Name)"
            }
        }
        if (-not (Test-ArgumentPresent -Arguments $ProbeEvidence -Name "-DryRun")) {
            throw "selftest_failed: wrapper did not forward -DryRun"
        }
        if (-not (Test-ArgumentPresent -Arguments $ProbeEvidence -Name "-ValidateEvidenceBundle")) {
            throw "selftest_failed: wrapper did not forward -ValidateEvidenceBundle"
        }
        $script:Doctor = [System.Management.Automation.SwitchParameter]::Present
        $ProbeDoctor = New-QemuSmokeArguments
        if (-not (Test-ArgumentPresent -Arguments $ProbeDoctor -Name "-Doctor")) {
            throw "selftest_failed: wrapper did not forward -Doctor"
        }
    } finally {
        $script:BuildDir = $OriginalBuildDir
        $script:AppOutDir = $OriginalAppOutDir
        $script:EvidenceDir = $OriginalEvidenceDir
        $script:FrameSignatureOut = $OriginalFrameSignatureOut
        $script:GoldenFrameSignatures = $OriginalGoldenFrameSignatures
        $script:FrameDumpOut = $OriginalFrameDumpOut
        $script:GoldenFrameDumps = $OriginalGoldenFrameDumps
        $script:FramePpmOut = $OriginalFramePpmOut
        $script:InputTraceOut = $OriginalInputTraceOut
        $script:GoldenInputTrace = $OriginalGoldenInputTrace
        $script:StorageTraceOut = $OriginalStorageTraceOut
        $script:GoldenStorageTrace = $OriginalGoldenStorageTrace
        $script:DomainSummaryOut = $OriginalDomainSummaryOut
        $script:BackendContractOut = $OriginalBackendContractOut
        $script:GoldenDomainSummary = $OriginalGoldenDomainSummary
        $script:ValidateLog = $OriginalValidateLog
        $script:ValidateFrameSignatures = $OriginalValidateFrameSignatures
        $script:CompareFrameSignatures = $OriginalCompareFrameSignatures
        $script:ActualFrameSignatures = $OriginalActualFrameSignatures
        $script:ValidateFrameDumps = $OriginalValidateFrameDumps
        $script:ValidateFramePpm = $OriginalValidateFramePpm
        $script:ValidateInputTrace = $OriginalValidateInputTrace
        $script:CompareInputTrace = $OriginalCompareInputTrace
        $script:ActualInputTrace = $OriginalActualInputTrace
        $script:ValidateStorageTrace = $OriginalValidateStorageTrace
        $script:CompareStorageTrace = $OriginalCompareStorageTrace
        $script:ActualStorageTrace = $OriginalActualStorageTrace
        $script:CompareFrameDumps = $OriginalCompareFrameDumps
        $script:ActualFrameDumps = $OriginalActualFrameDumps
        $script:ValidateDomainSummary = $OriginalValidateDomainSummary
        $script:ValidateBackendContract = $OriginalValidateBackendContract
        $script:CompareDomainSummary = $OriginalCompareDomainSummary
        $script:ActualDomainSummary = $OriginalActualDomainSummary
        $script:ValidateEvidenceBundle = $OriginalValidateEvidenceBundle
        $script:Doctor = $OriginalDoctor
        $script:DryRun = $OriginalDryRun
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
