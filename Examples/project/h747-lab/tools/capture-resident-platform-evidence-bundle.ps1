param(
    [string]$ArtifactManifest = "",
    [string]$Log = "",
    [switch]$BoardMatrix,
    [switch]$InstalledStoreMatrix,
    [string]$ControlPort = "COM16",
    [string]$UsbPort = "",
    [string[]]$Media = @("qspi", "emmc"),
    [int]$RepeatPerMedia = 1,
    [int]$WriteChunkSize = 256,
    [int]$InterChunkDelayMs = 1,
    [switch]$QemuElf,
    [switch]$DryRun,
    [switch]$SelfTest
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "charm-resident-artifacts.ps1")

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
}

function Get-DefaultBundleLog {
    $H747Root = Resolve-Path (Join-Path $PSScriptRoot "..")
    return (Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_evidence_bundle.log")
}

function Get-DefaultManifest {
    return (Get-CharmResidentDefaultManifestPath)
}

function Get-CmakeBuildDir {
    param(
        [string]$SourceDir,
        [string]$BuildName
    )
    return (Join-Path $SourceDir $BuildName)
}

function Get-MediaList {
    param([string[]]$RawMedia)

    $Seen = New-Object System.Collections.Generic.HashSet[string]
    $Result = New-Object System.Collections.Generic.List[string]
    foreach ($Item in $RawMedia) {
        foreach ($Part in ($Item -split ",")) {
            $Name = $Part.Trim().ToLowerInvariant()
            if ([string]::IsNullOrWhiteSpace($Name)) {
                continue
            }
            if ($Name -ne "qspi" -and $Name -ne "emmc") {
                throw "Unsupported media '$Name'. Expected qspi or emmc."
            }
            if ($Seen.Add($Name)) {
                [void]$Result.Add($Name)
            }
        }
    }
    if ($Result.Count -eq 0) {
        throw "At least one media must be selected."
    }
    return ,$Result.ToArray()
}

function Write-BundleLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Text
    )
    Write-Host $Text
    [void]$Lines.Add($Text)
}

function Invoke-Logged {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Label,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory = ""
    )

    $CommandLine = "$FilePath $($Arguments -join ' ')"
    Write-BundleLine -Lines $Lines -Text "== $Label =="
    Write-BundleLine -Lines $Lines -Text $CommandLine

    if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) {
        & $FilePath @Arguments 2>&1 | Tee-Object -Variable Output | Write-Host
    } else {
        Push-Location $WorkingDirectory
        try {
            & $FilePath @Arguments 2>&1 | Tee-Object -Variable Output | Write-Host
        } finally {
            Pop-Location
        }
    }

    foreach ($Line in @($Output)) {
        [void]$Lines.Add([string]$Line)
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

function Invoke-CmakeProject {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Name,
        [string]$SourceDir,
        [string]$BuildDir
    )

    Invoke-Logged -Lines $Lines -Label "$Name configure" -FilePath "cmake" -Arguments @(
        "-S", $SourceDir,
        "-B", $BuildDir
    )
    Invoke-Logged -Lines $Lines -Label "$Name build" -FilePath "cmake" -Arguments @(
        "--build", $BuildDir, "--config", "Debug"
    )
}

function Invoke-Ctest {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Name,
        [string]$BuildDir
    )

    Invoke-Logged -Lines $Lines -Label "$Name ctest" -FilePath "ctest" -Arguments @(
        "--test-dir", $BuildDir,
        "-C", "Debug",
        "--output-on-failure"
    )
}

function Get-ExecutablePath {
    param(
        [string]$BuildDir,
        [string]$ExeName
    )

    $DebugPath = Join-Path $BuildDir "Debug\$ExeName"
    if (Test-Path -LiteralPath $DebugPath) {
        return (Resolve-Path -LiteralPath $DebugPath).Path
    }
    $FlatPath = Join-Path $BuildDir $ExeName
    if (Test-Path -LiteralPath $FlatPath) {
        return (Resolve-Path -LiteralPath $FlatPath).Path
    }
    throw "missing_tool: $ExeName was not built under $BuildDir"
}

function Get-BinSize {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "missing_firmware: $Path"
    }
    return (Get-Item -LiteralPath $Path).Length
}

function Invoke-SelfTest {
    $RepoRoot = Get-RepoRoot
    $DefaultManifest = [System.IO.Path]::GetFullPath((Get-DefaultManifest))
    $DefaultLog = [System.IO.Path]::GetFullPath((Get-DefaultBundleLog))

    if (-not $DefaultManifest.EndsWith("Examples\app_abi\elf_samples\out\artifact_manifest.json", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "selftest_failed: default manifest path is unexpected: $DefaultManifest"
    }
    if (-not $DefaultLog.EndsWith("Examples\project\h747-lab\cmake-build-h747-lab-debug\resident_platform_evidence_bundle.log", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "selftest_failed: default log path is unexpected: $DefaultLog"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\app_abi\elf_samples\build_resident_platform_artifacts.ps1"))) {
        throw "selftest_failed: artifact build script is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\system\resident_platform_inspect_tool\CMakeLists.txt"))) {
        throw "missing_tool: resident_platform_inspect_tool source is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1"))) {
        throw "selftest_failed: board matrix script is missing"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot "Examples\system\run-resident-elf-qemu-smoke.ps1"))) {
        throw "selftest_failed: resident ELF QEMU smoke entry is missing"
    }
    $ParsedMedia = Get-MediaList -RawMedia @("qspi,emmc")
    if ($ParsedMedia.Count -ne 2 -or $ParsedMedia[0] -ne "qspi" -or $ParsedMedia[1] -ne "emmc") {
        throw "selftest_failed: comma media parsing failed"
    }
    try {
        Get-MediaList -RawMedia @("badmedia") | Out-Null
        throw "selftest_failed: invalid media did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("Unsupported media", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: invalid media did not report Unsupported media: $($_.Exception.Message)"
        }
    }
    try {
        Get-ExecutablePath -BuildDir (Join-Path ([System.IO.Path]::GetTempPath()) "charm_missing_tool_dir") -ExeName "missing.exe" | Out-Null
        throw "selftest_failed: missing tool did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("missing_tool", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: missing tool did not report missing_tool: $($_.Exception.Message)"
        }
    }

    try {
        Read-CharmResidentArtifactManifest -Path (Join-Path ([System.IO.Path]::GetTempPath()) "charm_missing_manifest.json") | Out-Null
        throw "selftest_failed: missing manifest did not fail"
    } catch {
        if ($_.Exception.Message.IndexOf("manifest_missing", [System.StringComparison]::Ordinal) -lt 0) {
            throw "selftest_failed: missing manifest did not report manifest_missing: $($_.Exception.Message)"
        }
    }

    Write-Host "[resident-platform-evidence-bundle] selftest ok"
}

function Write-Summary {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [object]$Manifest,
        [string]$InspectStatus,
        [object[]]$SmokeResults,
        [string]$QemuElfStatus,
        [string]$QemuElfLog,
        [string]$QemuElfDomainSummary,
        [string]$QemuElfFramePpm,
        [int64]$FirmwareSize,
        [string]$BoardMatrixLog,
        [string]$InstalledStoreMatrixLog
    )

    Write-BundleLine -Lines $Lines -Text "== summary =="
    Write-BundleLine -Lines $Lines -Text "manifest=$($Manifest.Path)"
    Write-BundleLine -Lines $Lines -Text ("store size={0} crc={1} packetstream_size={2}" -f `
        $Manifest.StoreSize, `
        (Format-CharmResidentHex32 $Manifest.StoreCrc32), `
        $Manifest.StorePacketStreamSize)
    Write-BundleLine -Lines $Lines -Text "inspect=$InspectStatus"
    foreach ($Result in $SmokeResults) {
        Write-BundleLine -Lines $Lines -Text "smoke $($Result.Name)=$($Result.Status)"
    }
    Write-BundleLine -Lines $Lines -Text "qemu_elf=$QemuElfStatus"
    if (-not [string]::IsNullOrWhiteSpace($QemuElfLog)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_log=$QemuElfLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_log=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($QemuElfDomainSummary)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain_summary=$QemuElfDomainSummary"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_domain_summary=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($QemuElfFramePpm)) {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_frame_ppm=$QemuElfFramePpm"
    } else {
        Write-BundleLine -Lines $Lines -Text "qemu_elf_frame_ppm=skipped"
    }
    Write-BundleLine -Lines $Lines -Text "h747_lab_dev_loader.bin=$FirmwareSize"
    if (-not [string]::IsNullOrWhiteSpace($BoardMatrixLog)) {
        Write-BundleLine -Lines $Lines -Text "board_matrix_log=$BoardMatrixLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "board_matrix_log=skipped"
    }
    if (-not [string]::IsNullOrWhiteSpace($InstalledStoreMatrixLog)) {
        Write-BundleLine -Lines $Lines -Text "installed_store_matrix_log=$InstalledStoreMatrixLog"
    } else {
        Write-BundleLine -Lines $Lines -Text "installed_store_matrix_log=skipped"
    }
    Write-BundleLine -Lines $Lines -Text "resident-platform-evidence-bundle: ok"
}

if ($SelfTest) {
    Invoke-SelfTest
    exit 0
}

$RepoRoot = Get-RepoRoot
$H747Root = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($ArtifactManifest)) {
    $ArtifactManifest = Get-DefaultManifest
}
if ([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Get-DefaultBundleLog
}
$MediaList = Get-MediaList -RawMedia $Media

$ArtifactManifest = [System.IO.Path]::GetFullPath($ArtifactManifest)
$Log = [System.IO.Path]::GetFullPath($Log)
$LogDir = Split-Path -Parent $Log

$InspectSource = Join-Path $RepoRoot "Examples\system\resident_platform_inspect_tool"
$InspectBuild = Get-CmakeBuildDir -SourceDir $InspectSource -BuildName "cmake-build-resident-platform-inspect-tool"
$InspectSmokeSource = Join-Path $RepoRoot "Examples\system\resident_platform_inspect_smoke"
$InspectSmokeBuild = Get-CmakeBuildDir -SourceDir $InspectSmokeSource -BuildName "cmake-build-resident-platform-inspect-smoke"
$ArtifactSmokeSource = Join-Path $RepoRoot "Examples\system\resident_platform_artifact_smoke"
$ArtifactSmokeBuild = Get-CmakeBuildDir -SourceDir $ArtifactSmokeSource -BuildName "cmake-build-resident-platform-artifact-smoke"
$PacketSmokeSource = Join-Path $RepoRoot "Examples\system\dev_loader_packet_stream_smoke"
$PacketSmokeBuild = Get-CmakeBuildDir -SourceDir $PacketSmokeSource -BuildName "cmake-build-dev-loader-packet-stream-smoke"
$StoreHandoffSource = Join-Path $RepoRoot "Examples\system\dev_loader_store_install_handoff_smoke"
$StoreHandoffBuild = Get-CmakeBuildDir -SourceDir $StoreHandoffSource -BuildName "cmake-build-dev-loader-store-install-handoff-smoke"
$ModuleXSmokeSource = Join-Path $RepoRoot "Examples\system\app_abi_modulex_smoke"
$ModuleXSmokeBuild = Get-CmakeBuildDir -SourceDir $ModuleXSmokeSource -BuildName "cmake-build-app-abi-modulex-smoke"
$QemuElfScript = Join-Path $RepoRoot "Examples\system\run-resident-elf-qemu-smoke.ps1"
$QemuElfLog = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\qemu-ci.log"
$QemuElfDomainSummary = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\domain-summary.json"
$QemuElfFramePpm = Join-Path $RepoRoot "Examples\system\resident_elf_qemu_smoke\frame-ppm"

$BoardMatrixLog = Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_board_matrix_from_bundle.log"
$InstalledStoreMatrixLog = Join-Path $H747Root "cmake-build-h747-lab-debug\resident_platform_installed_store_matrix_from_bundle.log"

if ($DryRun) {
    Write-Host "resident-platform-evidence-bundle dry-run"
    Write-Host "repo=$RepoRoot"
    Write-Host "artifact_manifest=$ArtifactManifest"
    Write-Host "log=$Log"
    Write-Host "board_matrix=$($BoardMatrix.IsPresent)"
    Write-Host "installed_store_matrix=$($InstalledStoreMatrix.IsPresent)"
    Write-Host "qemu_elf=$($QemuElf.IsPresent)"
    Write-Host "inspect_source=$InspectSource"
    Write-Host "host_smokes=resident_platform_inspect_smoke,resident_platform_artifact_smoke,dev_loader_packet_stream_smoke,dev_loader_store_install_handoff_smoke,app_abi_modulex_smoke"
    Write-Host "h747_build=build-h747-lab-dev-loader-debug"
    if ($QemuElf) {
        Write-Host "qemu_elf_script=$QemuElfScript"
        Write-Host "qemu_elf_log=$QemuElfLog"
        Write-Host "qemu_elf_domain_summary=$QemuElfDomainSummary"
        Write-Host "qemu_elf_frame_ppm=$QemuElfFramePpm"
    }
    if ($BoardMatrix) {
        Write-Host "board_matrix_log=$BoardMatrixLog"
        Write-Host "board_matrix_media=$($MediaList -join ',') repeat=$RepeatPerMedia write_chunk=$WriteChunkSize delay_ms=$InterChunkDelayMs"
    }
    if ($InstalledStoreMatrix) {
        Write-Host "installed_store_matrix_log=$InstalledStoreMatrixLog"
        Write-Host "installed_store_matrix_media=$($MediaList -join ',') repeat=$RepeatPerMedia"
    }
    exit 0
}

$Lines = New-Object System.Collections.Generic.List[string]

try {
    if (-not (Test-Path -LiteralPath $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir | Out-Null
    }

    Write-BundleLine -Lines $Lines -Text "resident-platform-evidence-bundle started $(Get-Date -Format o)"
    Write-BundleLine -Lines $Lines -Text "repo=$RepoRoot"
    Write-BundleLine -Lines $Lines -Text "artifact_manifest=$ArtifactManifest"

    Invoke-Logged -Lines $Lines -Label "build resident platform artifacts" -FilePath "powershell" -Arguments @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        (Join-Path $RepoRoot "Examples\app_abi\elf_samples\build_resident_platform_artifacts.ps1"),
        "-Validate"
    )

    $Manifest = Read-CharmResidentArtifactManifest -Path $ArtifactManifest

    Invoke-CmakeProject -Lines $Lines -Name "resident-platform-inspect" -SourceDir $InspectSource -BuildDir $InspectBuild
    $InspectExe = Get-ExecutablePath -BuildDir $InspectBuild -ExeName "resident-platform-inspect.exe"
    Invoke-Logged -Lines $Lines -Label "resident-platform-inspect" -FilePath $InspectExe -Arguments @(
        $ArtifactManifest,
        "--strict"
    )
    $InspectStatus = "ok"

    $SmokeResults = @()
    $Smokes = @(
        @{ Name = "resident_platform_inspect_smoke"; Source = $InspectSmokeSource; Build = $InspectSmokeBuild },
        @{ Name = "resident_platform_artifact_smoke"; Source = $ArtifactSmokeSource; Build = $ArtifactSmokeBuild },
        @{ Name = "dev_loader_packet_stream_smoke"; Source = $PacketSmokeSource; Build = $PacketSmokeBuild },
        @{ Name = "dev_loader_store_install_handoff_smoke"; Source = $StoreHandoffSource; Build = $StoreHandoffBuild },
        @{ Name = "app_abi_modulex_smoke"; Source = $ModuleXSmokeSource; Build = $ModuleXSmokeBuild }
    )
    foreach ($Smoke in $Smokes) {
        Invoke-CmakeProject -Lines $Lines -Name $Smoke.Name -SourceDir $Smoke.Source -BuildDir $Smoke.Build
        Invoke-Ctest -Lines $Lines -Name $Smoke.Name -BuildDir $Smoke.Build
        $SmokeResults += [pscustomobject]@{
            Name = $Smoke.Name
            Status = "pass"
        }
    }

    $QemuElfStatus = "skipped"
    $QemuElfLogResolved = ""
    $QemuElfDomainSummaryResolved = ""
    $QemuElfFramePpmResolved = ""
    if ($QemuElf) {
        Invoke-Logged -Lines $Lines -Label "resident ELF QEMU smoke selftest" -FilePath "powershell" -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $QemuElfScript,
            "-SelfTest"
        )
        Invoke-Logged -Lines $Lines -Label "resident ELF QEMU smoke" -FilePath "powershell" -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            $QemuElfScript
        )
        $QemuElfStatus = "pass"
        if (Test-Path -LiteralPath $QemuElfLog) {
            $QemuElfLogResolved = (Resolve-Path -LiteralPath $QemuElfLog).Path
        }
        if (Test-Path -LiteralPath $QemuElfDomainSummary) {
            $QemuElfDomainSummaryResolved = (Resolve-Path -LiteralPath $QemuElfDomainSummary).Path
        }
        if (Test-Path -LiteralPath $QemuElfFramePpm) {
            $QemuElfFramePpmResolved = (Resolve-Path -LiteralPath $QemuElfFramePpm).Path
        }
    }

    Invoke-Logged -Lines $Lines -Label "h747 dev_loader build-only" -FilePath "cmake" -Arguments @(
        "--build",
        "--preset",
        "build-h747-lab-dev-loader-debug",
        "--",
        "-j1"
    ) -WorkingDirectory $H747Root

    $FirmwarePath = Join-Path $H747Root "cmake-build-h747-lab-debug\h747_lab_dev_loader.bin"
    $FirmwareSize = Get-BinSize -Path $FirmwarePath

    $BoardLogResolved = ""
    if ($BoardMatrix) {
        $BoardArgs = New-Object System.Collections.Generic.List[string]
        foreach ($Argument in @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $PSScriptRoot "capture-dev-loader-usb-cdc-appstore-platform-matrix-smoke.ps1"),
            "-ArtifactManifest",
            $ArtifactManifest,
            "-ControlPort",
            $ControlPort,
            "-UsbPort",
            $UsbPort,
            "-RepeatPerMedia",
            ([string]$RepeatPerMedia),
            "-WriteChunkSize",
            ([string]$WriteChunkSize),
            "-InterChunkDelayMs",
            ([string]$InterChunkDelayMs),
            "-Media"
        )) {
            [void]$BoardArgs.Add($Argument)
        }
        foreach ($Name in $MediaList) {
            [void]$BoardArgs.Add($Name)
        }
        [void]$BoardArgs.Add("-Log")
        [void]$BoardArgs.Add($BoardMatrixLog)

        Invoke-Logged -Lines $Lines -Label "board matrix" -FilePath "powershell" -Arguments @(
            $BoardArgs.ToArray()
        )
        $BoardLogResolved = [System.IO.Path]::GetFullPath($BoardMatrixLog)
    }

    $InstalledStoreLogResolved = ""
    if ($InstalledStoreMatrix) {
        $InstalledArgs = New-Object System.Collections.Generic.List[string]
        foreach ($Argument in @(
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            (Join-Path $PSScriptRoot "capture-dev-loader-installed-store-matrix-smoke.ps1"),
            "-ArtifactManifest",
            $ArtifactManifest,
            "-ControlPort",
            $ControlPort,
            "-RepeatPerMedia",
            ([string]$RepeatPerMedia),
            "-Media"
        )) {
            [void]$InstalledArgs.Add($Argument)
        }
        foreach ($Name in $MediaList) {
            [void]$InstalledArgs.Add($Name)
        }
        [void]$InstalledArgs.Add("-Log")
        [void]$InstalledArgs.Add($InstalledStoreMatrixLog)

        Invoke-Logged -Lines $Lines -Label "installed store matrix" -FilePath "powershell" -Arguments @(
            $InstalledArgs.ToArray()
        )
        $InstalledStoreLogResolved = [System.IO.Path]::GetFullPath($InstalledStoreMatrixLog)
    }

    Write-Summary -Lines $Lines `
        -Manifest $Manifest `
        -InspectStatus $InspectStatus `
        -SmokeResults $SmokeResults `
        -QemuElfStatus $QemuElfStatus `
        -QemuElfLog $QemuElfLogResolved `
        -QemuElfDomainSummary $QemuElfDomainSummaryResolved `
        -QemuElfFramePpm $QemuElfFramePpmResolved `
        -FirmwareSize $FirmwareSize `
        -BoardMatrixLog $BoardLogResolved `
        -InstalledStoreMatrixLog $InstalledStoreLogResolved

    Set-Content -LiteralPath $Log -Encoding UTF8 -Value $Lines
    Write-Host "log=$Log"
} catch {
    [void]$Lines.Add("resident-platform-evidence-bundle: fail")
    [void]$Lines.Add($_.Exception.Message)
    if (-not (Test-Path -LiteralPath $LogDir)) {
        New-Item -ItemType Directory -Path $LogDir | Out-Null
    }
    Set-Content -LiteralPath $Log -Encoding UTF8 -Value $Lines
    Write-Error $_.Exception.Message
    exit 1
}
