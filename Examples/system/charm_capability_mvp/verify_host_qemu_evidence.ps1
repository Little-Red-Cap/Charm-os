[CmdletBinding()]
param(
    [string]$HostBuildDir = (Join-Path $PSScriptRoot 'cmake-build-charm-capability-mvp'),
    [string]$QemuBuildDir = (Join-Path $PSScriptRoot 'qemu\cmake-build-charm-capability-mvp-qemu'),
    [string]$QemuExe = 'D:\Toolchains\qemu\qemu-system-arm.exe',
    [switch]$DryRun,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Get-DomainEvidence {
    param(
        [string]$Text,
        [ValidateSet('host', 'qemu')][string]$Domain
    )

    $prefix = [System.Text.RegularExpressions.Regex]::Escape("[charm-capability-mvp-$Domain]")
    $positive = [System.Text.RegularExpressions.Regex]::Match(
        $Text,
        "$prefix positive=ok timestamp=([0-9]+) checksum=(0x[0-9a-fA-F]{8})")
    if (-not $positive.Success) {
        throw "${Domain}_positive_evidence_missing"
    }

    foreach ($token in @(
        "[charm-capability-mvp-$Domain] missing=missing_binding start_count=0",
        "[charm-capability-mvp-$Domain] duplicate=duplicate_binding start_count=0",
        "[charm-capability-mvp-$Domain] mismatch=contract_mismatch start_count=0",
        "[charm-capability-mvp-$Domain] invalid=invalid_provision start_count=0",
        "[charm-capability-mvp-$Domain] ok"
    )) {
        if (-not $Text.Contains($token)) {
            throw "${Domain}_token_missing: $token"
        }
    }
    if ($Text.Contains("[charm-capability-mvp-$Domain] failed") -or
        $Text.Contains("[charm-capability-mvp-$Domain] error=")) {
        throw "${Domain}_failure_evidence_present"
    }

    return [pscustomobject]@{
        Domain = $Domain
        Timestamp = [uint64]::Parse($positive.Groups[1].Value)
        Checksum = $positive.Groups[2].Value.ToLowerInvariant()
    }
}

function Assert-EquivalentEvidence {
    param([object[]]$Evidence)

    if ($Evidence.Count -ne 2) {
        throw "domain_count_mismatch: expected=2 actual=$($Evidence.Count)"
    }
    if ($Evidence[0].Timestamp -ne $Evidence[1].Timestamp -or
        $Evidence[0].Checksum -ne $Evidence[1].Checksum) {
        throw "host_qemu_mismatch: host=$($Evidence[0].Timestamp)/$($Evidence[0].Checksum) qemu=$($Evidence[1].Timestamp)/$($Evidence[1].Checksum)"
    }
}

function New-SyntheticEvidence {
    param([string]$Domain)

    return @"
[charm-capability-mvp-$Domain] positive=ok timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-$Domain] missing=missing_binding start_count=0
[charm-capability-mvp-$Domain] duplicate=duplicate_binding start_count=0
[charm-capability-mvp-$Domain] mismatch=contract_mismatch start_count=0
[charm-capability-mvp-$Domain] invalid=invalid_provision start_count=0
[charm-capability-mvp-$Domain] ok
"@
}

if ($SelfTest) {
    $evidence = @(
        Get-DomainEvidence -Text (New-SyntheticEvidence -Domain 'host') -Domain 'host'
        Get-DomainEvidence -Text (New-SyntheticEvidence -Domain 'qemu') -Domain 'qemu'
    )
    Assert-EquivalentEvidence -Evidence $evidence

    $mismatch = New-SyntheticEvidence -Domain 'qemu'
    $mismatch = $mismatch.Replace('0x49b880f0', '0x00000000')
    try {
        Assert-EquivalentEvidence -Evidence @(
            $evidence[0]
            (Get-DomainEvidence -Text $mismatch -Domain 'qemu')
        )
        throw 'self_test_failed: mismatch accepted'
    } catch {
        if (-not $_.Exception.Message.StartsWith('host_qemu_mismatch:')) {
            throw
        }
    }
    Write-Output '[charm-capability-mvp-host-qemu-self-test] ok'
    exit 0
}

$hostRunner = Join-Path $PSScriptRoot 'run_host_ci.ps1'
$sourceBoundary = Join-Path $PSScriptRoot 'verify_portable_source_boundary.ps1'
$qemuRunner = Join-Path $PSScriptRoot 'qemu\run_qemu_ci.ps1'
$hostExe = Join-Path $HostBuildDir 'charm-capability-mvp-host.exe'
$qemuLog = Join-Path $QemuBuildDir 'qemu-ci.log'

if ($DryRun) {
    Write-Output '[charm-capability-mvp-host-qemu-dry-run]'
    Write-Output "  host_build=$([System.IO.Path]::GetFullPath($HostBuildDir))"
    Write-Output "  qemu_build=$([System.IO.Path]::GetFullPath($QemuBuildDir))"
    Write-Output '  board=pending'
    exit 0
}

& $sourceBoundary -Domains host,qemu
if (-not $?) {
    throw 'source_boundary_failed'
}
& $hostRunner -Profile all -BuildDir $HostBuildDir
if (-not $?) {
    throw 'host_ci_failed'
}
if (-not (Test-Path -LiteralPath $hostExe -PathType Leaf)) {
    throw "host_executable_missing: $hostExe"
}
$hostText = (& $hostExe 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "host_runtime_failed: $LASTEXITCODE`n$hostText"
}

& $qemuRunner -QemuExe $QemuExe -BuildDir $QemuBuildDir
if (-not $?) {
    throw 'qemu_runtime_failed'
}
if (-not (Test-Path -LiteralPath $qemuLog -PathType Leaf)) {
    throw "qemu_evidence_missing: $qemuLog"
}

$evidence = @(
    Get-DomainEvidence -Text $hostText -Domain 'host'
    Get-DomainEvidence -Text (Get-Content -LiteralPath $qemuLog -Raw -Encoding UTF8) -Domain 'qemu'
)
Assert-EquivalentEvidence -Evidence $evidence

foreach ($item in $evidence) {
    Write-Output "[charm-capability-mvp-host-qemu] $($item.Domain) timestamp=$($item.Timestamp) checksum=$($item.Checksum)"
}
Write-Output '[charm-capability-mvp-host-qemu] board=pending'
Write-Output '[charm-capability-mvp-host-qemu] partial=ok domains=host,qemu'
