[CmdletBinding()]
param(
    [string]$HostBuildDir = '',
    [string]$QemuBuildDir = '',
    [string]$QemuExe = 'D:\Toolchains\qemu\qemu-system-arm.exe',
    [string]$BoardLog = '',
    [switch]$SelfTest,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$requiredBoardTokens = @(
    'charm-mvp: ok',
    '[charm-capability-mvp-h747] positive=ok',
    '[charm-capability-mvp-h747] missing=missing_binding start_count=0',
    '[charm-capability-mvp-h747] ok'
)
$forbiddenBoardTokens = @(
    '[charm-capability-mvp-h747] failed',
    '[charm-capability-mvp-h747] error=',
    'fault: HardFault',
    'fault: MemManage',
    'fault: BusFault',
    'fault: UsageFault'
)

function Get-HostQemuEvidence {
    param([string]$Text)

    $evidence = [System.Collections.Generic.List[object]]::new()
    foreach ($domain in @('host', 'qemu')) {
        $prefix = [System.Text.RegularExpressions.Regex]::Escape(
            "[charm-capability-mvp-host-qemu] $domain")
        $match = [System.Text.RegularExpressions.Regex]::Match(
            $Text,
            "$prefix timestamp=([0-9]+) checksum=(0x[0-9a-fA-F]{8})")
        if (-not $match.Success) {
            throw "host_qemu_summary_invalid: ${domain}_positive_missing"
        }
        $evidence.Add([pscustomobject]@{
            Domain = $domain
            Timestamp = [uint64]::Parse($match.Groups[1].Value)
            Checksum = $match.Groups[2].Value.ToLowerInvariant()
        })
    }

    foreach ($token in @(
        '[charm-capability-mvp-host-qemu] resolution_cases=21',
        '[charm-capability-mvp-host-qemu] app_failure_cases=12',
        '[charm-capability-mvp-host-qemu] board=pending',
        '[charm-capability-mvp-host-qemu] partial=ok domains=host,qemu'
    )) {
        if (-not $Text.Contains($token)) {
            throw "host_qemu_summary_invalid: token_missing=$token"
        }
    }
    return @($evidence)
}

function Get-BoardEvidence {
    param([string]$Text)

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($token in $requiredBoardTokens) {
        if (-not $Text.Contains($token)) {
            $missing.Add($token)
        }
    }
    $forbidden = [System.Collections.Generic.List[string]]::new()
    foreach ($token in $forbiddenBoardTokens) {
        if ($Text.Contains($token)) {
            $forbidden.Add($token)
        }
    }
    if ($missing.Count -ne 0 -or $forbidden.Count -ne 0) {
        throw "board_evidence_invalid: missing=$($missing -join ',') forbidden=$($forbidden -join ',')"
    }

    $prefix = [System.Text.RegularExpressions.Regex]::Escape(
        '[charm-capability-mvp-h747]')
    $positive = [System.Text.RegularExpressions.Regex]::Match(
        $Text,
        "$prefix positive=ok timestamp=([0-9]+) checksum=(0x[0-9a-fA-F]{8})")
    if (-not $positive.Success) {
        throw 'board_evidence_invalid: positive_parse_failed'
    }
    return [pscustomobject]@{
        Domain = 'h747'
        Timestamp = [uint64]::Parse($positive.Groups[1].Value)
        Checksum = $positive.Groups[2].Value.ToLowerInvariant()
    }
}

function Assert-EquivalentEvidence {
    param([object[]]$Evidence)

    if ($Evidence.Count -ne 3) {
        throw "domain_count_mismatch: expected=3 actual=$($Evidence.Count)"
    }
    $reference = $Evidence[0]
    foreach ($item in $Evidence) {
        if ($item.Timestamp -ne $reference.Timestamp -or
            $item.Checksum -ne $reference.Checksum) {
            throw "cross_environment_mismatch: $($reference.Domain)=$($reference.Timestamp)/$($reference.Checksum) $($item.Domain)=$($item.Timestamp)/$($item.Checksum)"
        }
    }
}

function Assert-Rejected {
    param(
        [scriptblock]$Action,
        [string]$ExpectedPrefix
    )

    try {
        & $Action
    } catch {
        if (-not $_.Exception.Message.StartsWith($ExpectedPrefix)) {
            throw
        }
        return
    }
    throw "self_test_failed: expected rejection '$ExpectedPrefix'"
}

function New-SyntheticHostQemuEvidence {
    return @'
[charm-capability-mvp-host-qemu] host timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-host-qemu] qemu timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-host-qemu] resolution_cases=21
[charm-capability-mvp-host-qemu] app_failure_cases=12
[charm-capability-mvp-host-qemu] board=pending
[charm-capability-mvp-host-qemu] partial=ok domains=host,qemu
'@
}

function New-SyntheticBoardEvidence {
    return @'
charm-mvp: ok
[charm-capability-mvp-h747] positive=ok timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-h747] missing=missing_binding start_count=0
[charm-capability-mvp-h747] ok
'@
}

if ($SelfTest) {
    $hostQemuText = New-SyntheticHostQemuEvidence
    $boardText = New-SyntheticBoardEvidence
    $evidence = @(
        Get-HostQemuEvidence -Text $hostQemuText
        Get-BoardEvidence -Text $boardText
    )
    Assert-EquivalentEvidence -Evidence $evidence

    Assert-Rejected -ExpectedPrefix 'cross_environment_mismatch:' -Action {
        $mismatch = $boardText.Replace('0x49b880f0', '0x00000000')
        Assert-EquivalentEvidence -Evidence @(
            Get-HostQemuEvidence -Text $hostQemuText
            Get-BoardEvidence -Text $mismatch
        )
    }
    foreach ($token in $requiredBoardTokens) {
        Assert-Rejected -ExpectedPrefix 'board_evidence_invalid:' -Action {
            Get-BoardEvidence -Text $boardText.Replace($token, '') | Out-Null
        }
    }
    foreach ($token in $forbiddenBoardTokens) {
        Assert-Rejected -ExpectedPrefix 'board_evidence_invalid:' -Action {
            Get-BoardEvidence -Text ($boardText + "`n$token") | Out-Null
        }
    }
    Assert-Rejected -ExpectedPrefix 'host_qemu_summary_invalid:' -Action {
        Get-HostQemuEvidence -Text $hostQemuText.Replace(
            '[charm-capability-mvp-host-qemu] resolution_cases=21', '') | Out-Null
    }
    Write-Output '[charm-capability-mvp-cross-environment-self-test] ok'
    exit 0
}

$hostQemuVerifier = Join-Path $PSScriptRoot 'verify_host_qemu_evidence.ps1'
$sourceBoundaryVerifier = Join-Path $PSScriptRoot 'verify_portable_source_boundary.ps1'
if ([string]::IsNullOrWhiteSpace($BoardLog)) {
    $BoardLog = Join-Path $PSScriptRoot `
        '..\..\project\h747-lab\cmake-build-h747-lab-capability-mvp-debug\h747_lab_capability_mvp_board.log'
}
$boardLogFull = [System.IO.Path]::GetFullPath($BoardLog)

if ($DryRun) {
    Write-Output '[charm-capability-mvp-cross-environment-dry-run]'
    Write-Output "  host_qemu_verifier=$hostQemuVerifier"
    Write-Output "  source_boundary=$sourceBoundaryVerifier"
    Write-Output "  board_log=$boardLogFull"
    Write-Output '  requires_real_board=1'
    exit 0
}

# Missing real-board evidence is checked before any build work.
if (-not (Test-Path -LiteralPath $boardLogFull -PathType Leaf)) {
    throw "board_evidence_missing: $boardLogFull"
}

& $sourceBoundaryVerifier -Domains host,qemu,h747
if (-not $?) {
    throw 'source_boundary_failed'
}

$hostQemuArguments = @{
    QemuExe = $QemuExe
}
if (-not [string]::IsNullOrWhiteSpace($HostBuildDir)) {
    $hostQemuArguments['HostBuildDir'] = $HostBuildDir
}
if (-not [string]::IsNullOrWhiteSpace($QemuBuildDir)) {
    $hostQemuArguments['QemuBuildDir'] = $QemuBuildDir
}
$hostQemuText = (& $hostQemuVerifier @hostQemuArguments 2>&1 | Out-String)
if (-not $?) {
    throw "host_qemu_evidence_failed`n$hostQemuText"
}

$evidence = @(
    Get-HostQemuEvidence -Text $hostQemuText
    Get-BoardEvidence -Text (Get-Content -LiteralPath $boardLogFull -Raw -Encoding UTF8)
)
Assert-EquivalentEvidence -Evidence $evidence

foreach ($item in $evidence) {
    Write-Output "[charm-capability-mvp-cross-environment] $($item.Domain) timestamp=$($item.Timestamp) checksum=$($item.Checksum)"
}
Write-Output '[charm-capability-mvp-cross-environment] resolution_cases=21 domains=host,qemu'
Write-Output '[charm-capability-mvp-cross-environment] app_failure_cases=12 domains=host,qemu'
Write-Output '[charm-capability-mvp-cross-environment] board=minimal_positive_and_prestart_failure'
Write-Output '[charm-capability-mvp-cross-environment] ok'
